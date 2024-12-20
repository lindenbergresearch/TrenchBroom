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


#include "StringUtils.h"

static const char FILL_CHAR = ' ';

namespace TrenchBroom {
void StatsTable::addString(const std::string &name, const std::string &value) {
    addPair(name, value);
}

void StatsTable::addString(const std::string &name, const QString &value) {
    addPair(name, value.toStdString());

}

void StatsTable::addInt(const std::string &name, const int value) {
    addPair(name, std::to_string(value));

}

void StatsTable::addLong(const std::string &name, const long value) {
    addPair(name, std::to_string(value));

}

void StatsTable::addBool(const std::string &name, const bool value) {
    addPair(name, value ? "true" : "false");
}

void StatsTable::addFloat(const std::string &name, const float value, const int n, const std::string &suffix) {
    const auto format = "%." + std::to_string(n) + "f%s";
    addPair(name, stringf(format.c_str(), value, suffix.c_str()));
}

void StatsTable::addDouble(const std::string &name, const double value, const int n, const std::string &suffix) {
    const auto format = "%." + std::to_string(n) + "f%s";
    addPair(name, stringf(format.c_str(), value, suffix.c_str()));
}

void StatsTable::addMem(const std::string &name, const size_t value) {
    if (value >= 1024) {
        addPair(name, std::to_string(value / 1024) + "K");
    } else {
        addPair(name, std::to_string(value) + "B");
    }
}

void StatsTable::addTime(const std::string &name, const double value, const TimeUnit tu) {
    switch (tu) {
        case SECONDS:
            addDouble(name, value * static_cast<double>(tu), 4, "s");
            break;
        case MILLIS:
            addDouble(name, value * static_cast<double>(tu), 2, "ms");
            break;
        case MICROS:
            addDouble(name, value * static_cast<double>(tu), 2, "µs");
            break;
        case NANOS:
            addDouble(name, value * static_cast<double>(tu), 1, "ns");
            break;
    }
}



std::string StatsTable::render(bool reverse) const {
    std::string buffer{};

    std::vector<TStringPair> items = m_lines;

    if (reverse) {
        std::reverse(items.begin(), items.end());
    } else {
        buffer += m_title + m_lineSeparatorChar + m_lineSeparatorChar;
    }

    for (const auto &[fst, snd] : items) {
        buffer += fst;

        if (fst.empty() || fst == "-") {
            buffer.append(size_t (m_labelWidth + m_rightPadding+14), '-');
            buffer += m_lineSeparatorChar;
            continue;
        }

        if (m_labelWidth > 0) {
            buffer.append(m_labelWidth - fst.size(), FILL_CHAR);
        }

        buffer.append(1, m_assignmentChar);

        if (m_rightPadding > 0) {
            buffer.append(m_rightPadding, FILL_CHAR);
        }

        buffer += snd;
        buffer += m_lineSeparatorChar;
    }

    if (reverse) {
        buffer += m_lineSeparatorChar + m_title;
    }

    return buffer;
}

size_t StatsTable::labelWidth() const {
    return m_labelWidth;
}

void StatsTable::setLabelWidth(size_t mLeftSize) {
    m_labelWidth = mLeftSize;
}

size_t StatsTable::rightPadding() const {
    return m_rightPadding;
}

void StatsTable::setRightPadding(size_t mRightSize) {
    m_rightPadding = mRightSize;
}

const std::string &StatsTable::title() const {
    return m_title;
}

void StatsTable::setTitle(const std::string &mTitle) {
    m_title = mTitle;
}

char StatsTable::assignmentChar() const {
    return m_assignmentChar;
}

void StatsTable::setAssignmentChar(char assignmentChar) {
    m_assignmentChar = assignmentChar;
}

char StatsTable::lineSeparatorChar() const {
    return m_lineSeparatorChar;
}

void StatsTable::setLineSeparatorChar(char lineSeparatorChar) {
    m_lineSeparatorChar = lineSeparatorChar;
}

void StatsTable::addPair(std::string name, std::string value, bool pushFront) {
    if (pushFront) {
        m_lines.insert(m_lines.begin(), std::pair(name, value));
    } else {
        m_lines.push_back(std::pair(name, value));
    }
}

void StatsTable::addSeparator() {
    m_lines.push_back(std::pair("-", ""));
}

void StatsTable::reset() {
    m_lines.clear();
}

Stats::Stats() {
    reset();
    min = 10E10;
    max = 0.0;
    j = 0;
}

void Stats::push(double value) {
    stat.add(value);

    if (value < min) {
        min = value;
    }

    if (value > max) {
        max = value;
    }

    if (j > resetMinMax) {
        min = 10E10;
        max = 0.0;
        j = 0;
    }
}

double Stats::average() {
    return stat.average();
}

void Stats::reset() {
    j = 0;
    min = 0.0;
    max = 0.0;
}

void Stats::softReset() {
    j++;
}

std::string replaceAllChars(std::string str, char ch) {
    std::fill(str.begin(), str.end(), ch);
    return str;
}

std::string stringf(const char *const zcFormat, ...) {
    // initialize use of the variable argument array
    va_list vaArgs;
    va_start(vaArgs, zcFormat);

    // reliably acquire the size
    // from a copy of the variable argument array
    // and a functionally reliable call to mock the formatting
    va_list vaArgsCopy;
    va_copy(vaArgsCopy, vaArgs);
    const int iLen = std::vsnprintf(nullptr, 0, zcFormat, vaArgsCopy);
    va_end(vaArgsCopy);

    // return a formatted string without risking memory mismanagement
    // and without assuming any compiler or platform specific behavior
    std::vector<char> zc(uint32_t(iLen) + 1UL);
    std::vsnprintf(zc.data(), zc.size(), zcFormat, vaArgs);
    va_end(vaArgs);

    return std::string(zc.data(), uint32_t(iLen));
}

std::string numberWithSuffix(size_t count, const std::string &singular, const std::string &plural) {
    return std::to_string(count) + " " + (count != 1 ? plural : singular);
}

BoxStats::BoxStats(size_t size) : m_pos(0), m_length(static_cast<size_t>(pow(2, size))), m_mask(m_length - 1) {
    m_data.reserve(m_length);
}

void BoxStats::add(double n) {
    m_data[m_pos] = n;
    m_pos = ++m_pos & m_mask;
}

double BoxStats::average() {
    double sum = 0;
    for (size_t i = 0; i < m_length; i++) {
        sum += m_data[i];
    }

    return sum / (double) m_length;
}
}
