/*
** $Id: ltable.c,v 2.32.1.2 2007/12/28 15:32:23 roberto Exp $
** Lua tables (hash)
** See Copyright Notice in lua.h
*/


/*
** Implementation of tables (aka arrays, objects, or hash tables).
** Tables keep its elements in two parts: an array part and a hash part.
** Non-negative integer keys are all candidates to be kept in the array
** part. The actual size of the array is the largest `n' such that at
** least half the slots between 0 and n are in use.
** Hash uses a mix of chained scatter table with Brent's variation.
** A main invariant of these tables is that, if an element is not
** in its main position (i.e. the `original' position that its hash gives
** to it), then the colliding element is in its own main position.
** Hence even when the load factor reaches 100%, performance remains good.
*/

#include <math.h>
#include <string.h>

#define ltable_c
#define LUA_CORE

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "ltable.h"


/*
** max size of array part is 2^MAXBITS
*/
#if LUAI_BITSINT > 26
#define MAXBITS		26
#else
#define MAXBITS		(LUAI_BITSINT-2)
#endif

#define MAXASIZE	(1 << MAXBITS)


#define hashpow2(t,n)      (gnode(t, lmod((n), sizenode(t))))
  
#define hashstr(t,str)  hashpow2(t, (str)->tsv.hash)
#define hashboolean(t,p)        hashpow2(t, p)


/*
** for some types, it is better to avoid modulus by power of 2, as
** they tend to have many 2 factors.
*/// 根据hash值对hash部分大小(或1的作用保证不会对0取模)获得hash部分对应位置的地址
#define hashmod(t,n)	(gnode(t, ((n) % ((sizenode(t)-1)|1))))


#define hashpointer(t,p)	hashmod(t, IntPoint(p))


/*
** number of ints inside a lua_Number
*/
#define numints		cast_int(sizeof(lua_Number)/sizeof(int))



#define dummynode		(&dummynode_)

static const Node dummynode_ = {
  {{NULL}, LUA_TNIL},  /* value */
  {{{NULL}, LUA_TNIL, NULL}}  /* key */
};


/*
** hash for lua_Numbers
*/// 根据数字获得hash部分对应的地址 hash方式是将数字的各个字对应的数值相加结果再取模
static Node *hashnum (const Table *t, lua_Number n) {
  unsigned int a[numints];
  int i;
  if (luai_numeq(n, 0))  /* avoid problems with -0 */
    return gnode(t, 0);
  memcpy(a, &n, sizeof(a));
  for (i = 1; i < numints; i++) a[0] += a[i];// 各字和
  return hashmod(t, a[0]);
}



/*
** returns the `main' position of an element in a table (that is, the index
** of its hash value)
*/// 返回主位置的元素 主位置是其hash值对应的第一个位置
static Node *mainposition (const Table *t, const TValue *key) {
  switch (ttype(key)) {
    case LUA_TNUMBER:
      return hashnum(t, nvalue(key));
    case LUA_TSTRING:
      return hashstr(t, rawtsvalue(key));
    case LUA_TBOOLEAN:
      return hashboolean(t, bvalue(key));
    case LUA_TLIGHTUSERDATA:
      return hashpointer(t, pvalue(key));
    default:
      return hashpointer(t, gcvalue(key));
  }
}


/*
** returns the index for `key' if `key' is an appropriate key to live in
** the array part of the table, -1 otherwise.
*/ // 如果key可作为数组部分的键，则返回该键代表的索引值
static int arrayindex (const TValue *key) {
  if (ttisnumber(key)) {
    lua_Number n = nvalue(key);
    int k;
    lua_number2int(k, n);// k = n
    if (luai_numeq(cast_num(k), n))
      return k;
  }
  return -1;  /* `key' did not match some condition */
}


/*
** returns the index of a `key' for table traversals. First goes all
** elements in the array part, then elements in the hash part. The
** beginning of a traversal is signalled by -1.
*/	// 返回key在table中的下标(首先从数组部分查找，再从hash部分查找)
static int findindex (lua_State *L, Table *t, StkId key) {
  int i;
  if (ttisnil(key)) return -1;  /* first iteration */
  i = arrayindex(key);
  if (0 < i && i <= t->sizearray)  /* is `key' inside array part? */
    return i-1;  /* yes; that's the index (corrected to C) */
  else {
    Node *n = mainposition(t, key);// 从主位开始找
    do {  /* check whether `key' is somewhere in the chain */
      /* key may be dead already, but it is ok to use it in `next' */
      if (luaO_rawequalObj(key2tval(n), key) ||	// key在当前位置
            (ttype(gkey(n)) == LUA_TDEADKEY && iscollectable(key) && // 可被回收
             gcvalue(gkey(n)) == gcvalue(key))) {// gc数据也相同
        i = cast_int(n - gnode(t, 0));  /* key index in hash table */
        /* hash elements are numbered after array ones */
        return i + t->sizearray;
      }
      else n = gnext(n);// 迭代下一个hash值相同的key
    } while (n);
    luaG_runerror(L, "invalid key to " LUA_QL("next"));  /* key not found */
    return 0;  /* to avoid warnings */
  }
}

