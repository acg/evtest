#pragma once

#include <ev.h>
#include "iobuf.h"


typedef struct
{
  ev_io watcher;
  iobuf_t *io;
}
ev_iobuf_t;

void ev_iobuf_reader( struct ev_loop *loop, ev_io *watcher, int revents );
void ev_iobuf_writer( struct ev_loop *loop, ev_io *watcher, int revents );

