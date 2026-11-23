#pragma once

class QApplication;
class QEvent;
class QObject;

namespace TrenchBroom::View {

bool widgetFontStashAvailable();
bool notifyWidgetTextPaint(QApplication& app, QObject* receiver, QEvent* event);

// Use Qt glyph coverage for a reference sample or an explicit local override.
class ScopedQtTextPaint {
    bool m_previous;
public:
    ScopedQtTextPaint();
    ~ScopedQtTextPaint();
    ScopedQtTextPaint(const ScopedQtTextPaint&) = delete;
    ScopedQtTextPaint& operator=(const ScopedQtTextPaint&) = delete;
};

} // namespace TrenchBroom::View
