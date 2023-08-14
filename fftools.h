#ifndef FFTOOLS_H
#define FFTOOLS_H

#include <stdio.h>
#include <setjmp.h>
#include "thread_queue.h"

typedef struct FFToolsSession {
    ThreadQueue *tq;
    /** Holds information to implement exception handling. */
    jmp_buf ex_buf__;
    int cancel_requested;
} FFToolsSession;

extern __thread FFToolsSession* session;

#endif // FFTOOLS_H
