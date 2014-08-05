/*
** $Id: lparser.h,v 1.57.1.1 2007/12/27 13:02:25 roberto Exp $
** Lua Parser 语法分析
** See Copyright Notice in lua.h
*/

#ifndef lparser_h
#define lparser_h

#include "llimits.h"
#include "lobject.h"
#include "lzio.h"


/*
** Expression descriptor
*/
// 表达式类型
typedef enum {
  VVOID,	/* no value */
  VNIL,
  VTRUE,
  VFALSE,
  VK,		/* info = index of constant in `k' */// k内常量的索引
  VKNUM,	/* nval = numerical value */// 数值
  VLOCAL,	/* info = local register */// 局部寄存器
  VUPVAL,       /* info = index of upvalue in `upvalues' */// upvalues上的索引
  VGLOBAL,	/* info = index of table; aux = index of global name in `k' */// info = 表索引; aux = k内全局名称的索引
  VINDEXED,	/* info = table register; aux = index register (or `k') */// info = 表寄存器; aux = 索引寄存器（或k）
  VJMP,		/* info = instruction pc */// 指令pc
  VRELOCABLE,	/* info = instruction pc */
  VNONRELOC,	/* info = result register */// 结果寄存器
  VCALL,	/* info = instruction pc */
  VVARARG	/* info = instruction pc */
} expkind;
// 表达式描述
typedef struct expdesc {
  expkind k;
  union {
    struct { int info, aux; } s; // info表示语句对应的索引(在指令数组code(proto结构体)中)或者说表示为对应constant的索引(proto的TValue *k这个数组的索引)
    lua_Number nval;
  } u;
  int t;  /* patch list of `exit when true' */// 真时退出的补丁列表
  int f;  /* patch list of `exit when false' */// 假时退出的补丁列表
} expdesc;


typedef struct upvaldesc {
  lu_byte k;
  lu_byte info;
} upvaldesc;


struct BlockCnt;  /* defined in lparser.c */


/* state needed to generate code for a given function */
typedef struct FuncState {	// 解析到的函数的状态
  Proto *f;  /* current function header */// 当前函数头
  Table *h;  /* table to find (and reuse) elements in `k' */// 用来查找（或重用）k内元素的表
  struct FuncState *prev;  /* enclosing function */// 闭合函数
  struct LexState *ls;  /* lexical state */// 词法分析状态机
  struct lua_State *L;  /* copy of the Lua state */// Lua状态的复制
  struct BlockCnt *bl;  /* chain of current blocks */// 当前块链
  int pc;  /* next position to code (equivalent to `ncode') */// 代码的下一个位置（等价于ncode）
  int lasttarget;   /* `pc' of last `jump target' */// 上一次jump target的pc
  int jpc;  /* list of pending jumps to `pc' */// 即将跳转的pc列表
  int freereg;  /* first free register */// 第一个空闲的寄存器
  int nk;  /* number of elements in `k' */// k内的元素个数
  int np;  /* number of elements in `p' */// p内的元素个数
  short nlocvars;  /* number of elements in `locvars' */// locvars内的元素个数
  lu_byte nactvar;  /* number of active local variables */// 激活的局部变量的个数
  upvaldesc upvalues[LUAI_MAXUPVALUES];  /* upvalues */
  unsigned short actvar[LUAI_MAXVARS];  /* declared-variable stack */// 已声明变量的堆栈
} FuncState;


LUAI_FUNC Proto *luaY_parser (lua_State *L, ZIO *z, Mbuffer *buff,
                                            const char *name);


#endif
