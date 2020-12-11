CC=gcc
CCOPTS=-I /usr/include/libevdev-1.0 -Wall
LD=gcc
LDOPTS=-l evdev -l pthread -l m

OBJS=main.o io.o logging.o ports.o input.o

.c.o:
	$(CC) -c $(CCOPTS) $<

all: $(OBJS)
	$(LD) -o joyemu $(LDOPTS) $(OBJS)
	
clean:
	rm -f *~ *.o joyemu

