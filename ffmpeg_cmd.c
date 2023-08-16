#include "stdio.h"

typedef void (*log_callback_fp)(int level, char* message);
typedef void (*statistics_callback_fp)(int frameNumber, float fps, float quality, int64_t size, int time, double bitrate, double speed);
extern int ffmpeg_execute_with_callbacks(int argc, char **argv, log_callback_fp log_callback, statistics_callback_fp statistics_callback);

void log_callback(int level, char* message) {
	printf("%s", message);
}

int main(int argc, char** argv) {
	ffmpeg_execute_with_callbacks(argc, argv, log_callback, NULL);
}