// 取下一个元素 先从数组部分找，没有找到再从hash部分找
int luaH_next (lua_State *L, Table *t, StkId key) {
  int i = findindex(L, t, key);  /* find original element */
  for (i++; i < t->sizearray; i++) {  /* try first array part */
    if (!ttisnil(&t->array[i])) {  /* a non-nil value? */
      setnvalue(key, cast_num(i+1));
      setobj2s(L, key+1, &t->array[i]);
      return 1;
    }
  }
  for (i -= t->sizearray; i < sizenode(t); i++) {  /* then hash part */
    if (!ttisnil(gval(gnode(t, i)))) {  /* a non-nil value? */
      setobj2s(L, key, key2tval(gnode(t, i)));
      setobj2s(L, key+1, gval(gnode(t, i)));
      return 1;
    }
  }
  return 0;  /* no more elements */
}


/*
** {=============================================================
** Rehash
** ==============================================================
*/

// 计算table数组部分的恰当大小；数组部分不要求连续，允许存在空位，但空位个数必须小于元素个数，数组大小必定为2的幂
static int computesizes (int nums[], int *narray) {
  int i;
  int twotoi;  /* 2^i */ // 当前预期的数组部分大小
  int a = 0;  /* number of elements smaller than 2^i */
  int na = 0;  /* number of elements to go to array part */// 数组部分元素个数
  int n = 0;  /* optimal size for array part */// 数组部分的理想大小
  for (i = 0, twotoi = 1; twotoi/2 < *narray; i++, twotoi *= 2) {
    if (nums[i] > 0) {
      a += nums[i];// 当前统计的数组元素个数
      if (a > twotoi/2) {  /* more than half elements present? */// 当前所有元素个数必须超过数组大小的一半
        n = twotoi;  /* optimal size (till now) */ // 更新最佳大小
        na = a;  /* all elements smaller than n will go to array part */// 数组部分元素的个数
      }
    }
    if (a == *narray) break;  /* all elements already counted */
  }
  *narray = n;// 调整后的数组大小
  lua_assert(*narray/2 <= na && na <= *narray);
  return na;
}

// 如果key可作为数组的键值，则计数加一
static int countint (const TValue *key, int *nums) {
  int k = arrayindex(key);
  if (0 < k && k <= MAXASIZE) {  /* is `key' an appropriate array index? */
    nums[ceillog2(k)]++;  /* count as such */
    return 1;
  }
  else
    return 0;
}

// 计算数组部分元素个数
static int numusearray (const Table *t, int *nums) {
  int lg;
  int ttlg;  /* 2^lg */// 因为每次数组大小的增长是按照容量翻倍进行的，所以统计时也按照同样的方式分段
  int ause = 0;  /* summation of `nums' */// 所有元素个数
  int i = 1;  /* count to traverse all array keys */
  for (lg=0, ttlg=1; lg<=MAXBITS; lg++, ttlg*=2) {  /* for each slice */// 每次统计区间范围翻倍
    int lc = 0;  /* counter */
    int lim = ttlg;
    if (lim > t->sizearray) {
      lim = t->sizearray;  /* adjust upper limit */// 区间元素个数的上限
      if (i > lim)	// 所有元素都已经统计完了
        break;  /* no more elements to count */
    }
    /* count elements in range (2^(lg-1), 2^lg] */
    for (; i <= lim; i++) {
      if (!ttisnil(&t->array[i-1]))
        lc++;
    }
    nums[lg] += lc;// 分段计数
    ause += lc;
  }
  return ause;
}

// hash部分元素个数
static int numusehash (const Table *t, int *nums, int *pnasize) {
  int totaluse = 0;  /* total number of elements */
  int ause = 0;  /* summation of `nums' */
  int i = sizenode(t);// hash部分大小
  while (i--) {
    Node *n = &t->node[i];
    if (!ttisnil(gval(n))) {
      ause += countint(key2tval(n), nums);
      totaluse++;
    }
  }
  *pnasize += ause;
  return totaluse;
}

