#include <map>

#include "dart_api.h"
#include "dart_api_types.h"
#include "fftools_api.h"
#include "libavutil/thread.h"

static Dart_PostCObject post_c_object_ = nullptr;
static std::map<Dart_Port, FFToolsSession*> sessions_;

typedef struct DartApiArg {
	int64_t send_port;
    int argc;
    char **argv;
} DartApiArg;

void FFToolsFFIInitialize(void* post_c_object) {
	post_c_object_ = (Dart_PostCObject)post_c_object;
}

static void ffi_session_callback(FFToolsSession* session, void* user_data) {
	Dart_Port port = (Dart_Port)user_data;
	sessions_[port] = session;
}

static void ffi_log_callback(int level, char* log_message, void* user_data) {
	if (!post_c_object_) {
		fprintf(stderr, "Got log callback [%d] without post_c_object_: %s", level, log_message);
		return;
	}
	Dart_Port port = (Dart_Port)user_data;
	FFToolsMessage *message = (FFToolsMessage*)malloc(sizeof(FFToolsMessage));
	message->type = FFTOOLS_LOG_MESSAGE;
	message->data.log_val.level = level;
	message->data.log_val.message = strdup(log_message);
	Dart_CObject object;
	object.type = Dart_CObject_kInt64;
	object.value.as_int64 = (int64_t)message;
	int ret = post_c_object_(port, &object);
	if (!ret) {
		// Send failed
		fprintf(stderr, "Failed to post_c_object_ for log with error %d\n", ret);
		free(message->data.log_val.message);
		free(message);
	}
}

static void ffi_statistics_callback(int frameNumber, float fps, float quality, int64_t size, int time, double bitrate, double speed, void* user_data) {
	if (!post_c_object_) {
		fprintf(stderr, "Got statistics callback without post_c_object_\n");
		return;
	}
	Dart_Port port = (Dart_Port)user_data;
	FFToolsMessage *message = (FFToolsMessage*)malloc(sizeof(FFToolsMessage));
	message->type = FFTOOLS_STATISTICS_MESSAGE;
	message->data.stats_val.frameNumber = frameNumber;
	message->data.stats_val.fps = fps;
	message->data.stats_val.quality = quality;
	message->data.stats_val.size = size;
	message->data.stats_val.time = time;
	message->data.stats_val.bitrate = bitrate;
	message->data.stats_val.speed = speed;
	Dart_CObject object;
	object.type = Dart_CObject_kInt64;
	object.value.as_int64 = (int64_t)message;
	int ret = post_c_object_(port, &object);
	if (!ret) {
		// Send failed
		fprintf(stderr, "Failed to post_c_object_ for stats with error %d\n", ret);
		free(message);
	}
}

static void* ffmpeg_thread_(void* arg) {
	DartApiArg* dartArg = (DartApiArg*)arg;
	int returnCode = ffmpeg_execute_with_callbacks(dartArg->argc, dartArg->argv, ffi_session_callback, ffi_log_callback, ffi_statistics_callback, (void*)dartArg->send_port);
	FFToolsMessage *message = (FFToolsMessage*)malloc(sizeof(FFToolsMessage));
	message->type = FFTOOLS_RETURN_CODE_MESSAGE;
	message->data.returnCode = returnCode;
	Dart_CObject object;
	object.type = Dart_CObject_kInt64;
	object.value.as_int64 = (int64_t)message;
	int ret = post_c_object_(dartArg->send_port, &object);
	if (!ret) {
		// Send failed
		fprintf(stderr, "Failed to post_c_object_ for return code %d with error %d\n", returnCode, ret);
		free(message);
	}
	sessions_.erase(dartArg->send_port);
	for (int i = 0; i < dartArg->argc; i++) {
		free(dartArg->argv[i]);
	}
	free(dartArg->argv);
	free(dartArg);
	return nullptr;
}

void FFToolsFFIExecuteFFmpeg(int64_t send_port, int argc, char **argv) {
	DartApiArg* arg = (DartApiArg*)malloc(sizeof(DartApiArg));
	arg->send_port = send_port;
	arg->argc = argc;
	arg->argv = argv;
	pthread_t thread;
	pthread_create(&thread, NULL, ffmpeg_thread_, (void*)arg); // TODO: Check return value
}

static void* ffprobe_thread_(void* arg) {
	DartApiArg* dartArg = (DartApiArg*)arg;
	int returnCode = ffprobe_execute_with_callbacks(dartArg->argc, dartArg->argv, ffi_session_callback, ffi_log_callback, (void*)dartArg->send_port);
	FFToolsMessage *message = (FFToolsMessage*)malloc(sizeof(FFToolsMessage));
	message->type = FFTOOLS_RETURN_CODE_MESSAGE;
	message->data.returnCode = returnCode;
	Dart_CObject object;
	object.type = Dart_CObject_kInt64;
	object.value.as_int64 = (int64_t)message;
	int ret = post_c_object_(dartArg->send_port, &object);
	if (!ret) {
		// Send failed
		fprintf(stderr, "Failed to post_c_object_ for return code %d with error %d\n", returnCode, ret);
		free(message);
	}
	sessions_.erase(dartArg->send_port);
	for (int i = 0; i < dartArg->argc; i++) {
		free(dartArg->argv[i]);
	}
	free(dartArg->argv);
	free(dartArg);
	return nullptr;
}


void FFToolsFFIExecuteFFprobe(int64_t send_port, int argc, char **argv) {
	DartApiArg* arg = (DartApiArg*)malloc(sizeof(DartApiArg));
	arg->send_port = send_port;
	arg->argc = argc;
	arg->argv = argv;
	pthread_t thread;
	pthread_create(&thread, NULL, ffprobe_thread_, (void*)arg); // TODO: Check return value
}

void FFToolsCancel(int64_t send_port) {
	std::map<Dart_Port, FFToolsSession*>::iterator session = sessions_.find(send_port);
	if (session == sessions_.end()) {
		fprintf(stderr, "Failed to find session for send_port %lld to cancel\n", send_port);
		return;
	}
	session->second->cancel_requested = 1;
}