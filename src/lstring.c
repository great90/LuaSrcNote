/*
** $Id: lstring.c,v 2.8.1.1 2007/12/27 13:02:25 roberto Exp $
** String table (keeps all strings handled by Lua)
** See Copyright Notice in lua.h
*/


#include <string.h>

#define lstring_c
#define LUA_CORE

#include "lua.h"

#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"



void luaS_resize (lua_State *L, int newsize) { // 调整字符串空间的大小
  GCObject **newhash;
  stringtable *tb;
  int i;
  if (G(L)->gcstate == GCSsweepstring)
    return;  /* cannot resize during GC traverse */
  newhash = luaM_newvector(L, newsize, GCObject *); // 创建一个新的(GCObject*)数组,大小为newsize
  tb = &G(L)->strt;
  for (i=0; i<newsize; i++) newhash[i] = NULL;
  /* rehash */// 重新进行hash
  for (i=0; i<tb->size; i++) {
    GCObject *p = tb->hash[i];
    while (p) {  /* for each node in the list */
      GCObject *next = p->gch.next;  /* save next */
      unsigned int h = gco2ts(p)->hash;
      int h1 = lmod(h, newsize);  /* new position *///新的位置
      lua_assert(cast_int(h%newsize) == lmod(h, newsize));
      p->gch.next = newhash[h1];  /* chain it */
      newhash[h1] = p;
      p = next;
    }
  }
  luaM_freearray(L, tb->hash, tb->size, TString *);// 释放旧的内存
  tb->size = newsize;
  tb->hash = newhash;
}

// 创建一个Lua字符串 每创建一个新字符串都会做很多操作，不建议频繁做字符串创建、连接、销毁等操作，最好进行缓存 固定格式的用format
static TString *newlstr (lua_State *L, const char *str, size_t l,
                                       unsigned int h) { // 长度和hash值
  TString *ts;
  stringtable *tb;
  if (l+1 > (MAX_SIZET - sizeof(TString))/sizeof(char))
    luaM_toobig(L);
  ts = cast(TString *, luaM_malloc(L, (l+1)*sizeof(char)+sizeof(TString))); // 申请的内存既要保存TString对象，还要保存C字符串
  ts->tsv.len = l;
  ts->tsv.hash = h;
  ts->tsv.marked = luaC_white(G(L));
  ts->tsv.tt = LUA_TSTRING;
  ts->tsv.reserved = 0;
  memcpy(ts+1, str, l*sizeof(char));// 将C字符串拷贝到TString对象后
  ((char *)(ts+1))[l] = '\0';  /* ending 0 */
  tb = &G(L)->strt;
  h = lmod(h, tb->size);// 截取hash值，防止越界
  ts->tsv.next = tb->hash[h];  /* chain new entry */// 对于hash引起的冲突采用链接的方式解决
  tb->hash[h] = obj2gco(ts); // 新加的字符串在链表前端
  tb->nuse++;
  if (tb->nuse > cast(lu_int32, tb->size) && tb->size <= MAX_INT/2)
    luaS_resize(L, tb->size*2);  /* too crowded */// 当前个数超过容量，则将容量扩充至原来的两倍
  return ts;
}


TString *luaS_newlstr (lua_State *L, const char *str, size_t l) {
  GCObject *o;
  unsigned int h = cast(unsigned int, l);  /* seed */
  size_t step = (l>>5)+1;  /* if string is too long, don't hash all its chars */
  size_t l1;
  for (l1=l; l1>=step; l1-=step)  /* compute hash */ // 不对每个字符进行hash计算，采用步长方式减少计算
    h = h ^ ((h<<5)+(h>>2)+cast(unsigned char, str[l1-1])); // JS Hash算法
  for (o = G(L)->strt.hash[lmod(h, G(L)->strt.size)];
       o != NULL;
       o = o->gch.next) {// 根据hash值遍历已有的字符串
    TString *ts = rawgco2ts(o);
    if (ts->tsv.len == l && (memcmp(str, getstr(ts), l) == 0)) {
      /* string may be dead */
      if (isdead(G(L), o)) changewhite(o);//如果被GC标记回收，则将标记变白，不回收
      return ts; // 找到已存在的字符串
    }
  }
  return newlstr(L, str, l, h);  /* not found */// 没有找到则创建一个
}


Udata *luaS_newudata (lua_State *L, size_t s, Table *e) {
  Udata *u;
  if (s > MAX_SIZET - sizeof(Udata))
    luaM_toobig(L);
  u = cast(Udata *, luaM_malloc(L, s + sizeof(Udata)));// Udata的内存布局与TString类似
  u->uv.marked = luaC_white(G(L));  /* is not finalized */
  u->uv.tt = LUA_TUSERDATA;
  u->uv.len = s;
  u->uv.metatable = NULL;
  u->uv.env = e;
  /* chain it on udata list (after main thread) */
  u->uv.next = G(L)->mainthread->next;
  G(L)->mainthread->next = obj2gco(u);// 链接到mainthread中，由其管理udata
  return u;
}

/* 常用的字符串的哈希算法中，KDRHash无论是在实际效果还是编码实现中，效果都是最突出的。
APHash也是较为优秀的算法。DJBHash,JSHash,RSHash与SDBMHash各有千秋。PJWHash与ELFHash效
果最差，但得分相似，其算法本质是相似的。
在Lua中使用到的是JSHash算法 */