// 扩展table的数组部分，并将扩展的部分全部填充nil
static void setarrayvector (lua_State *L, Table *t, int size) {
  int i;
  luaM_reallocvector(L, t->array, t->sizearray, size, TValue);
  for (i=t->sizearray; i<size; i++)
     setnilvalue(&t->array[i]);
  t->sizearray = size;
}

// 重新分配table的hash部分，使其大小为可以容纳size个元素且为最小的2次幂，将其全部填充为nil
static void setnodevector (lua_State *L, Table *t, int size) {
  int lsize;
  if (size == 0) {  /* no elements to hash part? */ // 没有hash部分
    t->node = cast(Node *, dummynode);  /* use common `dummynode' */
    lsize = 0;
  }
  else {
    int i;
    lsize = ceillog2(size);
    if (lsize > MAXBITS)
      luaG_runerror(L, "table overflow");
    size = twoto(lsize);	// 先求对数值再求幂，新的大小足以容纳所有元素且为2的幂
    t->node = luaM_newvector(L, size, Node);
    for (i=0; i<size; i++) {
      Node *n = gnode(t, i);// 取出所有元素并设置为nil
      gnext(n) = NULL;
      setnilvalue(gkey(n));
      setnilvalue(gval(n));
    }
  }
  t->lsizenode = cast_byte(lsize);
  t->lastfree = gnode(t, size);  /* all positions are free */// 所有位置都是空的
}


static void resize (lua_State *L, Table *t, int nasize, int nhsize) {
  int i;
  int oldasize = t->sizearray;
  int oldhsize = t->lsizenode;
  Node *nold = t->node;  /* save old hash ... */ // 保存旧的hash部分
  if (nasize > oldasize)  /* array part must grow? */
    setarrayvector(L, t, nasize);// 扩展数组部分空间，并将扩展的部分填充nil
  /* create new hash part with appropriate size */
  setnodevector(L, t, nhsize);  // 根据hash部分元素个数生成新的hash部分，大小也是2的幂
  if (nasize < oldasize) {  /* array part must shrink? */
    t->sizearray = nasize;
    /* re-insert elements from vanishing slice */
    for (i=nasize; i<oldasize; i++) {
      if (!ttisnil(&t->array[i]))// 将需要从数组部分移出的项拷贝到hash部分
        setobjt2t(L, luaH_setnum(L, t, i+1), &t->array[i]);
    }
    /* shrink array */// 收缩数组部分
    luaM_reallocvector(L, t->array, oldasize, nasize, TValue);
  }
  /* re-insert elements from hash part */
  for (i = twoto(oldhsize) - 1; i >= 0; i--) {
    Node *old = nold+i;
    if (!ttisnil(gval(old)))// 将旧的hash部分的元素插回table中，包括数组部分和hash部分
      setobjt2t(L, luaH_set(L, t, key2tval(old)), gval(old));
  }				 // 取出对应key的Node，没有key就创建一个
  if (nold != dummynode)	// 回收旧的hash部分
    luaM_freearray(L, nold, twoto(oldhsize), Node);  /* free old array */
}


void luaH_resizearray (lua_State *L, Table *t, int nasize) {
  int nsize = (t->node == dummynode) ? 0 : sizenode(t);
  resize(L, t, nasize, nsize);
}

// rehash非常耗时，lua为提高效率，尽量少进行rehash；如对一个只有hash部分的table插入key为1的项，该项被暂时保存在hash部分，计算数组大小时再将其插入到数组部分
static void rehash (lua_State *L, Table *t, const TValue *ek) {
  int nasize, na;
  int nums[MAXBITS+1];  /* nums[i] = number of keys between 2^(i-1) and 2^i */
  int i;
  int totaluse;
  for (i=0; i<=MAXBITS; i++) nums[i] = 0;  /* reset counts */
  nasize = numusearray(t, nums);  /* count keys in array part */// 当前数组部分元素个数
  totaluse = nasize;  /* all those keys are integer keys */
  totaluse += numusehash(t, nums, &nasize);  /* count keys in hash part */// 当前hash部分元素个数，会顺便将可作为数组部分的元素统计到数组部分的个数中
  /* count extra key */
  nasize += countint(ek, nums);// 统计的是key为数字的项数
  totaluse++;
  /* compute new size for array part */
  na = computesizes(nums, &nasize);	// na:数组部分元素个数 nasize:数组部分大小
  /* resize the table to new computed sizes */
  resize(L, t, nasize, totaluse - na);
}



/*
** }=============================================================
*/

