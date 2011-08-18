#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <ev.h>
#include <stdio.h>
#include <string.h>
#include "strerr.h"


#define MAX_CLIENTS 3
#define BUFSIZE 1024


typedef struct
{
  int rfd;
  int wfd;
  char *buf;
  int size;
  int len;
  int eof;
}
iopipe_t;

typedef struct client_def_t client_t;

typedef struct
{
  ev_io watcher;
  client_t *client;
}
ev_io_client;

struct client_def_t
{
  char            buf[ BUFSIZE ];
  int             active;
  iopipe_t        iopipe;
  ev_io_client    ev_reader;
  ev_io_client    ev_writer;
};

static client_t clients[ MAX_CLIENTS ];

/*  */

client_t* new_client( client_t *clients );
int free_client( client_t *clients, client_t *client );
int add_client( struct ev_loop *loop, client_t *clients, int rfd, int wfd );
int add_unix_listener( struct ev_loop *loop, ev_io* ev_listener, const char *sockpath );

/* libev event callbacks */

void on_reader(struct ev_loop *loop, ev_io *watcher, int revents);
void on_writer(struct ev_loop *loop, ev_io *watcher, int revents);
void on_unix_listener(struct ev_loop *loop, ev_io *watcher, int revents);
void on_timeout(struct ev_loop *loop, ev_timer *watcher, int revents);



void on_reader(struct ev_loop *loop, ev_io *watcher, int revents)
{
  client_t *client = ((ev_io_client*)watcher)->client;
  iopipe_t *x = &client->iopipe;

  fprintf( stderr, "fd %d readable\n", x->rfd );

  int oldlen = x->len;
  ssize_t bytes;

  if ((bytes = read(x->rfd, x->buf+x->len, x->size-x->len)) < 0)
    strerr_diesys( "read error" );

  x->len += bytes;
  
  if (bytes == 0)
    x->eof = 1;

  fprintf( stderr, "read %u bytes, len = %u\n", bytes, x->len );

  if (x->len == x->size || x->eof)
    ev_io_stop( loop, watcher );
  if (x->len > 0 && !oldlen)
    ev_io_start( loop, (ev_io*)&client->ev_writer );
}


void on_writer(struct ev_loop *loop, ev_io *watcher, int revents)
{
  client_t *client = ((ev_io_client*)watcher)->client;
  iopipe_t *x = &client->iopipe;

  fprintf( stderr, "fd %d writable\n", x->wfd );

  int oldlen = x->len;
  ssize_t bytes;

  if ((bytes = write(x->wfd, x->buf, x->len)) < 0)
    strerr_diesys( "write error" );

  int left = x->len - bytes;

  if (bytes > 0 && left > 0)
    memmove( x->buf, x->buf+bytes, left );

  x->len = left;

  fprintf( stderr, "wrote %u bytes, len = %u\n", bytes, x->len );

  if (x->len == 0) {
    ev_io_stop( loop, watcher );
    if (x->eof) free_client( clients, client );
  }
  if (!x->eof && x->len < x->size && oldlen == x->size)
    ev_io_start( loop, (ev_io*)&client->ev_reader );
}


client_t* new_client( client_t *clients )
{
  int i;
  client_t *c;

  for (i=0, c=clients; i<MAX_CLIENTS; i++, c++)
    if (!c->active) {
      c->active = 1;
      return c;
    }

  return NULL;
}


int free_client( client_t *clients, client_t *client )
{
  int i;
  client_t *c;

  for (i=0, c=clients; i<MAX_CLIENTS; i++, c++)
    if (c == client) {
      close( c->iopipe.rfd );
      close( c->iopipe.wfd );
      memset( c, 0, sizeof *c );
      return 0;
    }

  return -1;
}


int add_client( struct ev_loop *loop, client_t *clients, int rfd, int wfd )
{
  client_t *client;
  client = new_client( clients );

  if (!client)
    return -1;

  iopipe_t *iopipe = &client->iopipe;

  iopipe->rfd = rfd;
  iopipe->wfd = wfd;
  iopipe->buf = client->buf;
  iopipe->size = sizeof client->buf;
  iopipe->len = 0;
  iopipe->eof = 0;

  ev_io_client *ev_reader = &client->ev_reader;
  ev_io_client *ev_writer = &client->ev_writer;
  ev_reader->client = client;
  ev_writer->client = client;

  ev_init( (ev_io*)ev_reader, on_reader );
  ev_io_set( (ev_io*)ev_reader, rfd, EV_READ );
  ev_io_start( loop, (ev_io*)ev_reader );

  ev_init( (ev_io*)ev_writer, on_writer );
  ev_io_set( (ev_io*)ev_writer, wfd, EV_WRITE );

  return 0;
}


int add_unix_listener( struct ev_loop *loop, ev_io* ev_listener, const char *sockpath )
{
  int listenfd;

  struct sockaddr_un addr;
  memset( &addr, 0, sizeof addr );
  addr.sun_family = AF_UNIX;
  strncpy( addr.sun_path, sockpath, sizeof(addr.sun_path)-1 );

  unlink( sockpath ); /* don't care if this fails */

  if ((listenfd = socket( AF_UNIX, SOCK_STREAM, 0 )) < 0)
    strerr_diesys("socket error");
  if (bind(listenfd, (struct sockaddr*)&addr, sizeof addr) < 0)
    strerr_diesys("bind error");
  if (listen(listenfd, MAX_CLIENTS-1) < 0)
    strerr_diesys("listen error");

  ev_init( ev_listener, on_unix_listener );
  ev_io_set( ev_listener, listenfd, EV_READ );
  ev_io_start( loop, ev_listener );

  return 0;
}


void on_unix_listener(struct ev_loop *loop, ev_io *watcher, int revents)
{
  int sock;
  struct sockaddr_un addr;
  socklen_t addr_len = sizeof addr;
  memset( &addr, 0, addr_len );

  if ((sock = accept(watcher->fd, (struct sockaddr*)&addr, &addr_len)) < 0)
    strerr_diesys( "accept error" );

  add_client( loop, clients, sock, sock ); 
}


void on_timeout(struct ev_loop *loop, ev_timer *watcher, int revents)
{
  struct timeval now;
  gettimeofday( &now, 0 );
  fprintf( stderr, "+%lu.%06lu timeout event\n", now.tv_sec, now.tv_usec );
}


int main( int argc, char **argv )
{
  /* Argument processing */

  char *prog = *argv++; argc--;

  if (!argc) {
    fprintf( stderr, "%s: usage: %s sock\n", prog, prog );
    _exit(1);
  }

  char *sockpath = *argv++; argc--;

  /* Event setup */

  struct ev_loop *loop = EV_DEFAULT;

  /* Standard io */

  // TODO use non-blocking
  add_client( loop, clients, 0, 1 );

  /* Unix domain socket listener */

  ev_io ev_listener;
  add_unix_listener( loop, &ev_listener, sockpath );

  /* Timeout */

  ev_timer timer;
  ev_timer_init( &timer, on_timeout, 5., 5. );
  ev_timer_start( loop, &timer );

  /* Enter main event loop */

  ev_loop( loop, 0 );

  /* Cleanup */

  unlink( sockpath );

  return 0;
}

