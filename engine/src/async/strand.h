// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__ASYNC__STRAND_H
#define WARSTAGE__ASYNC__STRAND_H

#ifndef ENABLE_ASYNC_STRAND_ASSERT
#ifdef NDEBUG
#define ENABLE_ASYNC_STRAND_ASSERT 0
#else
#define ENABLE_ASYNC_STRAND_ASSERT 1
#endif
#endif

#include <cassert>
#include <functional>
#include <memory>
#include <mutex>



class TimeoutObject {
public:
  TimeoutObject() = default;
  TimeoutObject(TimeoutObject&&) = delete;
  TimeoutObject(const TimeoutObject&) = delete;
  TimeoutObject& operator=(TimeoutObject&&) = delete;
  TimeoutObject& operator=(const TimeoutObject&) = delete;
  virtual ~TimeoutObject() = default;
  virtual void clear() = 0;
};

class IntervalObject {
public:
  IntervalObject() = default;
  IntervalObject(IntervalObject&&) = delete;
  IntervalObject(const IntervalObject&) = delete;
  IntervalObject& operator=(IntervalObject&&) = delete;
  IntervalObject& operator=(const IntervalObject&) = delete;
  virtual ~IntervalObject() = default;
  virtual void clear() = 0;
};

class ImmediateObject {
public:
  ImmediateObject() = default;
  ImmediateObject(ImmediateObject&&) = delete;
  ImmediateObject(const ImmediateObject&) = delete;
  ImmediateObject& operator=(ImmediateObject&&) = delete;
  ImmediateObject& operator=(const ImmediateObject&) = delete;
  virtual ~ImmediateObject() = default;
  virtual void clear() = 0;
};


inline void clearTimeout(TimeoutObject& timeoutObject) {
  timeoutObject.clear();
}

inline void clearInterval(IntervalObject& intervalObject) {
  intervalObject.clear();
}

inline void clearImmediate(ImmediateObject& immediateObject) {
  immediateObject.clear();
}


#include "./strand-asio.h"
#include "./strand-manual.h"

class Strand_Asio;
using Strand = Strand_Asio;


#endif
