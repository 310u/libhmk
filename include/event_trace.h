#pragma once

#if defined(DEBUG_EVENT_TRACE)
#include <stdio.h>

#define EVENT_TRACE(...) fprintf(stderr, __VA_ARGS__)
#else
#define EVENT_TRACE(...) ((void)0)
#endif
