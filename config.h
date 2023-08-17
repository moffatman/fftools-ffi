#ifndef FFMPEG_CONFIG_H
#define FFMPEG_CONFIG_H
#define FFMPEG_CONFIGURATION ""
#define FFMPEG_LICENSE "LGPL version 2.1 or later"
#define CONFIG_THIS_YEAR 2023
#define CONFIG_GPL 0
#define CONFIG_NONFREE 0
#define CONFIG_VERSION3 0
#define CONFIG_AVDEVICE 0
#define CONFIG_AVFILTER 1
#define CONFIG_SWSCALE 1
#define CONFIG_POSTPROC 0
#define CONFIG_AVFORMAT 1
#define CONFIG_AVCODEC 1
#define CONFIG_SWRESAMPLE 1
#define CONFIG_AVUTIL 1
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
	#define HAVE_W32THREADS 1
#elif __APPLE__ || __ANDROID__ || __linux__ || __unix__ || defined(_POSIX_VERSION)
    #define HAVE_PTHREADS 1
#endif
#endif /* FFMPEG_CONFIG_H */
