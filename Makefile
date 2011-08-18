TARGET = evtest

LDFLAGS = 
LIBS = -lev

$(TARGET) : evtest.o ndelay_on.o
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

%.o : %.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean :
	rm -f $(TARGET) *.o

