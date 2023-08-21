#ifndef DART_API_H_
#define DART_API_H_

#include "stdint.h"

enum FFToolsMessageType {
	FFTOOLS_RETURN_CODE_MESSAGE = 0,
	FFTOOLS_LOG_MESSAGE = 1,
	FFTOOLS_STATISTICS_MESSAGE = 2
};

typedef struct FFToolsMessage {
	enum FFToolsMessageType type;
    union {
		int32_t returnCode;
        struct {
            int32_t level;
            char* message;
        } log_val;
        struct {
            int32_t frameNumber;
            int32_t fps;
            float quality;
            int64_t size;
            int32_t time;
            double bitrate;
            double speed;
        } stats_val;
    } data;
} FFToolsMessage;

#if defined(__cplusplus)
extern "C" {
#endif

void FFToolsFFIInitialize(void* post_c_object);

void FFToolsFFIExecuteFFmpeg(int64_t send_port, int argc, char **argv);

void FFToolsFFIExecuteFFprobe(int64_t send_port, int argc, char **argv);

void FFToolsCancel(int64_t send_port);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif