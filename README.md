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

At the moment the methods supported are delete, get, head, options
and post.

Headers can be added and removed with the `add` and `del` commands:

	# add a custom header, just like the -H flag
	add Accept: text/plain
	
	# remove that header
	del Accept

`crest` has also some options that can be changed at runtime.  In a
previous example I used `set prefix localhost:8080` to set the option
`prefix` (equivalent to the `-p` flag by the way.)  There are other
options documented in the manpage like the HTTP version, the port and
so on.  The `show` command is used to print the value of an option and
`unset` will reset that option.

Lastly, `crest` has `sh(1)`-like pipes:

	get /user/5
	
	# this will execute jid with the output of the last
	# command as input
	|jid

(be sure to try [jid][jid] if you're working with json APIs: the whole
idea of pipes was implemented just to leverage jid ability to dig into
complex json.)

### Building

Make sure you have `libcurl` installed (you may need a package called
libcurl-dev, libcurl-devel or curl-dev), that's the only dependency.

To build the project you'll need `meson`:

	meson build
	cd build
	ninja
	ninja install # optional

or more simply:

	meson install

The install target will add only two files to your system: `crest`
(the executable) and `crest.1` (the man page.)

### Architecture

`crest` will `fork(2)` as soon as it can: the parent process will do
user interaction (reading commands, parsing, executing pipes) while the
child process will do the HTTP calls.  On OpenBSD the processes are
`pledge(2)`ed and the child is also `unveil(2)`ed (it needs to open
files in /etc/ssl to verify TLS certificates.)

The communication between the parent and the child is done through
OpenBSD' [imsg][imsg] functions (they're bundled in the `compat`
directory.)

### TODOs

random order:

 - [x] split the code into two processes and pledge(2) 'em
 - [ ] CONNECT, PATCH, PUT & TRACE.
 - [x] DELETE, OPTIONS
 - [x] flag to define headers
 - [x] flag to choose the default HTTP version
 - [ ] flag to hide/blacklist some/all response header
 - [x] `set` command to change some parameters at runtime
 - [x] document the set command
 - [x] document that it will execute file passed as arguments
 - [x] `del` command to delete HTTP headers
 - [ ] encoding & print (not so sure about this)
 - [ ] cookie support (not so sure about this)
 - [ ] support response bigger than UINT16_MAX bytes
 - [x] write a nice manpage
 - [ ] add syntax to define field
 - [ ] add syntax to help with managing json?
 - [ ] use yacc to parse?

### License

All the code is released under the OpenBSD license, with the only
exception being `compat/queue.h` and `compat/vis.{c,h}` that are under
BSD 3-clause.  All copyright notices are in the first lines of each
source file.

[imsg]: http://man.openbsd.org/imsg_init
[jid]: https://github.com/simeji/jid
[manpage]: crest.1.md
