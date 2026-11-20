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

#include <string>
#include <string_view>
#include <memory>

namespace TrenchBroom {
class Logger;
class ParserException;

enum class LogLevel;

namespace IO {
class ParserStatus {
    struct Source;
  private:
    Logger &m_logger;
    std::string m_prefix;
    std::shared_ptr<const Source> m_source;

  protected:
    explicit ParserStatus(Logger &logger, const std::string &prefix);

  public:
    virtual ~ParserStatus();

    // Owned source excerpts remain valid after the parser/input is destroyed.
    struct SourceLocation {
        size_t line = 0;
        size_t column = 0;
        std::string sourceName;
        std::string sourceLine;
        bool hasSourceLine = false;
    };
    class SourceScope {
        ParserStatus& m_status;
        std::shared_ptr<const Source> m_previous;
    public:
        SourceScope(ParserStatus& status, std::string_view text, std::string name);
        ~SourceScope();
        SourceScope(const SourceScope&) = delete;
        SourceScope& operator=(const SourceScope&) = delete;
    };
    void setSource(std::string_view text, std::string name = {});
    SourceScope scopedSource(std::string_view text, std::string name = {});
    SourceLocation location(size_t line, size_t column = 0) const;
    ParserException withContext(const ParserException& exception) const;
    void warn(const SourceLocation& location, const std::string& message);
    void error(const SourceLocation& location, const std::string& message);

  public:
    void progress(double progress);

    void debug(size_t line, size_t column, const std::string &str);

    void info(size_t line, size_t column, const std::string &str);

    void warn(size_t line, size_t column, const std::string &str);

    void error(size_t line, size_t column, const std::string &str);

    [[noreturn]] void errorAndThrow(size_t line, size_t column, const std::string &str);

    void debug(size_t line, const std::string &str);

    void info(size_t line, const std::string &str);

    void warn(size_t line, const std::string &str);

    void error(size_t line, const std::string &str);

    [[noreturn]] void errorAndThrow(size_t line, const std::string &str);

    void debug(const std::string &str);

    void info(const std::string &str);

    void warn(const std::string &str);

    void error(const std::string &str);

    [[noreturn]] void errorAndThrow(const std::string &str);

  private:
    void log(LogLevel level, size_t line, size_t column, const std::string &str);

    std::string buildMessage(size_t line, size_t column, const std::string &str) const;
    std::string buildMessage(const SourceLocation& location, const std::string& str) const;

    void log(LogLevel level, size_t line, const std::string &str);

    std::string buildMessage(size_t line, const std::string &str) const;

    void log(LogLevel level, const std::string &str);

    std::string buildMessage(const std::string &str) const;

  private:
    virtual void doProgress(double progress) = 0;

    virtual void doLog(LogLevel level, const std::string &str);
};
} // namespace IO
} // namespace TrenchBroom
