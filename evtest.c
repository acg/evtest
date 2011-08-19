#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <ev.h>
#include <stdio.h>
#include <string.h>
#include "ev_iopair.h"
#include "strerr.h"
#include "ndelay.h"

#define MAX_CLIENTS 3
#define BUFSIZE 1024


typedef struct client_def_t client_t;

struct client_def_t
{
  char            rbuf[ BUFSIZE ];
  char            wbuf[ BUFSIZE ];
  ev_iopair_t     ev_iopair;
  int             active;
};

static client_t clients[ MAX_CLIENTS ];


/* client list operations */

client_t* new_client( client_t *clients );
int free_client( client_t *clients, client_t *client );

/* registering event sources with libev */

int add_client( struct ev_loop *loop, client_t *clients, int rfd, int wfd );
int add_unix_listener( struct ev_loop *loop, ev_io* ev_listener, const char *sockpath );

/* libev event callbacks */

void on_unix_listener( struct ev_loop *loop, ev_io *watcher, int revents );
void on_timer( struct ev_loop *loop, ev_timer *watcher, int revents );

/* application handlers */

int on_data( ev_iopair_t* ev_iopair );
int on_command( void *ctx, const char* input, char* output, int *outsize );
int on_close( ev_iopair_t* ev_iopair );


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
      close( c->ev_iopair.rio.fd );
      close( c->ev_iopair.wio.fd );
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

  ev_iopair_t *ev_iopair = &client->ev_iopair;

  ev_iopair_init(
    ev_iopair, loop, client,
    rfd, client->rbuf, sizeof client->rbuf,
    wfd, client->wbuf, sizeof client->wbuf
  );

  ev_iopair->on_data = on_data;
  ev_iopair->on_close = on_close;

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


void on_unix_listener( struct ev_loop *loop, ev_io *watcher, int revents )
{
  int sock;
  struct sockaddr_un addr;
  socklen_t addr_len = sizeof addr;
  memset( &addr, 0, addr_len );

  if ((sock = accept(watcher->fd, (struct sockaddr*)&addr, &addr_len)) < 0)
    strerr_diesys( "accept error" );

  if (ndelay_on( sock ) < 0)
    strerr_diesys( "fcntl error" );

  add_client( loop, clients, sock, sock ); 
}


void on_timer( struct ev_loop *loop, ev_timer *watcher, int revents )
{
  struct timeval now;
  gettimeofday( &now, 0 );
  fprintf( stderr, "[%lu.%06lu] timer event\n", now.tv_sec, now.tv_usec );
}


int on_data( ev_iopair_t* ev_iopair )
{
  iobuf_t *rio = &ev_iopair->rio;
  iobuf_t *wio = &ev_iopair->wio;

  /* Parse and respond to line-delimited requests. */

  char *p;

  while (p = memchr( rio->buf, '\n', rio->len ))
  {
    *p++ = '\0';

    char *input = rio->buf;
    char *output = wio->buf + wio->len;
    size_t outsize = wio->size - wio->len;
    size_t oldsize = outsize;
    int rc;

    rc = on_command( ev_iopair->ctx, input, output, &outsize );

    ssize_t nread = p - input;
    ssize_t nwritten = oldsize - outsize;

    rio->on_data_removed( rio, nread );

    /* on_command failed, or nothing written for some other reason */

    if (rc || nwritten <= 0)
      continue;

    /* Turn null response terminator into newline */

    *(wio->buf + wio->len + nwritten - 1) = '\n';

    rio->on_data_added( wio, nwritten );
  }

  return 0;
}


int on_command( void *ctx, const char* input, char* output, int *outsize )
{
  const char *response = "";

  if (0 == strcmp( "hello", input ))
    response = "world";
  else
    response = "error";

  size_t len = strlen( response );

  if (len+1 > *outsize)
    return -1;

  strcpy( output, response );
  *outsize -= len+1;

  return 0;
}


int on_close( ev_iopair_t* ev_iopair )
{
  client_t *client = (client_t*)ev_iopair->ctx;
  free_client( clients, client );
  return 0;
}


int main( int argc, char **argv )
{
  int i;

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

  for (i=0; i<2; i++)
    if (ndelay_on( i ) < 0)
      strerr_diesys( "fcntl error" );

  add_client( loop, clients, 0, 1 );

  /* Unix domain socket listener */

  ev_io ev_listener;
  add_unix_listener( loop, &ev_listener, sockpath );

  /* Timeout */

  ev_timer timer;
  ev_timer_init( &timer, on_timer, 5., 5. );
  ev_timer_start( loop, &timer );

  /* Enter main event loop */

  ev_loop( loop, 0 );

  /* Cleanup */

  unlink( sockpath );

  return 0;
}

