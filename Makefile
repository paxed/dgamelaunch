VERSION = 1.4.1
# Hardcoded path to config file
DEFCONFIG = /etc/dgamelaunch.conf
NAME = dgamelaunch
exclusions = CVS .svn .cvsignore tags
PREFIX = /usr
SBINDIR = $(PREFIX)/sbin

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
SRCS = $(EDITOR) ttyrec.c dgamelaunch.c io.c ttyplay.c mygetnstr.c stripgfx.c strlcpy.c strlcat.c y.tab.c lex.yy.c
OBJS = $(SRCS:.c=.o)
LIBS = -lcurses -lcrypt $(LUTIL) -ll

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f $(NAME) nethackstub
	rm -f editors/*.o *.o .#* *~ y.tab.* lex.yy.c
	
install:
	$(INSTALL) -m 755 $(NAME) $(SBINDIR)
	
indent:
	indent -nut -ts2 *.c *.h
	rm -f *~

lex.yy.c: config.l
	$(LEX) $<

y.tab.c y.tab.h: config.y
	$(YACC) -d $<

lex.yy.o: lex.yy.c
y.tab.o: y.tab.c

dist: clean
	rm -rf $(NAME)-$(VERSION)
	(cd .. && ln -sf $(CURDIR) $(NAME)-$(VERSION))
	(cd .. && tar $(addprefix --exclude ,$(exclusions)) -chzf $(NAME)-$(VERSION).tar.gz $(NAME)-$(VERSION))
	rm -f ../$(NAME)-$(VERSION)
	@echo "Created source release $(NAME)-$(VERSION).tar.gz"
	
# Dependencies - we may auto-generate later
ee.o: ee.c
io.o: io.c ttyrec.h
last_char_is.o: last_char_is.c
mygetnstr.o: mygetnstr.c
nethackstub.o: nethackstub.c
stripgfx.o: stripgfx.c stripgfx.h
strlcat.o: strlcat.c
strlcpy.o: strlcpy.c
ttyplay.o: ttyplay.c dgamelaunch.h ttyplay.h ttyrec.h io.h stripgfx.h
ttyrec.o: ttyrec.c dgamelaunch.h ttyrec.h io.h
virus.o: virus.c last_char_is.c
y.tab.o: y.tab.c dgamelaunch.h
dgamelaunch.o: dgamelaunch.c dgamelaunch.h ttyplay.h ttyrec.h y.tab.h
