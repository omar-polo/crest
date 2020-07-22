.POSIX:
EMACS =		emacs

compile: crest4e.elc

clean:
	rm -f *.elc
.PHONY: clean

.SUFFIXES: .el .elc
.el.elc:
	${EMACS} -Q --batch -L . -f batch-byte-compile $<
