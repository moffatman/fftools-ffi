#include "stdio.h"

#include "fftools_api.h"

void log_callback(int level, char* message, void* user_data) {
	printf("%s", message);
}

int main(int argc, char** argv) {
	ffprobe_execute_with_callbacks(argc - 1, argv + 1, NULL, log_callback, NULL);
}