#pragma once
#include <thread>

#define TO_STR2(arg) #arg
#define TO_STR(arg) TO_STR2(arg)

static thread_local auto ttid = std::this_thread::get_id();

// Empty placeholders for compatibility
#if 0
#define INDEX_LOG_TRACE(fmt, ...) (printf("[%s:" TO_STR(__LINE__) "][%x] " fmt "\n", (__FILE__ + 80), ttid __VA_OPT__(, ) __VA_ARGS__))
#else
#define INDEX_LOG_TRACE(...) ((void)0)
#endif
#define INDEX_LOG_DEBUG(...) ((void)0)
#define INDEX_LOG_INFO(...) ((void)0)
#define INDEX_LOG_WARN(...) ((void)0)
#define INDEX_LOG_ERROR(...) ((void)0)
