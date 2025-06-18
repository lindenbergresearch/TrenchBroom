/*
Copyright (C) 2010-2017 Kristian Duske

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

#include "TimeUtil.h"

#if defined __APPLE__
#include <mach/mach_init.h>
#include <mach/thread_act.h>
#include <mach/mach_time.h>
#include <sys/sysctl.h>

#endif

#if defined _WIN32
#include <windows.h>
#include <shellapi.h>
#include <processthreadsapi.h>
#include <dbghelp.h>
#endif

namespace TrenchBroom {

void TimeUtil::init() {
#if defined _WIN32
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    startCounter = counter.QuadPart;

    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    counterTime = 1.0 / frequency.QuadPart;
#endif
#if defined __linux__
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    startTime = int64_t(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
#endif
#if defined __APPLE__
    startCounter = mach_absolute_time();

    mach_timebase_info_data_t tb;
    mach_timebase_info(&tb);
    counterTime = 1e-9 * static_cast<double>(tb.numer) / static_cast<double>(tb.denom);
#endif
}


double TimeUtil::current() {
#if defined _WIN32
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (counter.QuadPart - startCounter) * counterTime;
#elif defined ARCH_LIN
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    int64_t time = int64_t(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
    return (time - startTime) / 1e9;
#elif defined __APPLE__
    uint64_t counter = mach_absolute_time();
    return (static_cast<double>(counter) - static_cast<double>(startCounter)) * counterTime;
#else
    return 0.0;
#endif
}

TimeUtil::TimeUtil() {
    init();
}
}
