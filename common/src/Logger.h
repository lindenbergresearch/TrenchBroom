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

#include <sstream>
#include <string>
#include <map>

#include "Chrono.h"
#include "StringUtils.h"


class QString;

namespace TrenchBroom {

/* ------------------------------------------------------------------------------------------- */

/**
 * Log-Levels.
 */
enum class LogLevel {
  Trace, Debug, Info, Warn, Error
};

/**
 * Attributes of a certain Log-Level.
 */
struct LogLevelAttr {
  QString label;
  QString format;
};

/**
 * Definition of the level attributes.
 */
static std::map<LogLevel, LogLevelAttr> levelAttributes = {
    {LogLevel::Trace, {"Trace", "\033[1;90m"}},
    {LogLevel::Debug, {"Debug", "\033[1;36m"}},
    {LogLevel::Info, {"Info ", "\033[1;97m"}},
    {LogLevel::Warn, {"Warn ", "\033[1;33m"}},
    {LogLevel::Error, {"Error", "\033[1;31m"}},
};

/**
 * A LogMessage struct.
 */
struct LogMessage {
  LogLevel level;
  QString message;
  double time = Chrono::time();
  uint8_t group = 0x00;


  /**
   * Returns the log-message in a detailed form.
   * @param colored If true console color-codes are added.
   * @return The formatted log-message.
   */
  inline QString format(bool detailed = true, bool colored = true) const;
};

/**
 * Simple LogMessage cache.
 */
class LogMessageCache {
  public:
    static void add(LogMessage *logMessage);

    static LogMessage *get(size_t id);

    static void clear();

    static size_t size();

    static size_t currentID();

  private:
    static std::map<size_t, LogMessage *> cache;
    static size_t m_id;
};


/**
 * Abstract basic logger class.
 */
class Logger {
  public:
    /**
     * LogStream class for add stream like handling
     * to log-messages.
     */
    class LogStream {
      private:
        Logger *m_logger;
        LogLevel m_logLevel;
        std::stringstream m_buf;

      public:
        LogStream(Logger *logger, LogLevel logLevel);

        ~LogStream();

        template<typename T>
        LogStream &operator<<(T &&arg) {
            m_buf << std::forward<T>(arg);
            return *this;
        }

        // Specialization for QString
        LogStream &operator<<(const QString &arg) {
            m_buf << arg.toStdString();  // Convert QString to std::string
            return *this;
        }
    };

  public:
    virtual ~Logger();

    // --- DEBUG -------------------------------------- //

    LogStream debug();

    void debug(const char *message);

    void debug(const std::string &message);

    void debug(const QString &message);

    // --- INFO --------------------------------------- //

    LogStream info();

    void info(const char *message);

    void info(const std::string &message);

    void info(const QString &message);

    // --- WARNING ------------------------------------ //

    LogStream warn();

    void warn(const char *message);

    void warn(const std::string &message);

    void warn(const QString &message);

    // --- ERROR -------------------------------------- //

    LogStream error();

    void error(const char *message);

    void error(const std::string &message);

    void error(const QString &message);
   // --- ERROR -------------------------------------- //

    LogStream trace();

    void trace(const char *message);

    void trace(const std::string &message);

    void trace(const QString &message);

    // --- LOGGER ------------------------------------- //

    void log(LogLevel level, const std::string &message);

    void log(LogLevel level, const LogMessage *message);

    void log(LogLevel level, const QString &message);

  private:
    LogLevel m_logLevel =
        #ifdef NDEBUG
        LogLevel::Trace;
    #else
    LogLevel::Info;
    #endif

    LogMessage *createLogMessage(LogLevel level, const QString &message);

    virtual void doLog(LogLevel level, const LogMessage *message) = 0;
};

/* ------------------------------------------------------------------------------------------- */

class NullLogger : public Logger {
  private:

    void doLog(LogLevel level, const LogMessage *message) override;
};

/* ------------------------------------------------------------------------------------------- */


class DefaultQtLogger : public Logger {
  private:
    void doLog(LogLevel level, const LogMessage *message) override;

    bool coloredOut() const;

    void setColoredOut(bool mColoredOut);

  private:
    bool m_coloredOut = true;
    QString m_prefix;
    QString m_rawMessage;
};

/**
 * Default static instance.
 */
static DefaultQtLogger defaultQtLogger = DefaultQtLogger();

} // namespace TrenchBroom
