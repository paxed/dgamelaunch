CC = gcc
LDFLAGS = 
CFLAGS = -g3 -O0 -Wall -W -Wno-unused-parameter
SRCS = virus.c ttyrec.c dgamelaunch.c io.c ttyplay.c stripgfx.c
OBJS = $(SRCS:.c=.o)
LIBS = -lncurses -lcrypt

all: dgamelaunch

dgamelaunch: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f dgamelaunch
	rm -f *.o
install:
	cp dgamelaunch /opt/nethack/nethack.dtype.org/
indent:
	indent -ts2 *.c *.h
	rm *~
release: clean indent
