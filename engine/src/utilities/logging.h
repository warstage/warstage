// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__UTILITIES__LOGGING_H
#define WARSTAGE__UTILITIES__LOGGING_H

#include "value/value.h"

#include <cassert>
#include <functional>
#include <mutex>

enum class LogLevel { Error, Warning, Info, Debug };

using error_reporter_t = std::function<void(const char* name, const char* message, const char* stack)>;
void setErrorReporter(error_reporter_t reporter);

std::string makeStack(const char* file, int line);
std::string makeString(const char* format, ...);

void log_error(const char* name, const char* message, const char* stack, LogLevel level);
void log_assert(const char* e, const char* file, int line);
void log_assert_format_(const char* e, const char* file, int line, const char* format, ...);
void log_exception(const std::exception& e, const char* file, int line);
void log_rejection(const char* s, const char* file, int line);
void log_signal(int signum);
inline void log_silent_(...) { }

void log_print_(LogLevel level, const char* format, ...);

#define LOG_ASSERT(e) \
    ((e) ? ((void)0) : (log_assert(#e, __FILE__, __LINE__), assert(e)))
#define LOG_ASSERT_FORMAT(e, format, ...) \
    ((e) ? ((void)0) : (log_assert_format_(#e, __FILE__, __LINE__, format, ##__VA_ARGS__), assert(e)))

#define LOG_EXCEPTION(e) log_exception((e), __FILE__, __LINE__)
#define LOG_REJECTION(r) log_rejection(ReasonString(r).c_str(), __FILE__, __LINE__)

#define LOG_E(format, ...) log_print_(LogLevel::Error, format, ##__VA_ARGS__)
#define LOG_W(format, ...) log_print_(LogLevel::Warning, format, ##__VA_ARGS__)
#define LOG_I(format, ...) log_print_(LogLevel::Info, format, ##__VA_ARGS__)
#ifdef NDEBUG
#define LOG_D(format, ...) log_silent_(format, ##__VA_ARGS__)
#else
#define LOG_D(format, ...) log_print_(LogLevel::Debug, format, ##__VA_ARGS__)
#endif // NDEBUG

#define LOG_X(format, ...) log_silent_(format, ##__VA_ARGS__)
#define LOG_LIFECYCLE(format, ...) LOG_X(format, ##__VA_ARGS__)

/***/

#define MAKE_ERROR(message) (Struct() \
    << "message" << MakeErrorMessage(message, __FILE__, __LINE__) \
    << ValueEnd())

std::string MakeErrorMessage(const std::string& message, const char* file, int line);

Value MakeReason(int status, const char* file, int line, const char* format, ...);
#define REASON(status, format, ...) MakeReason(status, __FILE__, __LINE__, format, ##__VA_ARGS__)

std::string ReasonString(const std::exception_ptr& reason);
std::string ReasonString(const Value& reason);

#endif
