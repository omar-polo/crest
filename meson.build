project('crest', 'c',
	version : '0.1.0',
	default_options : ['warning_level=3'])

cc = meson.get_compiler('c')
ldflags = []

conf = configuration_data()

src = ['main.c', 'repl.c', 'io.c', 'parse.c', 'http.c',
	'svec.c', 'child.c']

deps = [dependency('libcurl')]

if get_option('enable_readline')
	# TODO: find out why readline isn't found on OpenBSD
	deps += cc.find_library('readline')
	conf.set('HAVE_READLINE', 1)
else
	conf.set('HAVE_READLINE', 0)
endif

conf.set10('HAVE_U_CHAR', cc.has_type('u_char', prefix : '#include <sys/types.h>'))

if not cc.has_function('getdtablecount')
	src += 'compat/getdtablecount.c'
	conf.set10('HAVE_GETDTABLECOUNT', 0)
else
	conf.set10('HAVE_GETDTABLECOUNT', 1)
endif

if not cc.has_function('freezero')
	src += 'compat/freezero.c'
	conf.set('HAVE_FREEZERO', 0)
else
	conf.set('HAVE_FREEZERO', 1)
endif

if not cc.has_function('recallocarray')
	src += 'compat/recallocarray.c'
	conf.set('HAVE_RECALLOCARRAY', 0)
else
	conf.set('HAVE_RECALLOCARRAY', 1)
endif

if not cc.has_function('imsg_init', args : '-lutil')
	src += ['compat/imsg.c', 'compat/imsg-buffer.c']
	conf.set('HAVE_IMSG', 0)
else
	ldflags += '-lutil'
	conf.set('HAVE_IMSG', 1)
endif

if not cc.has_function('strtonum')
	src += 'compat/strtonum.c'
	conf.set('HAVE_STRTONUM', 0)
else
	conf.set('HAVE_STRTONUM', 1)
endif

if not cc.has_header('vis.h')
	src += 'compat/vis.c'
	conf.set('HAVE_VIS_H', 0)
else
	conf.set('HAVE_VIS_H', 1)
endif

if not cc.has_header('sys/queue.h')
	conf.set('HAVE_QUEUE_H', 0)
else
	conf.set('HAVE_QUEUE_H', 1)
endif

if not cc.has_function('err')
	src += 'compat/err.c'
	conf.set('HAVE_ERR', 0)
else
	conf.set('HAVE_ERR', 1)
endif

configure_file(
	input         : 'config.h.in',
	output        : 'config.h',
	configuration : conf
)

crest = executable('crest',
	sources      : src,
	install      : true,
	dependencies : deps,
	link_args    : ldflags
)

install_man('crest.1')
