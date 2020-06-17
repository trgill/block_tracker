CC = gcc
CFLAGS = -g -Wall -I/usr/pgsql-12/include
CFILES = block_tracker.c db.c udev.c util.c

blkhistd:
	$(CC) $(CFLAGS) $(CFILES) -o blkhistd -ludev -lpq -ldevmapper

all: default

clean:
	rm -f *.o blkhistd *.db
