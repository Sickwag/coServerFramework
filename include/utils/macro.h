#pragma once

#include "log.h"
#include "util.h"
#include <assert.h>

#if defined __GNUC__ || defined __llvm__
#	define AZZATO_LIKELY(x) __builtin_expect(!!(x), 1)
#	define AZZATO_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#	define AZZATO_LIKELY(x) (x)
#	define AZZATO_UNLIKELY(x) (x)
#endif

// assert with info
#define AZZATO_ASSERT(x)                                                                  \
	if(AZZATO_UNLIKELY(!(x))) {                                                           \
		AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "ASSERTION: " #x << "\nbacktrace:\n"       \
											<< azzato::backtraceToString(100, 2, "    "); \
		assert(x);                                                                        \
	}

// assert with info, support custom tips
#define AZZATO_ASSERT2(x, w)                                                              \
	if(AZZATO_UNLIKELY(!(x))) {                                                           \
		AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "ASSERTION: " #x << "\n"                   \
											<< w << "\nbacktrace:\n"                      \
											<< azzato::backtraceToString(100, 2, "    "); \
		assert(x);                                                                        \
	}
