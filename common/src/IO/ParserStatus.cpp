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

#include "ParserStatus.h"

#include "Exceptions.h"
#include "Logger.h"

#include <cassert>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <QString>

namespace TrenchBroom {
namespace IO {
struct ParserStatus::Source {
    std::string text;
    std::string name;
    std::vector<size_t> lines{0};
    Source(std::string_view input, std::string sourceName) : text(input), name(std::move(sourceName)) {
        for (size_t i = 0; i < text.size(); ++i) {
            if (text[i] == '\r') {
                if (i + 1 < text.size() && text[i + 1] == '\n') ++i;
                lines.push_back(i + 1);
            } else if (text[i] == '\n') lines.push_back(i + 1);
        }
    }
};

void ParserStatus::setSource(std::string_view text, std::string name) {
    if (name.empty()) name = m_source && m_source->text == text ? m_source->name : m_prefix;
    // Repeated map-format attempts can share the same owned text and index.
    if (m_source && m_source->name == name && m_source->text == text) return;
    m_source = std::make_shared<Source>(text, std::move(name));
}
ParserStatus::SourceScope::SourceScope(ParserStatus& status, std::string_view text, std::string name)
    : m_status(status), m_previous(status.m_source) { status.setSource(text, std::move(name)); }
ParserStatus::SourceScope::~SourceScope() { m_status.m_source = std::move(m_previous); }
ParserStatus::SourceScope ParserStatus::scopedSource(std::string_view text, std::string name) {
    return SourceScope(*this, text, std::move(name));
}
ParserStatus::SourceLocation ParserStatus::location(size_t line, size_t column) const {
    auto result = SourceLocation{line, column, m_source ? m_source->name : m_prefix, {}, false};
    if (m_source && line > 0 && line <= m_source->lines.size()) {
        const auto start = m_source->lines[line - 1];
        const auto end = m_source->text.find_first_of("\r\n", start);
        result.sourceLine = m_source->text.substr(start, end == std::string::npos ? end : end - start);
        result.hasSourceLine = true;
    }
    return result;
}
ParserException ParserStatus::withContext(const ParserException& exception) const {
    if (exception.hasSourceContext() || !exception.line()) return exception;
    const auto here = location(exception.line(), exception.column());
    return here.hasSourceLine ? exception.withSourceContext(buildMessage(here, exception.reason())) : exception;
}
void ParserStatus::warn(const SourceLocation& here, const std::string& message) { doLog(LogLevel::Warn, buildMessage(here, message)); }
void ParserStatus::error(const SourceLocation& here, const std::string& message) { doLog(LogLevel::Error, buildMessage(here, message)); }

ParserStatus::ParserStatus(Logger &logger, const std::string &prefix) : m_logger(logger), m_prefix(prefix) {
}

ParserStatus::~ParserStatus() {}

void ParserStatus::progress(const double progress) {
    assert(progress >= 0.0 && progress <= 1.0);
    doProgress(progress);
}

void ParserStatus::debug(const size_t line, const size_t column, const std::string &str) {
    log(LogLevel::Debug, line, column, str);
}

void ParserStatus::info(const size_t line, const size_t column, const std::string &str) {
    log(LogLevel::Info, line, column, str);
}

void ParserStatus::warn(const size_t line, const size_t column, const std::string &str) {
    log(LogLevel::Warn, line, column, str);
}

void ParserStatus::error(const size_t line, const size_t column, const std::string &str) {
    log(LogLevel::Error, line, column, str);
}

void ParserStatus::errorAndThrow(const size_t line, const size_t column, const std::string &str) {
    error(line, column, str);
    throw ParserException(buildMessage(line, column, str));
}

void ParserStatus::debug(const size_t line, const std::string &str) {
    log(LogLevel::Debug, line, str);
}

void ParserStatus::info(const size_t line, const std::string &str) {
    log(LogLevel::Info, line, str);
}

void ParserStatus::warn(const size_t line, const std::string &str) {
    log(LogLevel::Warn, line, str);
}

void ParserStatus::error(const size_t line, const std::string &str) {
    log(LogLevel::Error, line, str);
}

void ParserStatus::errorAndThrow(size_t line, const std::string &str) {
    error(line, str);
    throw ParserException(buildMessage(line, str));
}

void ParserStatus::debug(const std::string &str) {
    log(LogLevel::Debug, str);
}

void ParserStatus::info(const std::string &str) {
    log(LogLevel::Info, str);
}

void ParserStatus::warn(const std::string &str) {
    log(LogLevel::Warn, str);
}

void ParserStatus::error(const std::string &str) {
    log(LogLevel::Error, str);
}

void ParserStatus::errorAndThrow(const std::string &str) {
    error(str);
    throw ParserException(buildMessage(str));
}

void ParserStatus::log(const LogLevel level, const size_t line, const size_t column, const std::string &str) {
    doLog(level, buildMessage(line, column, str));
}

std::string ParserStatus::buildMessage(const size_t line, const size_t column, const std::string &str) const {
    return buildMessage(location(line, column), str);
}

std::string ParserStatus::buildMessage(const SourceLocation& here, const std::string& str) const {
    std::stringstream msg;
    if (!here.hasSourceLine) {
        if (!here.sourceName.empty()) msg << here.sourceName << ": ";
        msg << str << " (line " << here.line;
        if (here.column) msg << ", column " << here.column;
        msg << ")";
        return msg.str();
    }
    if (!here.sourceName.empty()) {
        msg << here.sourceName << ':' << here.line;
        if (here.column) msg << ':' << here.column;
    } else {
        msg << "line " << here.line;
        if (here.column) msg << ", column " << here.column;
    }
    const auto width = std::to_string(here.line).size();
    msg << ": " << str << '\n' << std::setw(int(width)) << here.line << " | " << here.sourceLine;
    if (here.column) {
        msg << '\n' << std::string(width, ' ') << " | ";
        // Tokenizer columns are byte offsets. Keep tabs and count UTF-8
        // characters rather than their bytes when aligning the caret.
        const auto prefix = QString::fromUtf8(here.sourceLine.data(), qsizetype(std::min(here.column - 1, here.sourceLine.size())));
        for (auto c : prefix.toUcs4()) {
            if (c == '\t') msg << '\t';
            else if (QChar::category(c) != QChar::Mark_NonSpacing && QChar::category(c) != QChar::Mark_Enclosing) msg << ' ';
        }
        msg << '^';
    }
    return msg.str();
}

void ParserStatus::log(const LogLevel level, const size_t line, const std::string &str) {
    doLog(level, buildMessage(line, str));
}

std::string ParserStatus::buildMessage(const size_t line, const std::string &str) const {
    return buildMessage(location(line), str);
}

void ParserStatus::log(const LogLevel level, const std::string &str) {
    doLog(level, buildMessage(str));
}

std::string ParserStatus::buildMessage(const std::string &str) const {
    std::stringstream msg;
    if (!m_prefix.empty()) {
        msg << m_prefix << ": ";
    }
    msg << str << " (unknown position)";
    return msg.str();
}

void ParserStatus::doLog(const LogLevel level, const std::string &str) {
    m_logger.log(level, str);
}
} // namespace IO
} // namespace TrenchBroom
