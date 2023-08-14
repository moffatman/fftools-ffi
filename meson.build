project('ffmpeg-for-ffi', 'c', license: ['LGPL2.1+'])

ffmpeg = [
	dependency('libavformat'),
	dependency('libavcodec'),
	dependency('libavutil'),
	dependency('libavfilter'),
	dependency('libavdevice'),
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
	#'ffprobe.c',
	'objpool.c',
	'opt_common.c',
	'sync_queue.c',
	'thread_queue.c'
]

lib = library('ffmpeg-for-ffi', sources, dependencies: ffmpeg, install: true)