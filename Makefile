VERSION = 1.3.10.1
NAME = dgamelaunch
exclusions = CVS .cvsignore

ifndef optimize
  optimize = -O0
endif

CC = gcc
LDFLAGS = 
CFLAGS = -g3 $(optimize) -Wall -W -Wno-unused-parameter $(DEFS)
DEFS = -DVERSION=\"$(VERSION)\"
SRCS = virus.c ttyrec.c dgamelaunch.c io.c ttyplay.c stripgfx.c
OBJS = $(SRCS:.c=.o)
LIBS = -lncurses -lcrypt

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f dgamelaunch
	rm -f *.o .#*
install:
	cp dgamelaunch /usr/sbin
indent:
	indent -ts2 *.c *.h
	rm *~

dist: clean indent
	rm -rf $(NAME)-$(VERSION)
	(cd .. && ln -sf $(CURDIR) $(NAME)-$(VERSION))
	(cd .. && tar $(addprefix --exclude ,$(exclusions)) -chzf $(NAME)-$(VERSION).tar.gz $(NAME)-$(VERSION))
	rm -f ../$(NAME)-$(VERSION)
	@echo "Created source release $(NAME)-$(VERSION).tar.gz"
	
