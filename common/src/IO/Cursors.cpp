#include "Cursors.h"
#include "IO/ResourceUtils.h"

using namespace TrenchBroom::IO;

AppCursor::AppCursor(const Preferences::Path &source, CursorType type, const QPixmap &pixmap)
    : m_source(source), m_type(type), m_pixmap(pixmap) {}
