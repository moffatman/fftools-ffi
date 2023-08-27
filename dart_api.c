#include "dart_api.h"
#include "dart_api_types.h"
#include "fftools_api.h"
#include "libavutil/thread.h"

static Dart_PostCObject post_c_object_ = NULL;
static pthread_mutex_t* lock;

struct node {
	Dart_Port port;
	FFToolsSession* session;
	struct node* next;
};

struct node* head;

static void ensure_lock_created() {
	if (lock) {
		return;
	}
	lock = malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(lock, NULL);
}

static FFToolsSession* find_session(Dart_Port port) {
	ensure_lock_created();
	FFToolsSession* session = NULL;
	pthread_mutex_lock(lock);
	struct node* p = head;
	while (p) {
		if (p->port == port) {
			session = p->session;
			break;
		}
		p = p->next;
	}
	pthread_mutex_unlock(lock);
	return session;
}

static void store_session(Dart_Port port, FFToolsSession* session) {
	ensure_lock_created();
	pthread_mutex_lock(lock);
	struct node** to_p = &head;
	while (*to_p) {
		to_p = &(*to_p)->next;
	}
	*to_p = malloc(sizeof(struct node));
	(*to_p)->next = NULL;
	(*to_p)->port = port;
	(*to_p)->session = session;
	pthread_mutex_unlock(lock);
}

static void remove_session(Dart_Port port) {
	ensure_lock_created();
	pthread_mutex_lock(lock);
	struct node** to_p = &head;
	while (*to_p) {
		if ((*to_p)->port == port) {
			struct node* new_p = (*to_p)->next;
			free(*to_p);
			*to_p = new_p;
			break;
		}
		to_p = &(*to_p)->next;
	}
	pthread_mutex_unlock(lock);
}

typedef struct DartApiArg {
	int64_t send_port;
    int argc;
    char **argv;
} DartApiArg;

void FFToolsFFIInitialize(void* post_c_object) {
	post_c_object_ = (Dart_PostCObject)post_c_object;
}

static void ffi_session_callback(FFToolsSession* session, void* user_data) {
	store_session((Dart_Port)user_data, session);
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
	remove_session(dartArg->send_port);
	for (int i = 0; i < dartArg->argc; i++) {
		free(dartArg->argv[i]);
	}
	free(dartArg->argv);
	free(dartArg);
	return NULL;
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
	remove_session(dartArg->send_port);
	for (int i = 0; i < dartArg->argc; i++) {
		free(dartArg->argv[i]);
	}
	free(dartArg->argv);
	free(dartArg);
	return NULL;
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
	FFToolsSession* session = find_session(send_port);
	if (!session) {
		fprintf(stderr, "Failed to find session for send_port %lld to cancel\n", send_port);
		return;
	}
	session->cancel_requested = 1;
}