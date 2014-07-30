/*
** $Id: lstate.h,v 2.24.1.2 2008/01/03 15:20:39 roberto Exp $
** Global State
** See Copyright Notice in lua.h
*/

#ifndef lstate_h
#define lstate_h

#include "lua.h"

#include "lobject.h"
#include "ltm.h"
#include "lzio.h"



struct lua_longjmp;  /* defined in ldo.c */


/* table of globals */
#define gt(L)	(&L->l_gt)

/* registry */
#define registry(L)	(&G(L)->l_registry)


/* extra stack space to handle TM calls and some other extras */
#define EXTRA_STACK   5


#define BASIC_CI_SIZE           8

#define BASIC_STACK_SIZE        (2*LUA_MINSTACK)



typedef struct stringtable {
  GCObject **hash; // 对于TString,先计算其hash值,既在stringtable中hash的索引值,若该位置已经有元素,则用链表串起来;一个GCObject的表，在这里其实是个TString*数组
  lu_int32 nuse;  /* number of elements */ // 已有的字符串个数
  int size;	// 哈希表大小
} stringtable;


/*
** informations about a call
*/// 调用信息
typedef struct CallInfo {
  StkId base;  /* base for this function */
  StkId func;  /* function index in the stack */
  StkId	top;  /* top for this function */
  const Instruction *savedpc;// 函数调用中断时记录当前闭包执行到的pc位置
  int nresults;  /* expected number of results from this function */// 返回值个数，-1为任意返回个数
  int tailcalls;  /* number of tail calls lost under this entry */// 记录尾调用次数信息，用于调试
} CallInfo;		// 每次递归调用都会生成一个CallInfo，而全局CallInfo栈的大小有限，基于2的幂，因此最大深度为16834(2^14<LUAI_MAXCALLS(20000))
// 尾调用是指在return后直接调用函数，不能有其它操作；尾调用是在编译时分析出来的，有独立的操作码OP_TAILCALL，在虚拟机中的执行代码在lvm.c 603-634


#define curr_func(L)	(clvalue(L->ci->func))
#define ci_func(ci)	(clvalue((ci)->func))
#define f_isLua(ci)	(!ci_func(ci)->c.isC)
#define isLua(ci)	(ttisfunction((ci)->func) && f_isLua(ci))


/*
** `global state', shared by all threads of this state
*/ // 全局状态，被多个线程共享 主要包含了gc相关的东西
typedef struct global_State {
  stringtable strt;  /* hash table for strings */ // 专门用于存放字符串的全局hash数组
  lua_Alloc frealloc;  /* function to reallocate memory */// 内存分配函数
  void *ud;         /* auxiliary data to `frealloc' */// frealloc的辅助数据
  lu_byte currentwhite;
  lu_byte gcstate;  /* state of garbage collector */
  int sweepstrgc;  /* position of sweep in `strt' */
  GCObject *rootgc;  /* list of all collectable objects */
  GCObject **sweepgc;  /* position of sweep in `rootgc' */
  GCObject *gray;  /* list of gray objects */
  GCObject *grayagain;  /* list of objects to be traversed atomically */
  GCObject *weak;  /* list of weak tables (to be cleared) */
  GCObject *tmudata;  /* last element of list of userdata to be GC */
  Mbuffer buff;  /* temporary buffer for string concatentation */
  lu_mem GCthreshold;
  lu_mem totalbytes;  /* number of bytes currently allocated */
  lu_mem estimate;  /* an estimate of number of bytes actually in use */
  lu_mem gcdept;  /* how much GC is `behind schedule' */
  int gcpause;  /* size of pause between successive GCs */
  int gcstepmul;  /* GC `granularity' */
  lua_CFunction panic;  /* to be called in unprotected errors */
  TValue l_registry;	// 对应LUAREGISTRYINDEX的全局table
  struct lua_State *mainthread;
  UpVal uvhead;  /* head of double-linked list of all open upvalues */ // 整个lua虚拟机里面所有栈的upvalue链表的头
  struct Table *mt[NUM_TAGS];  /* metatables for basic types */// Lua5.0的特性，基本类型的元表
  TString *tmname[TM_N];  /* array with tag-method names */// 元方法名称字符串数组
} global_State;


