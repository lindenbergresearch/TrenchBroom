#pragma once

#include <filesystem>

#include <PreferenceManager.h>
#include <Preferences.h>
#include <IO/ResourceUtils.h>

namespace TrenchBroom::IO {

/**
 * Denotes the type of the cursor file
 */
enum CursorType {
   Undefined,
   Default,
   Move,
   Select,
   Delete,
   Edit,
   Zoom
};

/* ------------------------------------------------------------------------------------------- */

class AppCursor {
  public:
    AppCursor() {
        m_type = Undefined;
    }

    AppCursor(const Preferences::Path &source, CursorType type, const QPixmap &pixmap);

    bool isValid() {
        return
            !m_pixmap.isNull() &&
            !m_source.empty() &&
            m_type != Undefined;
    }

    /* ------------------------------------------------------------------------------------------- */

    QCursor getOrCreateCursor(QPoint hotspot = QPoint{-1, -1}) {
        if (!m_qCursor.pixmap().isNull()) return m_qCursor;

        if (!isValid()) return QCursor{};

        m_qCursor = QCursor{m_pixmap, hotspot.x(), hotspot.y()};
        return m_qCursor;
    }

    /* ------------------------------------------------------------------------------------------- */

    const QPixmap &pixmap() const {
        return m_pixmap;
    }

    void setPixmap(const QPixmap &pixmap) {
        m_pixmap = pixmap;
    }

    CursorType type() const {
        return m_type;
    }

    void setType(CursorType type) {
        m_type = type;
    }

    const Preferences::Path &source() const {
        return m_source;
    }

    void setSource(const Preferences::Path &source) {
        m_source = source;
    }

  private:
    Preferences::Path m_source;
    CursorType m_type;
    QPixmap m_pixmap;
    QCursor m_qCursor;
};

/**
 *
 * @param source
 * @param type
 * @return
 */
static AppCursor loadAppCursor(const Preferences::Path &source, CursorType type = Default) {
    auto pixmap = loadPixmapResource(source);

    if (!pixmap.isNull())
        return AppCursor(source, type, pixmap);

    return AppCursor();
}

}
