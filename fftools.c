#include <pthread.h>
#include <stdatomic.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "config.h"
#include "libavutil/bprint.h"
#include "libavutil/file.h"
#include "ffmpeg.h"
#include "thread_queue.h"
#include "fftools.h"

/** Fields that control the handling of SIGNALs */
volatile int handleSIGQUIT = 1;
volatile int handleSIGINT = 1;
volatile int handleSIGTERM = 1;
volatile int handleSIGXCPU = 1;
volatile int handleSIGPIPE = 1;

__thread FFToolsSession* session;

/** Holds the default log level */
int configuredLogLevel = AV_LOG_INFO;

/** Forward declaration for function defined in ffmpeg.c */
int ffmpeg_execute(int argc, char **argv);
/** Forward declaration for function defined in ffprobe.c */
int ffprobe_execute(int argc, char **argv);

void fftools_log_callback_function(void *ptr, int level, const char* format, va_list vargs);
static void fftools_statistics_callback_function(int frameNumber, float fps, float quality, int64_t size, int time, double bitrate, double speed);

typedef void (*log_callback_fp)(int level, char* message);
typedef void (*statistics_callback_fp)(int frameNumber, float fps, float quality, int64_t size, int time, double bitrate, double speed);

log_callback_fp current_log_callback;
statistics_callback_fp current_statistics_callback;

static const char *avutil_log_get_level_str(int level) {
    switch (level) {
    case AV_LOG_STDERR:
        return "stderr";
    case AV_LOG_QUIET:
        return "quiet";
    case AV_LOG_DEBUG:
        return "debug";
    case AV_LOG_VERBOSE:
        return "verbose";
    case AV_LOG_INFO:
        return "info";
    case AV_LOG_WARNING:
        return "warning";
    case AV_LOG_ERROR:
        return "error";
    case AV_LOG_FATAL:
        return "fatal";
    case AV_LOG_PANIC:
        return "panic";
    default:
        return "";
    }
}

static void avutil_log_format_line(void *avcl, int level, const char *fmt, va_list vl, AVBPrint part[4], int *print_prefix) {
    int flags = av_log_get_flags();
    AVClass* avc = avcl ? *(AVClass **) avcl : NULL;
    av_bprint_init(part+0, 0, 1);
    av_bprint_init(part+1, 0, 1);
    av_bprint_init(part+2, 0, 1);
    av_bprint_init(part+3, 0, 65536);

    if (*print_prefix && avc) {
        if (avc->parent_log_context_offset) {
            AVClass** parent = *(AVClass ***) (((uint8_t *) avcl) +
                                   avc->parent_log_context_offset);
            if (parent && *parent) {
                av_bprintf(part+0, "[%s @ %p] ",
                         (*parent)->item_name(parent), parent);
            }
        }
        av_bprintf(part+1, "[%s @ %p] ",
                 avc->item_name(avcl), avcl);
    }

    if (*print_prefix && (level > AV_LOG_QUIET) && (flags & AV_LOG_PRINT_LEVEL))
        av_bprintf(part+2, "[%s] ", avutil_log_get_level_str(level));

    av_vbprintf(part+3, fmt, vl);

    if(*part[0].str || *part[1].str || *part[2].str || *part[3].str) {
        char lastc = part[3].len && part[3].len <= part[3].size ? part[3].str[part[3].len - 1] : 0;
        *print_prefix = lastc == '\n' || lastc == '\r';
    }
}

static void avutil_log_sanitize(char *line) {
    while(*line){
        if(*line < 0x08 || (*line > 0x0D && *line < 0x20))
            *line='?';
        line++;
    }
}

typedef struct ThreadMessage {
    enum {THREADMESSAGE_LOG, THREADMESSAGE_STATS} type;
    union {
        struct {
            int level;
            char* message;
        } log_val;
        struct {
            int frameNumber;
            int fps;
            float quality;
            int64_t size;
            int time;
            double bitrate;
            double speed;
        } stats_val;
    } data;
} ThreadMessage;

static void *alloc_threadmessage(void) {
    return malloc(sizeof(ThreadMessage));
}

