#include "ev_iobuf.h"
#include <stdio.h> // fprintf
#include <sys/types.h>
#include "strerr.h"

/* FIXME use logging */


void ev_iobuf_reader( struct ev_loop *loop, ev_io *watcher, int revents )
{
  iobuf_t *io = ((ev_iobuf_t*)watcher)->io;

  fprintf( stderr, "fd %d readable\n", io->fd );

  ssize_t bytes;

  if ((bytes = read(io->fd, io->buf + io->len, io->size - io->len)) < 0)
    strerr_diesys( "read error" );

  fprintf( stderr, "read %u bytes\n", bytes );

  io->on_data_added( io, bytes );
}


void ev_iobuf_writer( struct ev_loop *loop, ev_io *watcher, int revents )
{
  iobuf_t *io = ((ev_iobuf_t*)watcher)->io;

  fprintf( stderr, "fd %d writable\n", io->fd );

  ssize_t bytes;

  if ((bytes = write(io->fd, io->buf, io->len)) < 0)
    strerr_diesys( "write error" );

  fprintf( stderr, "wrote %u bytes\n", bytes );

  /* on_data_removed */

  io->on_data_removed( io, bytes );
}

