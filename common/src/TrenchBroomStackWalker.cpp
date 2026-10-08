/*
 Copyright (C) 2016 Eric Wasylishen

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef _WIN32
#ifdef _MSC_VER
#include <QMutexLocker>

#include "StackWalker.h"
#endif
#else

#include <execinfo.h>

#endif

#include "TrenchBroomStackWalker.h"

#include <cstdlib>
#include <sstream>
#include <string>
#include <iostream>
#include <cxxabi.h>

namespace TrenchBroom {
#ifdef _WIN32
#ifdef _MSC_VER

// use https://stackwalker.codeplex.com/
class TBStackWalker : public StackWalker
{
public:
  std::stringstream m_string;
  TBStackWalker()
    : StackWalker()
  {
  }
  void clear() { m_string.str(""); }
  std::string asString() { return m_string.str(); }

protected:
  virtual void OnOutput(LPCSTR szText) { m_string << szText; }
};

static QMutex s_stackWalkerMutex;
static TBStackWalker* s_stackWalker;

static std::string getStackTraceInternal(CONTEXT* context)
{
  // StackWalker is not threadsafe so acquire a mutex
  QMutexLocker lock(&s_stackWalkerMutex);

  if (s_stackWalker == nullptr)
  {
    // create a shared instance on first use
    s_stackWalker = new TBStackWalker();
  }
  s_stackWalker->clear();
  if (context == nullptr)
  {
    // get the current call stack
    s_stackWalker->ShowCallstack();
  }
  else
  {
    // get the call stack of the exception.
    // see: http://www.codeproject.com/Articles/11132/Walking-the-callstack
    s_stackWalker->ShowCallstack(GetCurrentThread(), context);
  }
  return s_stackWalker->asString();
}

std::string TrenchBroomStackWalker::getStackTrace()
{
  return getStackTraceInternal(nullptr);
}

std::string TrenchBroomStackWalker::getStackTraceFromContext(void* context)
{
  return getStackTraceInternal(static_cast<CONTEXT*>(context));
}
#else
// TODO: not sure what to use on mingw
std::string TrenchBroomStackWalker::getStackTrace()
{
  return "";
}
#endif
#else

/* ------------------------------------------------------------------------------------------- */

/**
 * Translates the given internal mangled C++ ID into a readable form.
 *
 * @param mangled_name The identifier as string to demangle.
 * @return A readable, demangled name.
 */
std::string demangleSymbol(const std::string &mangled_name) {
    int status = 0;
    char *demangled = abi::__cxa_demangle(mangled_name.c_str(), nullptr, nullptr, &status);
    if (status == 0 && demangled != nullptr) {
        std::string result(demangled);
        std::free(demangled);
        return result;
    }

    return mangled_name;
}


/**
 * Collects the stack trace of the current thread and returns
 * it as printable string.
 *
 * @return Stacktrace as multiline string.
 */
std::string TrenchBroomStackWalker::getStackTrace() {
    void *callstack[MAX_CALL_STACK_FRAMES];
    const int frames = backtrace(callstack, MAX_CALL_STACK_FRAMES);

    if (frames <= 0) { return ""; }

    char **strs = backtrace_symbols(callstack, frames);
    if (!strs) { return ""; }

    std::stringstream ss;

    for (int i = 0; i < frames; i++) {
        std::string line(strs[i]);

        // macOS format: "index  binary_name  address  mangled_name + offset"
        // Find the start of the mangled token (on macOS, C++ symbols start with '_Z')
        size_t symbolStart = line.find(" _Z");
        if (symbolStart != std::string::npos) {
            symbolStart += 1; // Move past the space to the start of '_'

            // Find the end of the token (terminated by a space before the '+' sign)
            size_t symbolEnd = line.find(" +", symbolStart);
            if (symbolEnd != std::string::npos) {
                std::string mangled = line.substr(symbolStart, symbolEnd - symbolStart);
                std::string demangled = demangleSymbol(mangled);

                // Reconstruct the line replacing the mangled token with the demangled one
                line.replace(symbolStart, symbolEnd - symbolStart, demangled);
            }
        }

        ss << line << "\n";
    }

    std::free(strs);
    return ss.str();
}

#endif
}
