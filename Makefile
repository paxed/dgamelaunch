VERSION = 1.3.10.1
NAME = dgamelaunch
exclusions = CVS .cvsignore tags

ifndef optimize
  optimize = -O0
endif

CC = gcc
LDFLAGS = 
CFLAGS = -g3 $(optimize) -Wall $(DEFS)
DEFS = -DVERSION=\"$(VERSION)\"
SRCS = virus.c ttyrec.c dgamelaunch.c io.c ttyplay.c stripgfx.c
OBJS = $(SRCS:.c=.o)
LIBS = -lncurses -lcrypt -lutil

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f dgamelaunch
	rm -f *.o .#* *~
	
install:
	cp dgamelaunch /usr/sbin
	
indent:
	indent -nut -ts2 *.c *.h
	rm -f *~

dist: clean indent
	rm -rf $(NAME)-$(VERSION)
	(cd .. && ln -sf $(CURDIR) $(NAME)-$(VERSION))
	(cd .. && tar $(addprefix --exclude ,$(exclusions)) -chzf $(NAME)-$(VERSION).tar.gz $(NAME)-$(VERSION))
	rm -f ../$(NAME)-$(VERSION)
	@echo "Created source release $(NAME)-$(VERSION).tar.gz"
	
# Dependencies - we may auto-generate later
dgamelaunch.o: dgamelaunch.c dgamelaunch.h
io.o: io.c ttyrec.h
last_char_is.o: last_char_is.c
stripgfx.o: stripgfx.c stripgfx.h
ttyplay.o: ttyplay.c ttyrec.h io.h stripgfx.h
ttyrec.o: ttyrec.c dgamelaunch.h ttyrec.h io.h
virus.o: virus.c last_char_is.c
