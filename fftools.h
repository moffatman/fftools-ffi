#ifndef FFTOOLS_H
#define FFTOOLS_H

#include "fftools_api.h"

extern __thread FFToolsSession* session;

int printf_stderr(const char *fmt, ...);

#endif // FFTOOLS_H
