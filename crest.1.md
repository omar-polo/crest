CREST(1) - General Commands Manual

# NAME

**crest** - Small repl to test REST endpoints

# SYNOPSYS

**crest**
\[**-i**]
\[**-H**&nbsp;*header*]
\[**-P**&nbsp;*port*]
\[**-V**&nbsp;*http&nbsp;version*]
\[**-c**&nbsp;*jtx*]
\[**-h**&nbsp;*host*]
\[**-p**&nbsp;*prefix*]
\[*file&nbsp;...*]

# DESCRIPTION

**crest**
is an interactive processor to send HTTP requests.  It takes input on
the command line: first it process the files given as argument, then
reads the standard input.

Options available:

**-i**

> do not verify the authenticity of the peer's certificate.

**-H**

> set an extra HTTP header to include in the request

**-P**

> set the port.  It's only necessary to do this explicitly if you want to
> send the requests to a server listening on a non-standard port and you
> don't specify the port implicitly in the url or with a prefix.

**-V**

> set the http version to use.  The possible values are:

> 0

> > for HTTP/1.0

> 1

> > for HTTP/1.1

> 2

> > for HTTP/2

> T

> > for HTTP/2 over TLS only (the default.)

> 3

> > for HTTP/3

> X

> > for "I don't care about the version"

> Please note that
> **crest**
> can (and probably will) fall back to HTTP/1.1 if HTTP2 can't be
> negotiated.  This behavior is due to libcurl, see
> CURLOPT\_HTTP\_VERSION(3)
> for more information.

**-c**

> is a short-hand to declare the Content-Type header.  The possible
> values are:

> j

> > for "Content-Type: application/json"

> t

> > for "Content-Type: text/plain"

> x

> > for "Content-Type: application/xml"

**-h**

> is a hort-hand for the "Host" header

**-p**

> set the the prefix.  The prefix is a string that is appended
> **before**
> every url.  Will be guaranteed that
> **only**
> a slash will be present between the prefix and the url (i.e. if the
> prefix is localhost:8080/ and the url is /foo the final URL will be
> localhost:8080/foo and not localhost:8080//foo)

# SYNTAX

The syntax for
**crest**
is as follows: empty lines are ignored, lines starting with a # are
comments and thus ignored and every other line is a command.
Spaces and tabs are used to delimit commands and their arguments.

The supported commands are:

**show** *opt*

> to show the value of an option.  See the section below for the list
> of options.

**set** *opt* *val*

> to set the value of an option.  See the section below for the list
> of options.

**unset** *opt*

> unset an option.

**add** *header*

> to add a custom header

**del** *header*

> to delete a header previously added with add

**quit**, **exit**

> to exit.

**help**, **usage**

> to show a brief help.

|**cmd**

> where cmd is a
> sh(1)
> command.  It will invoke the cmd in a shell binding its standard input
> to the body of the previously HTTP response.

*verb* **url** \[*payload*]

> perform an HTTP request.
> *verb*
> is one of
> **delete**,
> **get**,
> **head**,
> **options** or
> **post**.
> If the
> **prefix**
> is defined, the URL will be prefixed such that only a single slash will
> be present between the prefix and the given URL.  An optional payload
> can be provided and will be sended as-is.  Keep in mind that for some
> HTTP method the payload has not defined semantic (see RFC 7231.)

# OPTIONS

The following options are available for the
**set** and **show** commands:

**headers**

> Read only.  All the headers added.

**useragent**

> The value for the user agent.

**prefix**

> The prefix

**http**

> The HTTP version.  Accepted values are:

> 1\.0

> 1\.1

> 2

> 2TLS

> > means HTTP/2 only through TLS.  Can degrade to HTTP/1.1.  This is the
> > default value.

> 3

> > for HTTP/3.

> none

> > to let libcurl choose the version by itself.

**http-version**

> Alias for
> **http**

**port**

> The port

**peer-verification**

> Enable or disable the verification of the peer certificate.  Accepted
> values are
> *on* or *true*
> to enable it or
> *off* or *false* to disable it. Defaults to
> *on*.

# SEE ALSO

curl(1)

# AUTHORS

Omar Polo &lt;op@xglobe.in&gt;

# CAVEATS

*	The headers and bodies of the replies will be passed through
	vis(3)
	before they're printed to the user.  This is to ensure that no
	"funny"
	sequences of character can mess up the user terminal.  If you want to
	obtain the raw body you can use the pipe command (i.e. |cat should print
	the last body as-is to standard output.)

Void Linux - October 24, 2019