/*
** `per thread' state
*/ // 单个线程状态 typedef TValue *StkId lua_state表示一个lua虚拟机，它是per-thread的，也就是一个协程
struct lua_State {
  CommonHeader;
  // 栈相关
  StkId top;  /* first free slot in the stack */// 指向当前线程栈的栈顶指针，指向栈上的第一个空闲的slot
  StkId base;  /* base of current function */// 指向当前函数运行的相对基位置，参考闭包
  StkId stack_last;  /* last free slot in the stack */// 栈的实际最后一个位置（栈的长度是动态增长的） 栈上的最后一个空闲的slot
  StkId stack;  /* stack base */// 栈底 整个栈的栈底

  global_State *l_G;	// 指向全局状态的指针
  // 函数相关
  lu_byte status;	// 线程脚本的状态，见Lua.h L42
  CallInfo *ci;  /* call info for current function */// 当前线程运行的函数调用信息
  const Instruction *savedpc;  /* `savedpc' of current function */// 函数调用前，记录上一个函数的pc位置
  CallInfo *end_ci;  /* points after end of ci array*/// 指向函数调用栈的栈顶
  CallInfo *base_ci;  /* array of CallInfo's */// 指向函数调用栈的栈底
  // 需要用到的长度、大小、C嵌套数量等
  int stacksize;	// 栈的大小
  int size_ci;  /* size of array `base_ci' */// 函数调用栈的大小
  unsigned short nCcalls;  /* number of nested C calls */// 当前C函数的调用的深度
  unsigned short baseCcalls;  /* nested C calls when resuming coroutine */// 用于记录每个线程状态的C函数调用深度的辅助成员
  lu_byte hookmask;	// 是否存在某个钩子函数的掩码，见Lua.h L310
  lu_byte allowhook;// 是否允许hook
  int basehookcount;// 用户设置的出发LUA_MASKCOUNT对应的钩子函数需要执行的指令数
  int hookcount;// 与上面对应的已运行的指令数
  lua_Hook hook;// 用户注册的钩子函数
  // 全局环境相关
  TValue l_gt;  /* table of globals */// 当前线程执行的全局环境表
  TValue env;  /* temporary place for environments */// 当前运行的环境表
  // GC相关
  GCObject *openupval;  /* list of open upvalues in this stack */ // 当前的栈上的所有open的uvalue
  GCObject *gclist;	// 用于gc
  // 错误处理相关
  struct lua_longjmp *errorJmp;  /* current error recover point */// 发生错误时的长跳转位置
  ptrdiff_t errfunc;  /* current error handling function (stack index) */// 用户注册的错误回调函数
};


#define G(L)	(L->l_G)


/*
** Union of all collectable objects
*/
union GCObject {// 每个GCObject结构体都有一个CommonHeader位于最开始部分
  GCheader gch;
  union TString ts;
  union Udata u;
  union Closure cl;
  struct Table h;
  struct Proto p;
  struct UpVal uv;	// upvalue也是可GC的
  struct lua_State th;  /* thread */
}; // 这部分还是数据的时候,数据部分(除gch外)启用,否则就是gc部分了(gch) 


/* macros to convert a GCObject into a specific value */
#define rawgco2ts(o)	check_exp((o)->gch.tt == LUA_TSTRING, &((o)->ts))
#define gco2ts(o)	(&rawgco2ts(o)->tsv)
#define rawgco2u(o)	check_exp((o)->gch.tt == LUA_TUSERDATA, &((o)->u))
#define gco2u(o)	(&rawgco2u(o)->uv)
#define gco2cl(o)	check_exp((o)->gch.tt == LUA_TFUNCTION, &((o)->cl))
#define gco2h(o)	check_exp((o)->gch.tt == LUA_TTABLE, &((o)->h))
#define gco2p(o)	check_exp((o)->gch.tt == LUA_TPROTO, &((o)->p))
#define gco2uv(o)	check_exp((o)->gch.tt == LUA_TUPVAL, &((o)->uv))
#define ngcotouv(o) \
	check_exp((o) == NULL || (o)->gch.tt == LUA_TUPVAL, &((o)->uv))
#define gco2th(o)	check_exp((o)->gch.tt == LUA_TTHREAD, &((o)->th))

/* macro to convert any Lua object into a GCObject */
#define obj2gco(v)	(cast(GCObject *, (v)))


LUAI_FUNC lua_State *luaE_newthread (lua_State *L);
LUAI_FUNC void luaE_freethread (lua_State *L, lua_State *L1);

#endif

