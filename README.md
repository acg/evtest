# evtest - a libev demo program #

This is a [libev](http://software.schmorp.de/pkg/libev.html) test program in ANSI C that demos:

* a unix domain socket listener
* client connections with a simple request / response protocol
* treating stdin / stdout as another client channel
* timer events

Batteries that are included:

* `ev_iochannel` - a `libev` producer / consumer pair
* `ev_iobuf` - fill and drain an `iobuf` on libev read / write events
* `iobuf` - an interface for buffered io on a file descriptor

