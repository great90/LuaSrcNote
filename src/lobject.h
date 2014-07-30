/*
** $Id: lobject.h,v 2.20.1.2 2008/08/06 13:29:48 roberto Exp $
** Type definitions for Lua objects
** See Copyright Notice in lua.h
*/


#ifndef lobject_h
#define lobject_h


#include <stdarg.h>


#include "llimits.h"
#include "lua.h"


/* tags for values visible from Lua */
#define LAST_TAG	LUA_TTHREAD

#define NUM_TAGS	(LAST_TAG+1)


/*
** Extra tags for non-values
*/
#define LUA_TPROTO	(LAST_TAG+1)
#define LUA_TUPVAL	(LAST_TAG+2)
#define LUA_TDEADKEY	(LAST_TAG+3)


/*
** Union of all collectable objects
*/
typedef union GCObject GCObject; // 定义在Lstate.h中

/*
** Common Header for all collectable objects (in macro form, to be
** included in other objects)
*/
#define CommonHeader	GCObject *next; lu_byte tt; lu_byte marked
// next指针将可gc的数据串联成链表,tt表示数据类型,marked存放的gc处理时的颜色值

/*
** Common header in struct form
*/
typedef struct GCheader {
  CommonHeader;
} GCheader;




/*
** Union of all Lua values
*/
typedef union {
  GCObject *gc;
  void *p;
  lua_Number n;
  int b;
} Value;


/*
** Tagged Values
*/

#define TValuefields	Value value; int tt

typedef struct lua_TValue {
  TValuefields;
} TValue;

/*
typedef struct lua_TValue {
	union {
	  union GCObject {
		struct GCheader {
		  GCObject *next; lu_byte tt; lu_byte marked;
		} gch;

		union TString ts;
		union Udata u;
		union Closure cl;
		struct Table h;
		struct Proto p;
		struct UpVal uv;
		struct lua_State th;  // thread
	  } gc; // 可gc类型 这部分还是数据的时候,数据部分(除gch外)启用,否则就是gc部分了(gch)

	  void *p;	
	  lua_Number n; // 数字类型
	  int b; // boolean类型
	} value;
	int tt; // 数据类型
} TValue;
*/

/* Macros to test type */
#define ttisnil(o)	(ttype(o) == LUA_TNIL)
#define ttisnumber(o)	(ttype(o) == LUA_TNUMBER)
#define ttisstring(o)	(ttype(o) == LUA_TSTRING)
#define ttistable(o)	(ttype(o) == LUA_TTABLE)
#define ttisfunction(o)	(ttype(o) == LUA_TFUNCTION)
#define ttisboolean(o)	(ttype(o) == LUA_TBOOLEAN)
#define ttisuserdata(o)	(ttype(o) == LUA_TUSERDATA)
#define ttisthread(o)	(ttype(o) == LUA_TTHREAD)
#define ttislightuserdata(o)	(ttype(o) == LUA_TLIGHTUSERDATA)

/* Macros to access values */
#define ttype(o)	((o)->tt) // 返回数据类型值
#define gcvalue(o)	check_exp(iscollectable(o), (o)->value.gc) // 获取可gc类型数据
#define pvalue(o)	check_exp(ttislightuserdata(o), (o)->value.p) // 获取lightuserdata
#define nvalue(o)	check_exp(ttisnumber(o), (o)->value.n) // 获取数字值
#define rawtsvalue(o)	check_exp(ttisstring(o), &(o)->value.gc->ts) // 获取TString值
#define tsvalue(o)	(&rawtsvalue(o)->tsv)	// 获取TString的值(结构体)
#define rawuvalue(o)	check_exp(ttisuserdata(o), &(o)->value.gc->u)// 获取userdata值
#define uvalue(o)	(&rawuvalue(o)->uv) // 获取userdata的值(结构体)
#define clvalue(o)	check_exp(ttisfunction(o), &(o)->value.gc->cl)//获取函数
#define hvalue(o)	check_exp(ttistable(o), &(o)->value.gc->h)// 获取table
#define bvalue(o)	check_exp(ttisboolean(o), (o)->value.b)	// 获取布尔值
#define thvalue(o)	check_exp(ttisthread(o), &(o)->value.gc->th) // 获取thread

#define l_isfalse(o)	(ttisnil(o) || (ttisboolean(o) && bvalue(o) == 0)) // 判断是否为假(包括nil)

/*
** for internal debug only
*/
#define checkconsistency(obj) \
  lua_assert(!iscollectable(obj) || (ttype(obj) == (obj)->value.gc->gch.tt))

#define checkliveness(g,obj) \
  lua_assert(!iscollectable(obj) || \
  ((ttype(obj) == (obj)->value.gc->gch.tt) && !isdead(g, (obj)->value.gc)))


/* Macros to set values */
#define setnilvalue(obj) ((obj)->tt=LUA_TNIL)

