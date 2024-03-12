#ifndef __ASYNC_IO_H__
#define __ASYNC_IO_H__
#include <aio.h>
#include <core.h>

struct aiocb async_pipe(int fd,u8* buffer, size_t size);
{
  struct aiocb aio = {0};
  aio.aio_buf = buffer;
  aio.aio_fildes = fd;
  aio.aio_nbytes = size;
  aio.aio_offset = 0; // start at begining
	return aio;
}

#endif //__ASYNC_IO_H__
