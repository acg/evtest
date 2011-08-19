TARGET = evtest

CFLAGS = -g
LDFLAGS = 
LIBS = -lev

OBJECTS = \
	evtest.o \
	ev_iobuf.o \
	ev_iopair.o \
	iobuf.o \
	ndelay_on.o 

$(TARGET) : $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

%.o : %.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean :
	rm -f $(TARGET) *.o

