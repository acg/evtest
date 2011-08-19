#include "iobuf.h"
#include <string.h>


int iobuf_data_added( iobuf_t* io, size_t bytes )
{
  int oldeof = io->eof;
  int oldlen = io->len;

  io->len += bytes;
  
  if (bytes == 0)
    io->eof = 1;

  /* on_full */

  if (bytes > 0 && io->len == io->size)
    if (io->on_full)
      io->on_full( io );

  /* on_nonempty */

  if (io->len > 0 && oldlen == 0)
    if (io->on_nonempty)
      io->on_nonempty( io );

  /* on_eof */

  if (io->eof && !oldeof)
    if (io->on_eof)
      io->on_eof( io );

  /* on_data */

  if (bytes > 0)
    if (io->on_data)
      io->on_data( io );

  return 0;
}


int iobuf_data_removed( iobuf_t* io, size_t bytes )
{
  int oldlen = io->len;
  int left = io->len - bytes;

  if (bytes > 0 && left > 0)
    memmove( io->buf, io->buf+bytes, left );

  io->len = left;

  /* on_empty */

  if (bytes > 0 && io->len == 0)
    if (io->on_empty)
      io->on_empty( io );

  /* on_nonfull */

  if (io->len < io->size && oldlen == io->size)
    if (io->on_nonfull)
      io->on_nonfull( io );

  return 0;
}


int iobuf_init( iobuf_t* io )
{
  memset( io, 0, sizeof *io );
  io->on_data_added = iobuf_data_added;
  io->on_data_removed = iobuf_data_removed;
  return 0;
}

