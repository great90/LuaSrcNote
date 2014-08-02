/*
** $Id: lzio.h,v 1.21.1.1 2007/12/27 13:02:25 roberto Exp $
** Buffered streams
** See Copyright Notice in lua.h
*/


#ifndef lzio_h
#define lzio_h

#include "lua.h"

#include "lmem.h"


#define EOZ	(-1)			/* end of stream */

typedef struct Zio ZIO;

#define char2int(c)	cast(int, cast(unsigned char, (c)))
// zgetc返回的是一个字符，这个字符返回到LexState中后，类型会成为int型，所以在宏里面用了一个char2int强制转换
#define zgetc(z)  (((z)->n--)>0 ?  char2int(*(z)->p++) : luaZ_fill(z))
// 读完一个字符，会将字符缓冲中的指针p向前移一位，并且余量计数n减1，一直到n降到0时，立马调用luaZ_fill重新填充字符缓冲区
typedef struct Mbuffer {
  char *buffer;	// 字符串指针
  size_t n;		// 当前待存储字符的一个索引(小于buffsize)
  size_t buffsize;// 当前缓存的大小
} Mbuffer;

#define luaZ_initbuffer(L, buff) ((buff)->buffer = NULL, (buff)->buffsize = 0)

#define luaZ_buffer(buff)	((buff)->buffer)
#define luaZ_sizebuffer(buff)	((buff)->buffsize)
#define luaZ_bufflen(buff)	((buff)->n)

#define luaZ_resetbuffer(buff) ((buff)->n = 0)


#define luaZ_resizebuffer(L, buff, size) \
	(luaM_reallocvector(L, (buff)->buffer, (buff)->buffsize, size, char), \
	(buff)->buffsize = size)

#define luaZ_freebuffer(L, buff)	luaZ_resizebuffer(L, buff, 0)


LUAI_FUNC char *luaZ_openspace (lua_State *L, Mbuffer *buff, size_t n);
LUAI_FUNC void luaZ_init (lua_State *L, ZIO *z, lua_Reader reader,
                                        void *data);
LUAI_FUNC size_t luaZ_read (ZIO* z, void* b, size_t n);	/* read next n bytes */
LUAI_FUNC int luaZ_lookahead (ZIO *z);



/* --------- Private Part ------------------ */

struct Zio {
  size_t n;			/* bytes still unread */
  const char *p;		/* current position in buffer */
  lua_Reader reader;	// 读入器，是外部挂入的一个钩子函数，与具体平台相关
  void* data;			/* additional data */
  lua_State *L;			/* Lua state (for reader) */
};


LUAI_FUNC int luaZ_fill (ZIO *z);

#endif
