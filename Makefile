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
SRCS = virus.c ttyrec.c dgamelaunch.c io.c ttyplay.c stripgfx.c strlcpy.c strlcat.c y.tab.o lex.yy.o
OBJS = $(SRCS:.c=.o)
LIBS = -lncurses -lcrypt -lutil

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f dgamelaunch
	rm -f *.o .#* *~ y.tab.* lex.yy.c
	
install:
	cp dgamelaunch /usr/sbin
	
indent:
	indent -nut -ts2 *.c *.h
	rm -f *~

lex.yy.c: config.l
	flex $<

y.tab.c: config.y
	bison -d -y $<

lex.yy.o: lex.yy.c
y.tab.o: y.tab.c

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