// 创建一个table对象
Table *luaH_new (lua_State *L, int narray, int nhash) {
  Table *t = luaM_new(L, Table);
  luaC_link(L, obj2gco(t), LUA_TTABLE);// gc相关，将该table插入全局状态机gc链表的开头
  t->metatable = NULL;
  t->flags = cast_byte(~0);
  /* temporary values (kept only if some malloc fails) */
  t->array = NULL;
  t->sizearray = 0;
  t->lsizenode = 0;
  t->node = cast(Node *, dummynode);
  setarrayvector(L, t, narray);
  setnodevector(L, t, nhash);
  return t;
}

// 释放table空间
void luaH_free (lua_State *L, Table *t) {
  if (t->node != dummynode)// 释放hash部分
    luaM_freearray(L, t->node, sizenode(t), Node);
  luaM_freearray(L, t->array, t->sizearray, TValue);// 释放数组部分
  luaM_free(L, t);
}

// 去最后一个空位
static Node *getfreepos (Table *t) {
  while (t->lastfree-- > t->node) {
    if (ttisnil(gkey(t->lastfree)))
      return t->lastfree;
  }
  return NULL;  /* could not find a free place */
}



/*
** inserts a new key into a hash table; first, check whether key's main 
** position is free. If not, check whether colliding node is in its main 
** position or not: if it is not, move colliding node to an empty place and 
** put new key in its main position; otherwise (colliding node is in its main 
** position), new key goes to an empty position. 
*/ /* 插入一个新的key到hash表中;首先会判断当前key的主位是否是空闲的。如果不是，
判断冲突的元素是否在其主位上(两者hash值相同):若不是，将冲突元素移到一个空位上，
再将当前key放到主位上；若是，则将当前key放到空位上。返回新建key对应value的地址 */
static TValue *newkey (lua_State *L, Table *t, const TValue *key) {
  Node *mp = mainposition(t, key);
  if (!ttisnil(gval(mp)) || mp == dummynode) {
    Node *othern;
    Node *n = getfreepos(t);  /* get a free place */
    if (n == NULL) {  /* cannot find a free place? */// 找不到空位
      rehash(L, t, key);  /* grow table */ // 调整table大小
      return luaH_set(L, t, key);  /* re-insert key into grown table */
    }
    lua_assert(n != dummynode);
    othern = mainposition(t, key2tval(mp));// 冲突元素的主位
    if (othern != mp) {  /* is colliding node out of its main position? */// 冲突元素不在其主位
      /* yes; move colliding node into free position */// 将冲突元素移到空位上；如果冲突元素不在主位，说明是在主位产生同hash冲突后放到空闲位置上的，从其主位开始一定可以链到该冲突元素
      while (gnext(othern) != mp) othern = gnext(othern);  /* find previous */// 找到冲突元素的前驱
      gnext(othern) = n;  /* redo the chain with `n' in place of `mp' */// 将空位链到冲突元素的前驱后
      *n = *mp;  /* copy colliding node into free pos. (mp->next also goes) */// 拷贝冲突元素到空位上
      gnext(mp) = NULL;  /* now `mp' is free */// 释放冲突元素
      setnilvalue(gval(mp));
    }
    else {  /* colliding node is in its own main position */// 冲突元素在主位，两者有相同的hash值
      /* new node will go into free position */
      gnext(n) = gnext(mp);  /* chain new position */// 链到主位之后
      gnext(mp) = n;
      mp = n;
    }
  }
  gkey(mp)->value = key->value; gkey(mp)->tt = key->tt;// key拷贝
  luaC_barriert(L, t, key);	// gc相关
  lua_assert(ttisnil(gval(mp)));
  return gval(mp);
}


/*
** search function for integers
*/// 返回table中键值为数字类型key的元素的value部分地址
const TValue *luaH_getnum (Table *t, int key) {
  /* (1 <= key && key <= t->sizearray) */
  if (cast(unsigned int, key-1) < cast(unsigned int, t->sizearray))
    return &t->array[key-1];// 数组部分
  else {
    lua_Number nk = cast_num(key);
    Node *n = hashnum(t, nk);// 取hash部分hash值对应的主位元素
    do {  /* check whether `key' is somewhere in the chain */// 在链表中迭代寻找该key对应的元素
      if (ttisnumber(gkey(n)) && luai_numeq(nvalue(gkey(n)), nk))
        return gval(n);  /* that's it */
      else n = gnext(n);
    } while (n);
    return luaO_nilobject;
  }
}


