CC=gcc
FUSE_CFLAGS=-D_FILE_OFFSET_BITS=64
CFLAGS=-Wall -g -std=gnu99 -DDEBUG $(FUSE_CFLAGS)
EXEC=ti_cat cksum
LDFLAGS=-pthread -lfuse

all: $(EXEC)

ti_cat: ti_cat.o teleinfo.o
	$(CC) -o $@ $^ $(LDFLAGS)

cksum: cksum.o
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

clean:
	rm -f *.o

mrproper: clean
	rm -f $(EXEC) *~
