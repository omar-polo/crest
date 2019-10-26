CC	?= cc
CFLAGS	 = `pkg-config --cflags libcurl` -g -D_GNU_SOURCE -Icompat -Wall
LDFLAGS	 = `pkg-config --libs   libcurl`
PREFIX	?= /usr/local

.PHONY: all clean install uninstall

all: crest

clean:
	rm -f *.o compat/*.o crest

OBJS	 = main.o repl.o io.o parse.o http.o svec.o child.o
COMPAT	 = compat/imsg.o compat/imsg-buffer.o compat/freezero.o \
		compat/recallocarray.o compat/vis.o		\
		compat/strtonum.o

${OBJS}: crest.h

crest: ${OBJS} ${COMPAT}
	${CC} ${OBJS} ${COMPAT} -o $@ ${LDFLAGS}

.SUFFIXES: .c .o
.c.o:
	@echo '  CC	$<'
	@${CC} ${CFLAGS} -c $< -o $@

# -D is non standard but accepted by GNU and OpenBSD install(1).
install: crest
	install -Dm 0755 crest ${DESTDIR}${PREFIX}/bin
	install -Dm 0644 crest.1 ${DESTDIR}${PREFIX}/man/man1

uninstall:
	rm ${DESTDIR}${PREFIX}/bin/crest
	rm ${DESTDIR}${PREFIX}/man/man1/crest.1
