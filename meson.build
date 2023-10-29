project('fftools-ffi', 'c', license: ['LGPL2.1+'])

pkg = import('pkgconfig')

deps = [
	dependency('libavformat'),
	dependency('libavcodec'),
	dependency('libavutil'),
	dependency('libavfilter'),
	dependency('libswresample'),
	dependency('libswscale')
]

if host_machine.system() == 'android'
	deps += meson.get_compiler('c').find_library('android', has_headers: ['android/log.h'], required: true)
	deps += meson.get_compiler('c').find_library('log', has_headers: ['android/log.h'], required: true)
endif

sources = [
	'cmdutils.c',
	'dart_api.c',
	'ffmpeg_hw.c',
	'ffmpeg_opt.c',
	'ffmpeg.c',
	'ffmpeg_filter.c',
	'ffmpeg_demux.c',
	'ffmpeg_mux.c',
	'ffmpeg_mux_init.c',
	'ffprobe.c',
	'fftools.c',
	'objpool.c',
	'opt_common.c',
	'sync_queue.c',
	'thread_queue.c'
]

lib = library('fftools-ffi', sources, dependencies: deps, install: true)
ffprobe = executable('ffprobe', ['ffprobe_cmd.c'], dependencies: deps, link_with: [
	lib
])
ffmpeg = executable('ffmpeg', ['ffmpeg_cmd.c'], dependencies: deps, link_with: [
	lib
])

install_headers(['dart_api.h'], subdir: 'fftools-ffi')

pkg.generate(libraries : [lib],
             subdirs : ['.'],
             version : '1.0',
             name : 'libfftools-ffi',
             filebase : 'fftools-ffi',
             description : 'FFTools front-end as a library')