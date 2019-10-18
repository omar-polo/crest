CC	?= cc
CFLAGS	 = `pkg-config --cflags libcurl` -g -D_GNU_SOURCE -D_DEFAULT_SOURCE -Icompat
LDFLAGS	 = `pkg-config --libs   libcurl`

.PHONY: all clean

all: crest

clean:
	rm -f *.o compat/*.o crest

OBJS	 = main.o repl.o io.o parse.o http.o svec.o
COMPAT	 = compat/imsg.o compat/imsg-buffer.o compat/freezero.o \
		compat/recallocarray.o compat/vis.o

${OBJS}: crest.h

crest: ${OBJS} ${COMPAT}
	${CC} ${OBJS} ${COMPAT} -o $@ ${LDFLAGS}

.SUFFIXES: .c .o
.c.o:
	${CC} ${CFLAGS} -c $< -o $@
