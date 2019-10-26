# crest

`crest` is a simple program to perform HTTP requests.  Its main purpose
is to explore and use HTTP API from the command line, with a non-verbose
syntax and being not resource-heavy.

### Usage

(for a longer and cleaner description you should check out the
[manpage][manpage].)

`crest` uses a simple language to perform HTTP requests.  The syntax is
`http-verb url payload` where the payload can be absent.  For example:

	get localhost:8080/foo

To avoid typing possibly multiple times the same part of an URL `crest`
has the concept of *prefix*.  The prefix is a string that is appended
before every URL in a way that is guaranteed that a single `/` will
separate the prefix and the given URL.

	set prefix localhost:8080/
	
	# this is now equivalent to `get localhost:8080/foo`
	get foo
	
	# this is also equivalent
	get /foo

At the moment the method supported are delete, get, head, options
and post.

`crest` has also some options that can be changed at runtime.  In the
previous example I used `set prefix localhost:8080` to set the option
`prefix` (that is equilent to set the `-p` flag by the way.)  There are
other options documented in the manpage, like the HTTP version, the port
and so on.

Lastly, the `show` command is used to print the value of an option.

### Building

Make sure you have `libcurl` installed (you may need a package called
libcurl-dev or libcurl-devel), that's the only dependency.

To build the project you only need `make(1)`.  Both GNU make and BSD
make were tested.

To install, execute `make install`.  The binary will be placed at
`${PREFIX}/bin`, with `${PREFIX}` being `/usr/local/` by default.
The manpage will be installed at `${PREFIX}/man/man1`.

To uninstall, execute `make uninstall` or simply remove the binary and
the manpage.  No other files will be installed.

The makefile will honor `${PREFIX}` and `${DESTDIR}`.

### TODOs

random order:

 - [x] split the code into two processes and pledge(3) 'em
 - [ ] CONNECT, PATCH, PUT & TRACE.
 - [x] DELETE, OPTIONS
 - [x] flag to define headers
 - [x] flag to choose the default HTTP version
 - [ ] flag (or something else) to hide/blacklist some/all response
       header
 - [x] `set' command to change some parameters at runtime
 - [x] document the set command
 - [x] document that it will execute file passed as arguments
 - [ ] `add'/`del' command (or better terms?) to add/delete custom
       HTTP headers
 - [ ] encoding & print (not so sure about this)
 - [ ] cookie support (not so sure about this)
 - [ ] support response bigger than UINT16_MAX bytes
 - [x] write a nice manpage

### License

All the code is released under the OpenBSD license, with the only
exception being `compat/queue.h` and `compat/vis.{c,h}` that are under
BSD 3-clause.  All copyright notices are in the first lines of each
source file.

[manpage]: crest.1.md
