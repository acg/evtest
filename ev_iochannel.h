#pragma once

#include <sys/types.h>
#include <ev.h>
#include "iobuf.h"
#include "ev_iobuf.h"


typedef struct ev_iochannel_def_t ev_iochannel_t;
typedef int (*ev_iochannel_callback)( ev_iochannel_t* );

struct ev_iochannel_def_t
{
  struct ev_loop *loop;
  iobuf_t rio;
  iobuf_t wio;
  ev_iobuf_t ev_rio;
  ev_iobuf_t ev_wio;
  void *ctx;

  ev_iochannel_callback on_data;
  ev_iochannel_callback on_close;

  // TODO: 
  // on_writable?
  // on_readable?
  // on_reof?
  // on_done?
};


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
);

