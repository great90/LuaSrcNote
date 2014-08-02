/*
** $Id: llex.h,v 1.58.1.1 2007/12/27 13:02:25 roberto Exp $
** Lexical Analyzer
** See Copyright Notice in lua.h
*/

#ifndef llex_h
#define llex_h

#include "lobject.h"
#include "lzio.h"


#define FIRST_RESERVED	257

/* maximum length of a reserved word */// 最长的关键字
#define TOKEN_LEN	(sizeof("function")/sizeof(char))


/*
* WARNING: if you change the order of this enumeration,
* grep "ORDER RESERVED"
*/
enum RESERVED {
  /* terminal symbols denoted by reserved words */
  TK_AND = FIRST_RESERVED, TK_BREAK,
  TK_DO, TK_ELSE, TK_ELSEIF, TK_END, TK_FALSE, TK_FOR, TK_FUNCTION,
  TK_IF, TK_IN, TK_LOCAL, TK_NIL, TK_NOT, TK_OR, TK_REPEAT,
  TK_RETURN, TK_THEN, TK_TRUE, TK_UNTIL, TK_WHILE,
  /* other terminal symbols */
  TK_CONCAT, TK_DOTS, TK_EQ, TK_GE, TK_LE, TK_NE, TK_NUMBER,
  TK_NAME, TK_STRING, TK_EOS
};

/* number of reserved words */
#define NUM_RESERVED	(cast(int, TK_WHILE-FIRST_RESERVED+1))


/* array with token `names' */
LUAI_DATA const char *const luaX_tokens [];

// 记录语义信息 数字或字符串
typedef union {
  lua_Number r;
  TString *ts;
} SemInfo;  /* semantics information */

// 保存解析出来的token
typedef struct Token {
  int token;
  SemInfo seminfo;// 保存经过词法解析后的词(不包括运算符以及[]等),只包括字母以及数字
} Token;

// 核心的解析结构，用于解析期间保存状态
typedef struct LexState {
  int current;  /* current character (charint) *///指示当前的字符 (相对于文件开头的偏移位置)，可以看做一个指针，实际是一个索引
  int linenumber;  /* input line counter *///指示当前解析器的current指针的行位置
  int lastline;  /* line of last token `consumed' */// 当前文件里面的最后一个有作用的记号所在的行
  Token t;  /* current token */// 当前的记号
  Token lookahead;  /* look ahead token */// 下一个 (待分析的) 记号
  struct FuncState *fs;  /* `FuncState' is private to the parser */// 函数状态指针
  struct lua_State *L;//状态机指针
  ZIO *z;  /* input stream *///输入流
  Mbuffer *buff;  /* buffer for tokens */// 存储所有记号的一个缓存
  TString *source;  /* current source name */// 当前源码的的名字
  char decpoint;  /* locale decimal point */// 小数点符号
} LexState;


LUAI_FUNC void luaX_init (lua_State *L);
LUAI_FUNC void luaX_setinput (lua_State *L, LexState *ls, ZIO *z,
                              TString *source);
LUAI_FUNC TString *luaX_newstring (LexState *ls, const char *str, size_t l);
LUAI_FUNC void luaX_next (LexState *ls);
LUAI_FUNC void luaX_lookahead (LexState *ls);
LUAI_FUNC void luaX_lexerror (LexState *ls, const char *msg, int token);
LUAI_FUNC void luaX_syntaxerror (LexState *ls, const char *s);
LUAI_FUNC const char *luaX_token2str (LexState *ls, int token);


#endif
