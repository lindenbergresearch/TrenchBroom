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

#include "Chrono.h"

namespace TrenchBroom {

// Initialize the static start time at the program start.
std::chrono::high_resolution_clock::time_point Chrono::startTime = std::chrono::high_resolution_clock::now();

std::chrono::high_resolution_clock::duration Chrono::elapsed() {
    return std::chrono::high_resolution_clock::now() - startTime;
}

QString Chrono::elapsedFormatted() {
    qint64 milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed()).count();
    qint64 seconds = milliseconds / 1000;
    qint64 minutes = seconds / 60;
    qint64 hours = minutes / 60;

    QString formattedTime = QString("%1h:%2m:%3s:%4ms")
        .arg(hours)
        .arg(minutes % 60)
        .arg(seconds % 60)
        .arg(milliseconds % 1000);

    return formattedTime;
}

long Chrono::nanoTime() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed()).count();
}

double Chrono::micoTime() {
    return double(nanoTime()) * 10e-3;
}


double Chrono::milliTime() {
    return double(nanoTime()) * 10e-6;
}

double Chrono::time() {
    return double(nanoTime()) * 10e-9;
}


void Chrono::start() {
    startTime = std::chrono::high_resolution_clock::now();
}
}
