CC	?= cc
CFLAGS	 = `pkg-config --cflags libcurl` -g -D_GNU_SOURCE
LDFLAGS	 = `pkg-config --libs   libcurl`

.PHONY: all clean

all: crest

clean:
	rm -f *.o crest

OBJS	 = main.o repl.o io.o parse.o http.o
${OBJS}: crest.h

crest: ${OBJS}
	${CC} ${OBJS} -o $@ ${LDFLAGS}

.SUFFIXES: .c .o
.c.o:
	${CC} ${CFLAGS} -c $< -o $@
