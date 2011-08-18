TARGET = evtest

LDFLAGS = 
LIBS = -lev

$(TARGET) : evtest.o
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

%.o : %.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean :
	rm -f $(TARGET) *.o

