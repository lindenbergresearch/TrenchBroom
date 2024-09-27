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

#include "Logger.h"
#include "StringUtils.h"

#include <QString>

#include <string>

namespace TrenchBroom {

/* ------------------------------------------------------------------------------------------- */


Logger::~Logger() {}

Logger::LogStream Logger::trace() {
    return Logger::LogStream(this, LogLevel::Trace);
}

void Logger::trace(const char *message) {
    trace(QString(message));
}

void Logger::trace(const std::string &message) {
    log(LogLevel::Trace, message);
}

void Logger::trace(const QString &message) {
    log(LogLevel::Trace, message);
}

Logger::LogStream Logger::debug() {
    return Logger::LogStream(this, LogLevel::Debug);
}


void Logger::debug(const char *message) {
    debug(QString(message));
}

void Logger::debug(const std::string &message) {
    log(LogLevel::Debug, message);
}

void Logger::debug(const QString &message) {
    log(LogLevel::Debug, message);
}

Logger::LogStream Logger::info() {
    return LogStream(this, LogLevel::Info);
}

void Logger::info(const char *message) {
    info(QString(message));
}

void Logger::info(const std::string &message) {
    log(LogLevel::Info, message);
}

void Logger::info(const QString &message) {
    log(LogLevel::Info, message);
}

Logger::LogStream Logger::warn() {
    return LogStream(this, LogLevel::Warn);
}

void Logger::warn(const char *message) {
    warn(QString(message));
}

void Logger::warn(const std::string &message) {
    log(LogLevel::Warn, message);
}

void Logger::warn(const QString &message) {
    log(LogLevel::Warn, message);
}

Logger::LogStream Logger::error() {
    return LogStream(this, LogLevel::Error);
}

void Logger::error(const char *message) {
    error(QString(message));
}

void Logger::error(const std::string &message) {
    log(LogLevel::Error, message);
}

void Logger::error(const QString &message) {
    log(LogLevel::Error, message);
}


void Logger::log(const LogLevel level, const std::string &message) {
    log(level, QString::fromStdString(message));
}

void Logger::log(const LogLevel level, const QString &message) {
    // check if log-level is active
    if (level < m_logLevel) {
        return;
    }

    // create log-message
    auto *msg = createLogMessage(level, message);
    // add to cache
    LogMessageCache::add(msg);

    // call subclass implementation
    doLog(level, msg);

}

void Logger::log(const LogLevel level, const LogMessage *message) {
    // check if log-level is active
    if (level < m_logLevel) {
        return;
    }

    // call subclass implementation
    doLog(level, message);
}

/* ------------------------------------------------------------------------------------------- */


LogMessage *Logger::createLogMessage(LogLevel level, const QString &message) {
    return new LogMessage{level, message};
}

LogLevel Logger::logLevel() const {
    return m_logLevel;
}

void Logger::setLogLevel(LogLevel logLevel) {
    m_logLevel = logLevel;
}

/* ------------------------------------------------------------------------------------------- */

void NullLogger::doLog(const LogLevel level, const LogMessage *) {}

/* ------------------------------------------------------------------------------------------- */


void DefaultQtLogger::doLog(LogLevel level, const LogMessage *message) {
    switch (level) {
        case LogLevel::Trace:
            qDebug().noquote() << message->format(true, m_coloredOut);
            break;
        case LogLevel::Debug:
            qDebug().noquote() << message->format(true, m_coloredOut);
            break;
        case LogLevel::Info:
            qInfo().noquote() << message->format(true, m_coloredOut);
            break;
        case LogLevel::Warn:
            qWarning().noquote() << message->format(true, m_coloredOut);
            break;
        case LogLevel::Error:
            qCritical().noquote() << message->format(true, m_coloredOut);
            break;
    }
}

bool DefaultQtLogger::coloredOut() const {
    return m_coloredOut;
}

void DefaultQtLogger::setColoredOut(bool mColoredOut) {
    m_coloredOut = mColoredOut;
}

/* ------------------------------------------------------------------------------------------- */

std::map<size_t, LogMessage *> LogMessageCache::cache{};
size_t LogMessageCache::m_id{0};


void LogMessageCache::add(LogMessage *logMessage) {
    cache[m_id++] = logMessage;
}

LogMessage *LogMessageCache::get(size_t id) {
    return cache[id];
}

void LogMessageCache::clear() {
    cache.clear();
}

size_t LogMessageCache::size() {
    return cache.size();
}

size_t LogMessageCache::currentID() {
    return m_id;
}

QString LogMessage::format(bool detailed, bool colored) const {
    auto attr = levelAttributes[level];
    QString msgStr =
        colored ?
        attr.format :
        QString{};

    if (detailed) {
        msgStr += QString::fromStdString(
            stringf(
                "[%09.3f] %s %s",
                time,
                attr.label.toStdString().c_str(),
                message.toStdString().c_str()
            )
        );
    } else {
        // just the plain message without details
        msgStr += message;
    }

    if (colored) {
        // reset color
        msgStr += "\033[0m";
    }

    return msgStr;
}
} // namespace TrenchBroom