/*
** search function for strings
*/// 返回table中键值为字符串类型key的元素的value部分地址
const TValue *luaH_getstr (Table *t, TString *key) {
  Node *n = hashstr(t, key);// 根据hash值取主位元素
  do {  /* check whether `key' is somewhere in the chain */ // 迭代寻找该key对应的元素
    if (ttisstring(gkey(n)) && rawtsvalue(gkey(n)) == key)
      return gval(n);  /* that's it */
    else n = gnext(n);
  } while (n);
  return luaO_nilobject;
}


/*
** main search function
*/ // 返回table中键值为key的元素的value部分地址
const TValue *luaH_get (Table *t, const TValue *key) {
  switch (ttype(key)) {
    case LUA_TNIL: return luaO_nilobject;
    case LUA_TSTRING: return luaH_getstr(t, rawtsvalue(key));
    case LUA_TNUMBER: {
      int k;
      lua_Number n = nvalue(key);
      lua_number2int(k, n);
      if (luai_numeq(cast_num(k), nvalue(key))) /* index is int? */
        return luaH_getnum(t, k);  /* use specialized version */
      /* else go through */
    }
    default: {
      Node *n = mainposition(t, key); // 根据hash值取主位元素
      do {  /* check whether `key' is somewhere in the chain */
        if (luaO_rawequalObj(key2tval(n), key))// 迭代寻找该key对应的元素
          return gval(n);  /* that's it */
        else n = gnext(n);
      } while (n);
      return luaO_nilobject;
    }
  }
}

// 取对key对应元素的value部分地址，如果元素不存在，则新建一个
TValue *luaH_set (lua_State *L, Table *t, const TValue *key) {
  const TValue *p = luaH_get(t, key);
  t->flags = 0;
  if (p != luaO_nilobject)
    return cast(TValue *, p);
  else {
    if (ttisnil(key)) luaG_runerror(L, "table index is nil");
    else if (ttisnumber(key) && luai_numisnan(nvalue(key)))
      luaG_runerror(L, "table index is NaN");
    return newkey(L, t, key);
  }
}

// 取对类型为(整型)数字的key对应元素的value部分地址，如果元素不存在，则新建一个
TValue *luaH_setnum (lua_State *L, Table *t, int key) {
  const TValue *p = luaH_getnum(t, key);
  if (p != luaO_nilobject)
    return cast(TValue *, p);
  else {
    TValue k;
    setnvalue(&k, cast_num(key));
    return newkey(L, t, &k);
  }
}

// 取对类型为字符串的key对应元素的value部分地址，如果元素不存在，则新建一个
TValue *luaH_setstr (lua_State *L, Table *t, TString *key) {
  const TValue *p = luaH_getstr(t, key);
  if (p != luaO_nilobject)
    return cast(TValue *, p);
  else {
    TValue k;
    setsvalue(L, &k, key);
    return newkey(L, t, &k);
  }
}


static int unbound_search (Table *t, unsigned int j) {
  unsigned int i = j;  /* i is zero or a present index */
  j++;
  /* find `i' and `j' such that i is present and j is not */
  while (!ttisnil(luaH_getnum(t, j))) {
    i = j;
    j *= 2;
    if (j > cast(unsigned int, MAX_INT)) {  /* overflow? */
      /* table was built with bad purposes: resort to linear search */
      i = 1;
      while (!ttisnil(luaH_getnum(t, i))) i++;
      return i - 1;
    }
  }
  /* now do a binary search between them */
  while (j - i > 1) {
    unsigned int m = (i+j)/2;
    if (ttisnil(luaH_getnum(t, m))) j = m;
    else i = m;
  }
  return i;
}


/*
** Try to find a boundary in table `t'. A `boundary' is an integer index
** such that t[i] is non-nil and t[i+1] is nil (and 0 if t[1] is nil).
*/
int luaH_getn (Table *t) {
  unsigned int j = t->sizearray;
  if (j > 0 && ttisnil(&t->array[j - 1])) {
    /* there is a boundary in the array part: (binary) search for it */
    unsigned int i = 0;
    while (j - i > 1) {
      unsigned int m = (i+j)/2;
      if (ttisnil(&t->array[m - 1])) j = m;
      else i = m;
    }
    return i;
  }
  /* else must find a boundary in hash part */
  else if (t->node == dummynode)  /* hash part is empty? */
    return j;  /* that is easy... */
  else return unbound_search(t, j);
}



#if defined(LUA_DEBUG)

Node *luaH_mainposition (const Table *t, const TValue *key) {
  return mainposition(t, key);
}

int luaH_isdummy (Node *n) { return n == dummynode; }

#endif
