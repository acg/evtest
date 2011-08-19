/*
  A libev test program that demos:

     * a unix domain socket listener
     * client connections with a simple request / response protocol
     * treat stdin / stdout as another client channel
     * timer events

  Alan Grow (c) 2011
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <ev.h>
#include <stdio.h>
#include <string.h>
#include "ev_iochannel.h"
#include "strerr.h"
#include "ndelay.h"


#define MAX_CLIENTS 3
#define BUFSIZE 1024


/* A client list */

typedef struct client_def_t client_t;

struct client_def_t
{
  char            rbuf[ BUFSIZE ];
  char            wbuf[ BUFSIZE ];
  ev_iochannel_t  ev_iochannel;
  int             active;
};

/* A static list of clients. Simple, avoids malloc / free. */

static client_t clients[ MAX_CLIENTS ];


/* Client list operations */

client_t* new_client( client_t *clients );
int free_client( client_t *clients, client_t *client );

/* Registering event sources with libev */

int add_client( struct ev_loop *loop, client_t *clients, int rfd, int wfd );
int add_unix_listener( struct ev_loop *loop, ev_io* ev_listener, const char *sockpath );

/* libev event callbacks */

void on_unix_listener( struct ev_loop *loop, ev_io *watcher, int revents );
void on_timer( struct ev_loop *loop, ev_timer *watcher, int revents );

/* Application level event handlers */

int on_data( ev_iochannel_t* ev_iochannel );
int on_command( void *ctx, const char* input, char* output, int *outsize );
int on_close( ev_iochannel_t* ev_iochannel );


/* Find a new free ( = inactive, zero'd out ) slot in the clients array */

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


/* Free a slot in the clients array */

int free_client( client_t *clients, client_t *client )
{
  int i;
  client_t *c;

  for (i=0, c=clients; i<MAX_CLIENTS; i++, c++)
    if (c == client) {
      close( c->ev_iochannel.rio.fd );
      close( c->ev_iochannel.wio.fd );
      memset( c, 0, sizeof *c );
      return 0;
    }

  return -1;
}


/* Create a new client with an event channel around a r/w fd pair.
   Register the event channel with libev. */

int add_client( struct ev_loop *loop, client_t *clients, int rfd, int wfd )
{
  client_t *client;
  client = new_client( clients );

  if (!client)
    return -1;

  ev_iochannel_t *ev_iochannel = &client->ev_iochannel;

  ev_iochannel_init(
    ev_iochannel, loop, client,
    rfd, client->rbuf, sizeof client->rbuf,
    wfd, client->wbuf, sizeof client->wbuf
  );

  ev_iochannel->on_data = on_data;
  ev_iochannel->on_close = on_close;
  ev_iochannel_start( ev_iochannel );

  return 0;
}


/* Register a new unix domain socket listener event source with libev. */

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


/* When the unix domain sock is readable, accept and register a new client. */

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


/* Handle a libev timer event. */

void on_timer( struct ev_loop *loop, ev_timer *watcher, int revents )
{
  struct timeval now;
  gettimeofday( &now, 0 );
  fprintf( stderr, "[%lu.%06lu] timer event\n", now.tv_sec, now.tv_usec );
}


/* New data arrived in the client's event channel.
   Act as a request / response command filter using a line delimited protocol. */

int on_data( ev_iochannel_t* ev_iochannel )
{
  iobuf_t *rio = &ev_iochannel->rio;
  iobuf_t *wio = &ev_iochannel->wio;

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

    rc = on_command( ev_iochannel->ctx, input, output, &outsize );

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


/* Handle a command request, produce a response. */

int on_command( void *ctx, const char* input, char* output, int *outsize )
{
  const char *response = "";

  /* trivial and silly for now */

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


/* Both sides of the client's event channel have closed. */

int on_close( ev_iochannel_t* ev_iochannel )
{
  client_t *client = (client_t*)ev_iochannel->ctx;
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

  /* Timer */

  ev_timer timer;
  ev_timer_init( &timer, on_timer, 5., 5. );
  ev_timer_start( loop, &timer );

  /* Enter main event loop */

  ev_loop( loop, 0 );

  /* Cleanup */

  unlink( sockpath );

  return 0;
}