#define setnvalue(obj,x) \
  { TValue *i_o=(obj); i_o->value.n=(x); i_o->tt=LUA_TNUMBER; }

#define setpvalue(obj,x) \
  { TValue *i_o=(obj); i_o->value.p=(x); i_o->tt=LUA_TLIGHTUSERDATA; }

#define setbvalue(obj,x) \
  { TValue *i_o=(obj); i_o->value.b=(x); i_o->tt=LUA_TBOOLEAN; }

#define setsvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TSTRING; \
    checkliveness(G(L),i_o); }

#define setuvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TUSERDATA; \
    checkliveness(G(L),i_o); }

#define setthvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TTHREAD; \
    checkliveness(G(L),i_o); }

#define setclvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TFUNCTION; \
    checkliveness(G(L),i_o); }

#define sethvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TTABLE; \
    checkliveness(G(L),i_o); }

#define setptvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TPROTO; \
    checkliveness(G(L),i_o); }




#define setobj(L,obj1,obj2) \
  { const TValue *o2=(obj2); TValue *o1=(obj1); \
    o1->value = o2->value; o1->tt=o2->tt; \
    checkliveness(G(L),o1); }


/*
** different types of sets, according to destination
*/

/* from stack to (same) stack */
#define setobjs2s	setobj
/* to stack (not from same stack) */
#define setobj2s	setobj
#define setsvalue2s	setsvalue
#define sethvalue2s	sethvalue
#define setptvalue2s	setptvalue
/* from table to same table */
#define setobjt2t	setobj
/* to table */
#define setobj2t	setobj
/* to new object */
#define setobj2n	setobj
#define setsvalue2n	setsvalue

#define setttype(obj, tt) (ttype(obj) = (tt))


#define iscollectable(o)	(ttype(o) >= LUA_TSTRING)



typedef TValue *StkId;  /* index to stack elements */


/*
** String headers for string table
*/
typedef union TString { // dummy使GCObject字节对齐,使得GCObject至少占一个word大小
  L_Umaxalign dummy;  /* ensures maximum alignment for strings */
  struct {
    CommonHeader;
    lu_byte reserved; // 是否为保留字符串,关键字;不为0则表示在数组luaX_tokens中的索引,不会被回收;在Llex.c的luaX_init中处理
    unsigned int hash;	// hash值
    size_t len;	// 字符串长度
  } tsv;
} TString;


#define getstr(ts)	cast(const char *, (ts) + 1)
#define svalue(o)       getstr(rawtsvalue(o))



typedef union Udata {
  L_Umaxalign dummy;  /* ensures maximum alignment for `local' udata */
  struct {
    CommonHeader;	// 与TValue中的GCHeader对应
    struct Table *metatable; // 元表
    struct Table *env;	// 上下文环境，创建userdata时，会把当前执行语句的curenv赋给userdata的env，可修改
    size_t len; // 绑定对象申请的空间大小
  } uv; 	// 绑定的C对象或数据内存紧跟在Udata后面
} Udata;




/*
** Function Prototypes
*/// 函数原型
typedef struct Proto {
  CommonHeader;	// 与GCHeader对应
  TValue *k;  /* constants used by the function */// 函数使用的常量数组
  Instruction *code;	// 虚拟机指令码数组
  struct Proto **p;  /* functions defined inside the function */// 函数内定义的函数原型
  int *lineinfo;  /* map from opcodes to source lines */ // 每个操作码所对应的行号，主要用于调试
  struct LocVar *locvars;  /* information about local variables */ // 局部变量信息
  TString **upvalues;  /* upvalue names */// upvalue名称 用于调试及API使用
  TString  *source;	// 函数来源，用于调试
  int sizeupvalues;	// upvalues名称数组的长度
  int sizek;  /* size of `k' */// 常量数组长度
  int sizecode;	// code数组长度
  int sizelineinfo;	// lineinfo数组长度
  int sizep;  /* size of `p' */	// p数组长度
  int sizelocvars;	// locvars数组长度
  int linedefined;	// 函数定义起始行号，即function语句行号
  int lastlinedefined;	// 函数结束行号，即end语句行号
  GCObject *gclist;	// 用于gc
  lu_byte nups;  /* number of upvalues */// upvalue的个数；nups在语法分析时生成，而Closure中的nupvalues是动态计算的
  lu_byte numparams; // 参数个数
  lu_byte is_vararg; // 是否变参函数
  lu_byte maxstacksize; // 函数所使用的stacksize
} Proto;
// Proto的所有参数都在语法分析和中间代码生成时获取，相当于编译出来的汇编码一样是不会变的，动态性在Closure中体现。

/* masks for new-style vararg */// 新变参风格掩码
#define VARARG_HASARG		1
#define VARARG_ISVARARG		2
#define VARARG_NEEDSARG		4


