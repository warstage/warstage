// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "logging.h"

#include <chrono>
#include <cmath>
#include <cstdarg>
#include <ctime>
#include <iostream>
#include <sstream>
#include <vector>
#include <typeinfo>

#ifdef WARSTAGE_ENABLE_BOOST_STACKTRACE
#define BOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED
#define BOOST_STACKTRACE_USE_ADDR2LINE
#include <boost/stacktrace.hpp>
#endif


// const char* str(LogLevel level) {
//   switch (level) {
//     case LogLevel::Error: return "error";
//     case LogLevel::Warning: return "warning";
//     case LogLevel::Info: return "info";
//     case LogLevel::Debug: return "debug";
//     default: return "?";
//   }
// }

static error_reporter_t errorReporter__;

void setErrorReporter(error_reporter_t reporter) {
  errorReporter__ = reporter;
}


namespace {
  static const char* stripPath(const char* path) {
    for (const char* i = path; *i != '\0'; ++i)
      if (*i == '/') {
        path = i;
      }
    return path + 1;
  }

}


std::string makeStack(const char* file, int line) {
#ifdef WARSTAGE_ENABLE_BOOST_STACKTRACE
  std::ostringstream stack{};
  stack << stripPath(file) << ':' << line << '\n';
  stack << boost::stacktrace::stacktrace{};
  return stack.str();
#else
  return std::string(stripPath(file)) + ":" + std::to_string(line);
#endif
}


std::string makeString(const char* format, ...) {
  std::string result;
  result.resize(16384);

  va_list args;
  va_start(args, format);
  int length = vsnprintf(result.data(), result.size(), format, args);
  va_end(args);
  result.resize(length);
  return result;
}


void log_error(const char* name, const char* message, const char* stack, LogLevel level) {
  log_print_(level, "%s: %s at %s", name, message, stack);
  if (errorReporter__) {
    errorReporter__(name, message, stack);
  }
}


void log_assert(const char* e, const char* file, int line) {
  std::string name = std::string("assert(") + e + ")";
  log_error(name.c_str(), e, makeStack(file, line).c_str(), LogLevel::Error);
}


void log_assert_format_(const char* e, const char* file, int line, const char* format, ...) {
  std::string f(format);
  while (!f.empty() && f.back() == '\n')
    f.erase(f.size() - 1);

  std::string message;
  message.resize(16384);

  va_list args;
  va_start(args, format);
  int length = vsnprintf(message.data(), message.size(), f.c_str(), args);
  va_end(args);
  message.resize(length);

  std::string name = std::string("assert(") + e + ")";
  log_error(name.c_str(), message.c_str(), makeStack(file, line).c_str(), LogLevel::Error);
}


void log_exception(const std::exception& e, const char* file, int line) {
  log_error("EXCEPTION", e.what(), makeStack(file, line).c_str(), LogLevel::Error);
}


void log_signal(const int signum) {
  std::ostringstream message{};
  message << "SIGNAL:" << signum;
  log_error("SIGNAL", message.str().c_str(), "", LogLevel::Error);
}


void log_rejection(const char* e, const char* file, int line) {
  log_error("REJECT", e, makeStack(file, line).c_str(), LogLevel::Info);
}


void log_print_(LogLevel level, const char* format, ...) {
    std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    int millisecs = static_cast<int>(std::fmod(static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count()), 1000));
    char s[64];
    std::strftime(s, 64, "%F %T", std::localtime(&t));
    fprintf(stdout, "%s.%.3d ", s, millisecs);

    std::string f(format);
    if (!f.empty() && f[f.size() - 1] != '\n')
        f.push_back('\n');
    va_list args;
    va_start(args, format);
    vfprintf(stdout, f.c_str(), args);
    fflush(stdout);
    va_end(args);
}


std::string MakeErrorMessage(const std::string& message, const char* file, int line) {
  std::ostringstream os;
  os << message << " in " << stripPath(file) << ":" << line;
  return os.str();
}


Value MakeReason(int status, const char* file, int line, const char* format, ...) {
  std::ostringstream name;
  name << "STATUS_" << status;

  va_list args1;
  va_start(args1, format);
  va_list args2;
  va_copy(args2, args1);
  int length = std::vsnprintf(nullptr, 0, format, args1) + 1;
  std::vector<char> buffer(static_cast<std::size_t>(length));
  va_end(args1);
  std::vsnprintf(buffer.data(), buffer.size(), format, args2);
  va_end(args2);

  return Struct{}
      << "name" << name.str()
      << "message" << buffer.data()
      << "file" << stripPath(file)
      << "line" << line
      << ValueEnd{};
}


std::string ReasonString(const std::exception_ptr& reason) {
  try {
    std::rethrow_exception(reason);
  } catch (const Value& v) {
    return ReasonString(v);
  } catch (const std::exception& e) {
    return e.what();
  } catch (...) {
    return "";
  }
}


std::string ReasonString(const Value& reason) {
  std::ostringstream os;

  if (const char* name = reason["name"_c_str]) {
    os << name << " ";
  }

  if (const char* message = reason["message"_c_str]) {
    os << message << " ";
  }

  if (const char* stack = reason["stack"_c_str]) {
    os << stack << " ";
  }

  return os.str();
}
