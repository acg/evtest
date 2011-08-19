#include "ev_iochannel.h"
#include <string.h>  // memset


int on_rio_full( iobuf_t *io )
{
  ev_iochannel_t *self = (ev_iochannel_t*)io->ctx;
  ev_io_stop( self->loop, &self->ev_rio.watcher );
  return 0;
}

int on_wio_empty( iobuf_t *io )
{
  ev_iochannel_t *self = (ev_iochannel_t*)io->ctx;
  ev_io_stop( self->loop, &self->ev_wio.watcher );

  if (self->rio.eof)
    if (self->on_close)
      self->on_close( self );

  return 0;
}  

int on_rio_nonfull( iobuf_t *io )
{
  ev_iochannel_t *self = (ev_iochannel_t*)io->ctx;
  ev_io_start( self->loop, &self->ev_rio.watcher );
  return 0;
}  

int on_rio_eof( iobuf_t *io )
{
  ev_iochannel_t *self = (ev_iochannel_t*)io->ctx;
  ev_io_stop( self->loop, &self->ev_rio.watcher );

  if (!self->wio.len)
    if (self->on_close)
      self->on_close( self );

  return 0;
}  

int on_wio_nonempty( iobuf_t *io )
{
  ev_iochannel_t *self = (ev_iochannel_t*)io->ctx;
  ev_io_start( self->loop, &self->ev_wio.watcher );
  return 0;
}

int on_rio_data( iobuf_t *io )
{
  ev_iochannel_t *self = (ev_iochannel_t*)io->ctx;
  if (self->on_data) return self->on_data( self );
  return 0;
}

int ev_iochannel_init
(
  ev_iochannel_t *self,
  struct ev_loop *loop,
  void *ctx,
  int rfd,
  char *rbuf,
  size_t rsize,
  int wfd,
  char *wbuf,
  size_t wsize
)
{
  memset( self, 0, sizeof *self );

  self->loop = loop;
  self->ctx = ctx;

  iobuf_init( &self->rio );
  iobuf_init( &self->wio );

  self->rio.fd = rfd;
  self->rio.buf = rbuf;
  self->rio.size = rsize;
  self->rio.len = 0;
  self->rio.eof = 0;
  self->rio.ctx = self;
  self->rio.on_full = on_rio_full;
  self->rio.on_nonfull = on_rio_nonfull;
  self->rio.on_eof = on_rio_eof;
  self->rio.on_data = on_rio_data;

  self->wio.fd = wfd;
  self->wio.buf = wbuf;
  self->wio.size = wsize;
  self->wio.len = 0;
  self->wio.eof = 0;
  self->wio.ctx = self;
  self->wio.on_empty = on_wio_empty;
  self->wio.on_nonempty = on_wio_nonempty;

  self->ev_rio.io = &self->rio;
  self->ev_wio.io = &self->wio;

  ev_init( (ev_io*)&self->ev_rio, ev_iobuf_reader );
  ev_io_set( (ev_io*)&self->ev_rio, rfd, EV_READ );
  // FIXME start this now, or wait?
  ev_io_start( loop, (ev_io*)&self->ev_rio );

  ev_init( (ev_io*)&self->ev_wio, ev_iobuf_writer );
  ev_io_set( (ev_io*)&self->ev_wio, wfd, EV_WRITE );

  return 0;
}