static void reset_threadmessage(void *obj) {
    ThreadMessage *obj_m = obj;
    // Don't bother freeing anything else, it will get overwritten later
    if (obj_m->type == THREADMESSAGE_LOG && obj_m->data.log_val.message) {
        free(obj_m->data.log_val.message);
        obj_m->data.log_val.message = NULL;
    }
}

static void free_threadmessage(void **obj) {
    ThreadMessage *obj_m = *obj;
    if (obj_m->type == THREADMESSAGE_LOG && obj_m->data.log_val.message) {
        free(obj_m->data.log_val.message);
    }
    free(obj_m);
}

static void threadmessage_move(void *dst, void *src) {
    ThreadMessage *dst_m = dst;
    ThreadMessage *src_m = src;
    dst_m->type = src_m->type;
    if (src_m->type == THREADMESSAGE_LOG) {
        dst_m->data.log_val.level = src_m->data.log_val.level;
        dst_m->data.log_val.message = strdup(src_m->data.log_val.message);
    }
    else {
        dst_m->data.stats_val.bitrate = src_m->data.stats_val.bitrate;
        dst_m->data.stats_val.fps = src_m->data.stats_val.fps;
        dst_m->data.stats_val.frameNumber = src_m->data.stats_val.frameNumber;
        dst_m->data.stats_val.quality = src_m->data.stats_val.quality;
        dst_m->data.stats_val.size = src_m->data.stats_val.size;
        dst_m->data.stats_val.speed = src_m->data.stats_val.speed;
        dst_m->data.stats_val.time = src_m->data.stats_val.time;
    }
}

void write_log_message_to_tq(int level, char* message) {
    ThreadMessage data;
    data.type = THREADMESSAGE_LOG;
    data.data.log_val.level = level;
    data.data.log_val.message = message;
    tq_send(session->tq, 0, &data); // TODO: Check return value
}

void write_statistics_message_to_tq(int frameNumber, float fps, float quality, int64_t size, int time, double bitrate, double speed) {
    ThreadMessage data;
    data.type = THREADMESSAGE_STATS;
    data.data.stats_val.frameNumber = frameNumber;
    data.data.stats_val.fps = fps;
    data.data.stats_val.quality = quality;
    data.data.stats_val.size = size;
    data.data.stats_val.time = time;
    data.data.stats_val.bitrate = bitrate;
    data.data.stats_val.speed = speed;
    tq_send(session->tq, 0, &data); // TODO: Check return value
}

typedef struct FFToolsArg {
    int argc;
    char **argv;
    FFToolsSession* session;
} FFToolsArg;

static void *ffmpeg_thread(void *arg) {
    FFToolsArg* toolsArg = arg;
    session = toolsArg->session;
    current_log_callback = write_log_message_to_tq;
    current_statistics_callback = write_statistics_message_to_tq;
    av_log_set_callback(fftools_log_callback_function);
    set_report_callback(fftools_statistics_callback_function);
    int ret = ffmpeg_execute(toolsArg->argc, toolsArg->argv);
    tq_send_finish(session->tq, 0);
    return (void*)(intptr_t)ret;
}

int ffmpeg_execute_with_callbacks(int argc, char **argv, log_callback_fp log_callback, statistics_callback_fp statistics_callback) {
    FFToolsArg arg;
    FFToolsSession session;
    ObjPool *op = objpool_alloc(alloc_threadmessage, reset_threadmessage, free_threadmessage);
    session.tq = tq_alloc(1, 10, op, threadmessage_move); // TODO: Idk
    arg.argc = argc;
    arg.argv = argv;
    arg.session = &session;
    pthread_t thread;
    pthread_create(&thread, NULL, ffmpeg_thread, (void*)&arg); // TODO: Check return value
    while (1) {
        ThreadMessage msg;
        int stream_idx;
        tq_receive(session.tq, &stream_idx, &msg); // TODO: Check return value
        if (stream_idx < 0) {
            // End of data - conversion done
            break;
        }
        if (msg.type == THREADMESSAGE_LOG) {
            log_callback(msg.data.log_val.level, msg.data.log_val.message);
        }
        else {
            statistics_callback(msg.data.stats_val.frameNumber, msg.data.stats_val.fps, msg.data.stats_val.quality, msg.data.stats_val.size, msg.data.stats_val.time, msg.data.stats_val.bitrate, msg.data.stats_val.speed);
        }
        reset_threadmessage(&msg);
    }
    void* ret;
    pthread_join(thread, &ret); // TODO: Check return value
    tq_free(&session.tq);
    return (int)(intptr_t)ret;
}

