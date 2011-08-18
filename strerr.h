#ifndef STRERR_H
#define STRERR_H

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#define strerr_die2(e,fmt,x1,x2) \
  { fprintf(stderr,fmt,x1,x2); _exit(e); }

#define strerr_die3(e,fmt,x1,x2,x3) \
  { fprintf(stderr,fmt,x1,x2,x3); _exit(e); }

#define strerr_die4(e,fmt,x1,x2,x3,x4) \
  { fprintf(stderr,fmt,x1,x2,x3,x4); _exit(e); }

#define strerr_diesys(x1) \
  strerr_die2(errno,"%s: %s\n",x1,strerror(errno));

#endif
