VERSION = 1.4
NAME = dgamelaunch
exclusions = CVS .cvsignore tags
PREFIX = /usr
SBINDIR = $(PREFIX)/sbin

ifndef optimize
  optimize = -O0
endif

ifeq (Linux,$(shell uname -s))
  LUTIL = -lutil
else
  ifeq (BSD,$(shell uname -s | grep -o BSD))
    LUTIL = -lutil
  endif
endif

CC = gcc
LDFLAGS = 
CFLAGS = -g3 $(optimize) -Wall -Wno-unused $(DEFS)
INSTALL = install -c
DEFS = -DVERSION=\"$(VERSION)\"
SRCS = virus.c ttyrec.c dgamelaunch.c io.c ttyplay.c mygetnstr.c stripgfx.c strlcpy.c strlcat.c y.tab.c lex.yy.c
OBJS = $(SRCS:.c=.o)
LIBS = -lcurses -lcrypt $(LUTIL) -ll

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f $(NAME)
	rm -f *.o .#* *~ y.tab.* lex.yy.c
	
install:
	$(INSTALL) -m 755 $(NAME) $(SBINDIR)
	
indent:
	indent -nut -ts2 *.c *.h
	rm -f *~

lex.yy.c: config.l
	lex $<

y.tab.c: config.y
	yacc -d $<

lex.yy.o: lex.yy.c
y.tab.o: y.tab.c

dist: clean
	rm -rf $(NAME)-$(VERSION)
	(cd .. && ln -sf $(CURDIR) $(NAME)-$(VERSION))
	(cd .. && tar $(addprefix --exclude ,$(exclusions)) -chzf $(NAME)-$(VERSION).tar.gz $(NAME)-$(VERSION))
	rm -f ../$(NAME)-$(VERSION)
	@echo "Created source release $(NAME)-$(VERSION).tar.gz"
	
# Dependencies - we may auto-generate later
dgamelaunch.o: dgamelaunch.c dgamelaunch.h y.tab.o
io.o: io.c ttyrec.h
last_char_is.o: last_char_is.c
stripgfx.o: stripgfx.c stripgfx.h
ttyplay.o: ttyplay.c ttyrec.h io.h stripgfx.h
ttyrec.o: ttyrec.c dgamelaunch.h ttyrec.h io.h
virus.o: virus.c last_char_is.c
