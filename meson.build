project('fftools-ffi', 'c', license: ['LGPL2.1+'])

ffmpeg = [
	dependency('libavformat'),
	dependency('libavcodec'),
	dependency('libavutil'),
	dependency('libavfilter'),
	dependency('libswresample'),
	dependency('libswscale')
]

sources = [
	'cmdutils.c',
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

lib = library('fftools-ffi', sources, dependencies: ffmpeg, install: true)
ffprobe = executable('ffprobe', ['ffprobe_cmd.c'], link_with: [
	lib
])
ffmpeg = executable('ffmpeg', ['ffmpeg_cmd.c'], link_with: [
	lib
])