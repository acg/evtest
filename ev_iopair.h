#pragma once

#include <sys/types.h>
#include <ev.h>
#include "iobuf.h"
#include "ev_iobuf.h"


typedef struct ev_iopair_def_t ev_iopair_t;
typedef int (*ev_iopair_callback)( ev_iopair_t* );

struct ev_iopair_def_t
{
  struct ev_loop *loop;
  iobuf_t rio;
  iobuf_t wio;
  ev_iobuf_t ev_rio;
  ev_iobuf_t ev_wio;
  void *ctx;

  ev_iopair_callback on_data;
  ev_iopair_callback on_close;

  // TODO: 
  // on_writable?
  // on_readable?
  // on_reof?
  // on_done?
};


int ev_iopair_init
(
  ev_iopair_t *self,
  struct ev_loop *loop,
  void *ctx,
  int rfd,
  char *rbuf,
  size_t rsize,
  int wfd,
  char *wbuf,
  size_t wsize
);

