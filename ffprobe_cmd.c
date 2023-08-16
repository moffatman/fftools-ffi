#include "stdio.h"

typedef void (*log_callback_fp)(int level, char* message);
extern int ffprobe_execute_with_callbacks(int argc, char **argv, log_callback_fp log_callback);

void log_callback(int level, char* message) {
	printf("%s", message);
}

int main(int argc, char** argv) {
	ffprobe_execute_with_callbacks(argc, argv, log_callback);
}