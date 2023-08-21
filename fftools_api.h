#ifndef FFTOOLS_API_H
#define FFTOOLS_API_H

#include "setjmp.h"
#include "stdint.h"
#include "stdio.h"

#include "cmdutils.h"
#include "thread_queue.h"

typedef struct FFToolsSession {
    ThreadQueue *tq;
    /** Holds information to implement exception handling. */
    jmp_buf ex_buf__;
    int cancel_requested;
    OptionDef *options;
} FFToolsSession;

typedef void (*session_callback_fp)(FFToolsSession* session, void* user_data);
typedef void (*log_callback_fp)(int level, char* message, void* user_data);
typedef void (*statistics_callback_fp)(int frameNumber, float fps, float quality, int64_t size, int time, double bitrate, double speed, void* user_data);

#if defined(__cplusplus)
extern "C" {
#endif

int ffmpeg_execute_with_callbacks(int argc, char **argv, session_callback_fp session_callback, log_callback_fp log_callback, statistics_callback_fp statistics_callback, void* user_data);
int ffprobe_execute_with_callbacks(int argc, char **argv, session_callback_fp session_callback, log_callback_fp log_callback, void* user_data);

#if defined(__cplusplus)
}  // extern "C"
#endif


#endif // FFTOOLS_API_H