typedef struct LocVar {	// 局部变量
  TString *varname;
  int startpc;  /* first point where variable is active */
  int endpc;    /* first point where variable is dead */
} LocVar;



/*
** Upvalues
*//* UpVal的实现：UpVal是在函数闭包生成的时候（运行到function时）绑定的。UpVal在闭包还没关闭前（即函数返回前），
是对栈的引用，这样可以在函数内修改对应的值从而修改UpVal的值；闭包关闭后（即函数退出后），UpVal不再是指针，而是值。 */
typedef struct UpVal {
  CommonHeader;
  TValue *v;  /* points to stack or to its own value */// 当函数打开时是指向对应stack位置值，当关闭后则指向自己
  union {
    TValue value;  /* the value (when closed) */// 函数关闭后保存的值
    struct {  /* double linked list (when open) */// 当函数打开时全局绑定的用于GC的双向链表
      struct UpVal *prev;
      struct UpVal *next;
    } l;
  } u;
} UpVal;


/*
** Closures
*/
// 闭包 CommonHeader与TValue中GCHeader对应的部分; isC:是否CClosure; nupvalues:外部对象数
#define ClosureHeader \
	CommonHeader; lu_byte isC; lu_byte nupvalues; GCObject *gclist; \
	struct Table *env	// nupvalues表示upvalue或者upvals的大小 env是运行环境

typedef struct CClosure {
  ClosureHeader;
  lua_CFunction f;	// 指向自定义C函数的指针
  TValue upvalue[1];// 函数运行所需要的一些参数(比如string 的match函数,它所需要的几个参数都会保存在upvalue里面
} CClosure;

// 在lua中闭包和函数是原型是一样的,只不过函数的upvalue为空,而闭包upvalue包含了它所需要的(非自身的)局部变量值
typedef struct LClosure {
  ClosureHeader;
  struct Proto *p;	// Lua的函数原型
  UpVal *upvals[1];	// Lua的函数upvalue,因具体实现需要一些额外数据,所以不直接用TValue
} LClosure;

// lua通过upvalue结构实现闭包 对任何外层局部变量的存取间接地通过upvalue来进行，当函数创建的时候会有一个局部变量表upvals; 当闭包创建完毕，会复制upvals的值到upvalue
typedef union Closure {
  CClosure c;
  LClosure l;
} Closure;


#define iscfunction(o)	(ttype(o) == LUA_TFUNCTION &&  (o)->c.isC)
#define isLfunction(o)	(ttype(o) == LUA_TFUNCTION && !clvalue(o)->c.isC)


/*
** Tables
*/

typedef union TKey {
  struct {
    TValuefields;	// Value value; int tt; 就是一个TValue
    struct Node *next;  /* for chaining */// 同一个hash值下冲突时通过该指针形成链表
  } nk;
  TValue tvk;
} TKey;


typedef struct Node {
  TValue i_val;
  TKey i_key;	
} Node;


typedef struct Table {
  CommonHeader;	// 与TValue中GCHeader对应的部分
  lu_byte flags;  /* 1<<p means tagmethod(p) is not present */ // 标记是否存在某个元方法(8个),是一种优化方式
  lu_byte lsizenode;  /* log2 of size of `node' array */// hash部分长度的log2值, hash部分长度为2的幂，每次增长都是翻倍
  struct Table *metatable;// 元表
  TValue *array;  /* array part */// 数组部分
  Node *node;	// hash部分 物理上采用一个Node数组，逻辑上作为一个hash表；通过key部分的next指针指向下一个hash值相同产生冲突的元素形成链表
  Node *lastfree;  /* any free position is before this position */// hash部分最后一个空闲的位置
  GCObject *gclist;// 用与gc
  int sizearray;  /* size of `array' array */// 数组大小
} Table;



/*
** `module' operation for hashing (size is always a power of 2)
*/
#define lmod(s,size) \
	(check_exp((size&(size-1))==0, (cast(int, (s) & ((size)-1)))))


#define twoto(x)	(1<<(x))
#define sizenode(t)	(twoto((t)->lsizenode))


#define luaO_nilobject		(&luaO_nilobject_)

LUAI_DATA const TValue luaO_nilobject_;

#define ceillog2(x)	(luaO_log2((x)-1) + 1)

LUAI_FUNC int luaO_log2 (unsigned int x);
LUAI_FUNC int luaO_int2fb (unsigned int x);
LUAI_FUNC int luaO_fb2int (int x);
LUAI_FUNC int luaO_rawequalObj (const TValue *t1, const TValue *t2);
LUAI_FUNC int luaO_str2d (const char *s, lua_Number *result);
LUAI_FUNC const char *luaO_pushvfstring (lua_State *L, const char *fmt,
                                                       va_list argp);
LUAI_FUNC const char *luaO_pushfstring (lua_State *L, const char *fmt, ...);
LUAI_FUNC void luaO_chunkid (char *out, const char *source, size_t len);


#endif