static void *ffprobe_thread(void *arg) {
    FFToolsArg* toolsArg = arg;
    session = toolsArg->session;
    current_log_callback = write_log_message_to_tq;
    current_statistics_callback = write_statistics_message_to_tq;
    av_log_set_callback(fftools_log_callback_function);
    set_report_callback(fftools_statistics_callback_function);
    int ret = ffprobe_execute(toolsArg->argc, toolsArg->argv);
    tq_send_finish(session->tq, 0);
    return (void*)(intptr_t)ret;
}

int ffprobe_execute_with_callbacks(int argc, char **argv, log_callback_fp log_callback, statistics_callback_fp statistics_callback) {
    FFToolsArg arg;
    FFToolsSession session;
    ObjPool *op = objpool_alloc(alloc_threadmessage, reset_threadmessage, free_threadmessage);
    session.tq = tq_alloc(1, 10, op, threadmessage_move); // TODO: Idk
    arg.argc = argc;
    arg.argv = argv;
    arg.session = &session;
    pthread_t thread;
    pthread_create(&thread, NULL, ffprobe_thread, (void*)&arg); // TODO: Check return value
    while (1) {
        ThreadMessage msg;
        int stream_idx;
        tq_receive(session.tq, &stream_idx, &msg); // TODO: Check return value
        if (stream_idx < 0) {
            // End of data - conversion done
            break;
        }
        if (msg.type == THREADMESSAGE_LOG) {
            log_callback(msg.data.log_val.level, msg.data.log_val.message);
        }
        else {
            statistics_callback(msg.data.stats_val.frameNumber, msg.data.stats_val.fps, msg.data.stats_val.quality, msg.data.stats_val.size, msg.data.stats_val.time, msg.data.stats_val.bitrate, msg.data.stats_val.speed);
        }
        reset_threadmessage(&msg);
    }
    void* ret;
    pthread_join(thread, &ret); // TODO: Check return value
    tq_free(&session.tq);
    return (int)(intptr_t)ret;
}

void fftools_log_callback_function(void *ptr, int level, const char* format, va_list vargs) {
    AVBPrint fullLine;
    AVBPrint part[4];
    int print_prefix = 1;

    if (level >= 0) {
        level &= 0xff;
    }
    int activeLogLevel = av_log_get_level();

    // AV_LOG_STDERR logs are always redirected
    if ((activeLogLevel == AV_LOG_QUIET && level != AV_LOG_STDERR) || (level > activeLogLevel)) {
        return;
    }

    av_bprint_init(&fullLine, 0, AV_BPRINT_SIZE_UNLIMITED);

    avutil_log_format_line(ptr, level, format, vargs, part, &print_prefix);
    avutil_log_sanitize(part[0].str);
    avutil_log_sanitize(part[1].str);
    avutil_log_sanitize(part[2].str);
    avutil_log_sanitize(part[3].str);

    // COMBINE ALL 4 LOG PARTS
    av_bprintf(&fullLine, "%s%s%s%s", part[0].str, part[1].str, part[2].str, part[3].str);

    if (fullLine.len > 0) {
        if (current_log_callback) {
            current_log_callback(level, fullLine.str);
        }
    }

    av_bprint_finalize(part, NULL);
    av_bprint_finalize(part+1, NULL);
    av_bprint_finalize(part+2, NULL);
    av_bprint_finalize(part+3, NULL);
    av_bprint_finalize(&fullLine, NULL);
}

static void fftools_statistics_callback_function(int frameNumber, float fps, float quality, int64_t size, int time, double bitrate, double speed) {
    if (current_statistics_callback) {
        current_statistics_callback(frameNumber, fps, quality, size, time, bitrate, speed);
    }
}
