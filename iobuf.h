#pragma once

#include <sys/types.h>


typedef struct iobuf_def_t iobuf_t;
typedef int (*iobuf_callback)( iobuf_t* );
typedef int (*iobuf_count_callback)( iobuf_t*, size_t );

struct iobuf_def_t
{
  int fd;
  char *buf;
  int size;
  int len;
  int eof;
  void *ctx;

  iobuf_count_callback  on_data_added;
  iobuf_count_callback  on_data_removed;
  iobuf_callback        on_data;
  iobuf_callback        on_empty;
  iobuf_callback        on_full;
  iobuf_callback        on_nonempty;
  iobuf_callback        on_nonfull;
  iobuf_callback        on_eof;
};

int iobuf_init( iobuf_t* io );

