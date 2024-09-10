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

#pragma once

#include <chrono>
#include <QString>

namespace TrenchBroom {

/**
 * @class Chrono
 * @brief A static utility class that records the program start time and provides functions to measure elapsed time in various units.
 *
 * The class sets the start time when the program starts and provides static methods to return the time elapsed
 * since the program started in nanoseconds, microseconds, milliseconds, and as a formatted string.
 */
class Chrono {
  public:
    /**
     * @brief Initializes the start time at program start.
     *
     * This method sets the start time when called. It is typically called once at the beginning of the program.
     */
    static void start();

    /**
     * @brief Returns the elapsed time since the program started in nanoseconds.
     *
     * @return qint64 The elapsed time in nanoseconds.
     */
    static long nanoTime();

    /**
     * @brief Returns the elapsed time since the program started in microseconds.
     *
     * @return qint64 The elapsed time in microseconds.
     */
    static double micoTime();

    /**
     * @brief Returns the elapsed time since the program started in milliseconds.
     *
     * @return qint64 The elapsed time in milliseconds.
     */
    static double milliTime();

    /**
     * @brief Returns the elapsed time since the program started in seconds.
     *
     * @return qint64 The elapsed time in milliseconds.
     */
    static double time();


    /**
     * @brief Returns the elapsed time since the program started as a formatted string (hours, minutes, seconds, milliseconds).
     *
     * The format is: "Xh:Ym:Zs:Wms", where X is hours, Y is minutes, Z is seconds, and W is milliseconds.
     *
     * @return QString The formatted time string.
     */
    static QString elapsedFormatted();

  private:
    static std::chrono::high_resolution_clock::time_point startTime;  ///< Stores the program's start time.

    /**
     * @brief Helper function to calculate the elapsed time since the start time.
     *
     * @return std::chrono::high_resolution_clock::duration The elapsed duration since the start time.
     */
    static std::chrono::high_resolution_clock::duration elapsed();
};

}
