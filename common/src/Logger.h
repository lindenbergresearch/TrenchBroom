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

#include <QString>

#include <type_traits>

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
    {LogLevel::Trace, {"Trace", "\033[0;90m"}},
    {LogLevel::Debug, {"Debug", "\033[0;36m"}},
    {LogLevel::Info, {"Info ", "\033[0;97m"}},
    {LogLevel::Warn, {"Warn ", "\033[0;33m"}},
    {LogLevel::Error, {"Error", "\033[0;31m"}},
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
     * @class LogStream
     * @brief A logging stream class that provides an interface for logging messages using stream syntax.
     *
     * This class enables logging with a syntax similar to C++ stream insertion (<<) operators.
     * It supports logging various data types, including std::string, QString, and primitive types.
     * Messages are buffered in a stringstream and logged upon destruction.
     */
    class LogStream {
      private:
        Logger *m_logger;         ///< Pointer to the Logger instance.
        LogLevel m_logLevel;      ///< The log level for the current message.
        std::stringstream m_buf;  ///< Buffer for the log message.

      public:
        /**
         * @brief Constructs a LogStream object.
         *
         * @param logger Pointer to the Logger instance that will handle the output.
         * @param logLevel The severity level of the log message.
         */
        LogStream(Logger *logger, LogLevel logLevel)
            : m_logger(logger), m_logLevel(logLevel) {}

        /**
         * @brief Destructor for LogStream.
         *
         * The destructor automatically sends the buffered message to the logger when
         * the LogStream object goes out of scope.
         */
        ~LogStream() {
            // Send the accumulated message in the buffer to the logger
            m_logger->log(m_logLevel, m_buf.str());
        }

        /**
         * @brief Stream insertion operator for general types.
         *
         * This templated operator allows insertion of various types into the log stream,
         * excluding QString and std::string, which have explicit overloads.
         *
         * @tparam T The type of the argument to be inserted.
         * @param arg The value to be inserted into the log stream.
         * @return Reference to the current LogStream object to allow chaining.
         */
        template<typename T, typename std::enable_if<!std::is_same<T, QString>::value, int>::type = 0>
        LogStream &operator<<(T &&arg) {
            m_buf << std::forward<T>(arg);
            return *this;
        }

        /**
         * @brief Stream insertion operator for std::string.
         *
         * Allows insertion of std::string into the log stream.
         *
         * @param arg The std::string to be inserted.
         * @return Reference to the current LogStream object to allow chaining.
         */
        LogStream &operator<<(const std::string &arg) {
            m_buf << arg;  // std::string is directly compatible with std::stringstream
            return *this;
        }

        /**
         * @brief Stream insertion operator for QString.
         *
         * Allows insertion of QString into the log stream by converting it to std::string.
         * This overload ensures QString is converted before inserting it into the stringstream.
         *
         * @param arg The QString to be inserted.
         * @return Reference to the current LogStream object to allow chaining.
         */
        LogStream &operator<<(const QString &arg) {
            m_buf << arg.toStdString();  // Convert QString to std::string
            return *this;
        }

        /**
         * @brief Stream insertion operator for C-style strings.
         *
         * Allows insertion of const char* (C-style strings) into the log stream.
         *
         * @param arg The C-style string to be inserted.
         * @return Reference to the current LogStream object to allow chaining.
         */
        LogStream &operator<<(const char *arg) {
            m_buf << arg;  // const char* is compatible with std::stringstream
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

    LogLevel logLevel() const;

    void setLogLevel(LogLevel logLevel);

  private:
    LogLevel m_logLevel = LogLevel::Debug;

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
