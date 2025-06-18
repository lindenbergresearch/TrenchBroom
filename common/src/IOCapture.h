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

#include <iostream>
#include <streambuf>
#include <sstream>
#include <cstdio>
#include <vector>
#include <functional>
#include <string>

#include <QTimer>

namespace TrenchBroom {

namespace IOCapture {
static const long POLL_TIMER_INTERVAL = 1000;
}


class OutputCapture : public QObject {
  public:
    enum OutType {
      StandardOut,
      StandardError
    };

    // Struct to store captured output data
    struct OutputData {
      std::string message;
      OutType type; // True for stdout, false for stderr
    };

    /* ------------------------------------------------------------------------------------------- */

  private:
    bool m_active;
    std::vector<OutputData> m_capturedData;
    std::function<void(const OutputData &)> onNewDataCallback;

    QTimer *m_pollTimer;

    // Original stream buffers for cout and cerr
    std::streambuf *m_prevCOUTBuff;
    std::streambuf *m_prevCERRBuff;

    std::basic_ostream<char> *m_savedCOUT;
    std::basic_ostream<char> *m_savedCERR;

    // String buffers for capturing cout and cerr data
    std::ostringstream m_COUTBuffer;
    std::ostringstream m_CERRBuffer;

    /* ------------------------------------------------------------------------------------------- */

  public:
    // Constructor initializes capture state
    OutputCapture() : m_active(false), onNewDataCallback(nullptr), m_pollTimer(new QTimer{this}) {
    }

    // Destructor stops capture and restores original streams
    ~OutputCapture() {
        stopCapture();
    }

    /* ------------------------------------------------------------------------------------------- */

    // Starts capturing output
    void startCapture() {
        if (m_active) { return; }

        // Redirect std::cout and std::cerr to internal buffers
        m_prevCOUTBuff = std::cout.rdbuf(m_COUTBuffer.rdbuf());
        m_prevCERRBuff = std::cerr.rdbuf(m_CERRBuffer.rdbuf());

        // create a stream for cout and cerr
        m_savedCOUT = new std::basic_ostream<char>(m_prevCOUTBuff);
        m_savedCERR = new std::basic_ostream<char>(m_prevCERRBuff);

        m_active = true;
        m_pollTimer->start(IOCapture::POLL_TIMER_INTERVAL);

        connect(m_pollTimer, &QTimer::timeout, this, &OutputCapture::poll);
    }

    // Stops capturing output and restores original streams
    void stopCapture() {
        if (!m_active) return;

        m_pollTimer->stop();

        // Restore original buffers
        std::cout.rdbuf(m_prevCOUTBuff);
        std::cerr.rdbuf(m_prevCERRBuff);

        m_savedCOUT->rdbuf(nullptr);
        m_savedCERR->rdbuf(nullptr);

        delete m_savedCOUT;
        delete m_savedCERR;

        m_active = false;
    }

    // Method to be called regularly to poll captured data
    void poll() {
       printf("poll: %s\n", m_active ? "yes" : "no");
        if (m_active) {
            // Check if there is new data in the cout buffer
            std::string coutData = m_COUTBuffer.str();
            printf("printf: %s", coutData.c_str());
            if (!coutData.empty()) {
                m_COUTBuffer.str(""); // Clear the buffer
                captureData(coutData, OutType::StandardOut); // Store as stdout data
                *m_savedCOUT << " COUT -> " << coutData.c_str();

            }

            // Check if there is new data in the cerr buffer
            std::string cerrData = m_CERRBuffer.str();
            if (!cerrData.empty()) {
                m_CERRBuffer.str(""); // Clear the buffer
                captureData(cerrData, OutType::StandardError); // Store as stderr data
                *m_savedCERR << " CERR -> " << cerrData.c_str();
            }
        }
    }

    // Set a callback to intercept data when it's captured
    void setOnNewDataCallback(std::function<void(const OutputData &)> callback) {
        onNewDataCallback = callback;
    }

    // Access captured data
    const std::vector<OutputData> &getCapturedData() const {
        return m_capturedData;
    }

  private:
    // Internal function to process captured data
    void captureData(const std::string &data, OutType type) {
        // Split data by newlines to handle multiline output
        std::istringstream stream(data);
        std::string line;
        while (std::getline(stream, line)) {
            OutputData entry{line, type};
            m_capturedData.push_back(entry);

            // If a callback is set, call it with new data
            if (onNewDataCallback) {
                onNewDataCallback(entry);
            }
        }
    }
};


}
