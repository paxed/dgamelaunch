VERSION = 1.4.3
# Hardcoded path to config file
DEFCONFIG = /etc/dgamelaunch.conf
NAME = dgamelaunch
exclusions = CVS .svn .cvsignore tags
PREFIX = /usr
SBINDIR = $(PREFIX)/sbin
ifeq (PREFIX,/usr)
  MANDIR = $(PREFIX)/share/man
else
  MANDIR = $(PREFIX)/man
endif
MAN8 = dgamelaunch.8

ifndef optimize
  optimize = -O0
endif

ifneq (,$(shell which flex 2>/dev/null))
  LEX = flex
else
  LEX = lex
endif

ifneq (,$(shell which bison 2>/dev/null))
  YACC = bison -y
else
  YACC = yacc
endif

ifeq (Linux,$(shell uname -s))
  LUTIL = -lutil
else
  ifeq (BSD,$(shell uname -s | sed -e 's/.*BSD/BSD/g'))
    LUTIL = -lutil
  endif
endif

ifeq (1,$(VIRUS))
  EDITOR = virus.c
else
  EDITOR = ee.c
endif

CC = gcc
LDFLAGS = 
CFLAGS = -g3 $(optimize) -Wall -Wno-unused $(DEFS)
INSTALL = install -c
DEFS = -DVERSION=\"$(VERSION)\" -DDEFCONFIG=\"$(DEFCONFIG)\"
SRCS = $(EDITOR) dgl-common.c ttyrec.c dgamelaunch.c io.c ttyplay.c mygetnstr.c stripgfx.c strlcpy.c strlcat.c y.tab.c lex.yy.c
EXTRA_SRCS = nethackstub.c
OBJS = $(SRCS:.c=.o)
WALL_OBJS = y.tab.o lex.yy.o dgl-common.o dgl-wall.o strlcat.o strlcpy.o
LIBS = -lcurses -lcrypt $(LUTIL) -ll

all: $(NAME) dgl-wall

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

dgl-wall: $(WALL_OBJS)
	$(CC) $(CFLAGS) -o $@ $(WALL_OBJS) $(LIBS)
	
clean:
	rm -f $(NAME) nethackstub dgl-wall
	rm -f editors/*.o *.o .#* *~ y.tab.* lex.yy.c Makefile.dep
	
install:
	$(INSTALL) -m 755 $(NAME) $(SBINDIR)
	$(INSTALL) -m 644 $(MAN8) $(MANDIR)/man8
	
indent:
	indent -nut -ts2 *.c *.h
	rm -f *~

lex.yy.c: config.l
	$(LEX) $<

y.tab.c y.tab.h: config.y
	$(YACC) -d $<

lex.yy.o: lex.yy.c
y.tab.o: y.tab.c

dist: dep clean
	rm -rf $(NAME)-$(VERSION)
	(cd .. && ln -sf $(CURDIR) $(NAME)-$(VERSION))
	(cd .. && tar $(addprefix --exclude ,$(exclusions)) -chzf $(NAME)-$(VERSION).tar.gz $(NAME)-$(VERSION))
	rm -f ../$(NAME)-$(VERSION)
	@echo "Created source release $(NAME)-$(VERSION).tar.gz"

dep: y.tab.c lex.yy.c
	@sed -e '/^# Source code dependencies/,$$d' < Makefile > Makefile.dep
	@echo "# Source code dependencies" >> Makefile.dep
	$(CC) -MM $(SRCS) $(EXTRA_SRCS) >> Makefile.dep
	mv Makefile.dep Makefile

# Source code dependencies
ee.o: ee.c
dgl-common.o: dgl-common.c dgamelaunch.h
ttyrec.o: ttyrec.c dgamelaunch.h ttyrec.h io.h
dgamelaunch.o: dgamelaunch.c dgamelaunch.h ttyplay.h ttyrec.h y.tab.h
io.o: io.c ttyrec.h
ttyplay.o: ttyplay.c dgamelaunch.h ttyplay.h ttyrec.h io.h stripgfx.h
mygetnstr.o: mygetnstr.c
stripgfx.o: stripgfx.c stripgfx.h
strlcpy.o: strlcpy.c
strlcat.o: strlcat.c
y.tab.o: y.tab.c dgamelaunch.h
lex.yy.o: lex.yy.c y.tab.h dgamelaunch.h
nethackstub.o: nethackstub.c
