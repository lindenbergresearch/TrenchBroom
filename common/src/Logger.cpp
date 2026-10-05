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
#include "Preferences.h"
#include "PreferenceManager.h"

#include <QString>
#include <string>

namespace TrenchBroom {
/* ------------------------------------------------------------------------------------------- */


Logger::~Logger() {}

Logger::LogStream Logger::debug() {
    return LogStream(this, LogLevel::Debug);
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

Logger::LogStream Logger::trace() {
    return LogStream(this, LogLevel::Trace);
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

LogLevel Logger::resolveLogLevel() {
    if (m_logLevel == LogLevel::None) {
        // if current loglevel still not initialized but preferences manager available
        // do read loglevel from preferences
        if (PreferenceManager::isInitialized()) {
            m_logLevel = pref(Preferences::AppLogLevel);
        } else {
            // not initialized return debug loglevel
            return LogLevel::Debug;
        }
    }

    return m_logLevel;
}


void Logger::log(const LogLevel level, const std::string &message) {
    log(level, QString::fromStdString(message));
}

void Logger::log(const LogLevel level, const LogMessage *message) {
    // call subclass implementation
    if (canLog(level)) doLog(level, message);
}

void Logger::log(const LogLevel level, const QString &message) {
    if (!canLog(level)) return;

    // create log-message
    auto *msg = createLogMessage(level, message);
    // add to cache
    LogMessageCache::add(msg);

    // call subclass implementation
    doLog(level, msg);
}

/* ------------------------------------------------------------------------------------------- */


LogMessage *Logger::createLogMessage(const LogLevel level, const QString &message) {
    return new LogMessage{level, message};
}

LogLevel Logger::logLevel() const {
    return m_logLevel;
}

void Logger::setLogLevel(const LogLevel logLevel) {
    m_logLevel = logLevel;
}

bool Logger::canLog(const LogLevel &msgLevel) {
    return msgLevel >= resolveLogLevel();
}

/* ------------------------------------------------------------------------------------------- */

void NullLogger::doLog(const LogLevel level, const LogMessage *message) {}

/* ------------------------------------------------------------------------------------------- */


void DefaultQtLogger::doLog(const LogLevel level, const LogMessage *message) {
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
        case LogLevel::None:
            break;
    }
}

bool DefaultQtLogger::coloredOut() const {
    return m_coloredOut;
}

void DefaultQtLogger::setColoredOut(const bool mColoredOut) {
    m_coloredOut = mColoredOut;
}

/* ------------------------------------------------------------------------------------------- */

QString LogMessage::format(const bool detailed, const bool colored) const {
    auto [label, format] = levelAttributes[level];
    auto msgStr = colored ? format : QString{};

    if (detailed) {
        msgStr += QString::fromStdString(
            stringf(
                "[%09.3f] %s %s",
                time,
                label.toStdString().c_str(),
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

void LogMessageCache::add(LogMessage *logMessage) {
    cache.push_back(logMessage);
}


LogMessage *LogMessageCache::get(const size_t id) {
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

std::vector<LogMessage *> LogMessageCache::cache{};

size_t LogMessageCache::m_id{};
} // namespace TrenchBroom
