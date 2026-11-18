#include "View/Widgets/Info/SemanticTextEdit.h"
#include "View/Preferences/FontUtils.h"

#include <QElapsedTimer>
#include <QEvent>
#include <QTextBlock>
#include <QTextFragment>
#include <QTimer>
#include <algorithm>
#include <vector>

namespace TrenchBroom::View {
SemanticTextEdit::SemanticTextEdit(QWidget* parent) : QTextEdit(parent) {
    setReadOnly(true);
    setUndoRedoEnabled(false);
}

QColor SemanticTextEdit::messageColor(MessageStyle role) const {
    const auto& color = m_colors[size_t(role)];
    return color.isValid() ? color : palette().color(QPalette::Text);
}

void SemanticTextEdit::setMessageColor(MessageStyle role, QColor color) {
    auto& current = m_colors[size_t(role)];
    if (current == color) return;
    current = color;
    scheduleRestyle();
}

void SemanticTextEdit::setSystemFontWeight(int weight) {
    weight = normalizedFontWeight(weight);
    if (weight == m_systemFontWeight) return;
    m_systemFontWeight = weight;
    scheduleRestyle();
}

QTextCharFormat SemanticTextEdit::messageFormat(const QTextEdit& edit, MessageStyle role, bool emphasized) {
    const auto* styled = qobject_cast<const SemanticTextEdit*>(&edit);
    QTextCharFormat format;
    format.setForeground(styled ? styled->messageColor(role) : edit.palette().color(QPalette::Text));
    format.setProperty(RoleProperty, int(role));
    format.setProperty(EmphasisProperty, emphasized);
    if (emphasized) format.setFontWeight(styled ? styled->systemFontWeight() : edit.font().weight());
    return format;
}

void SemanticTextEdit::changeEvent(QEvent* event) {
    QTextEdit::changeEvent(event);
    if (event->type() == QEvent::StyleChange || event->type() == QEvent::PaletteChange || event->type() == QEvent::FontChange) scheduleRestyle();
}

void SemanticTextEdit::scheduleRestyle() {
    m_restylePosition = 0;
    queueRestyle();
}

void SemanticTextEdit::queueRestyle() {
    if (m_restyleQueued) return;
    m_restyleQueued = true;
    QTimer::singleShot(0, this, [this] {
        m_restyleQueued = false;
        restyleChunk();
    });
}

void SemanticTextEdit::restyleChunk() {
    auto block = document()->findBlock(m_restylePosition);
    QElapsedTimer elapsed;
    elapsed.start();
    QTextCursor cursor(document());
    cursor.beginEditBlock();
    for (int count = 0; block.isValid() && count < 128 && elapsed.elapsed() < 5; ++count) {
        struct Range {
            int position, length;
            QTextCharFormat format;
        };
        std::vector<Range> ranges;
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const auto fragment = it.fragment();
            if (!fragment.isValid()) continue;
            const auto original = fragment.charFormat();
            if (!original.hasProperty(RoleProperty)) continue;
            const auto role = original.property(RoleProperty).toInt();
            if (role < 0 || role >= int(m_colors.size())) continue;
            auto format = original;
            format.setForeground(messageColor(MessageStyle(role)));
            if (original.property(EmphasisProperty).toBool()) format.setFontWeight(m_systemFontWeight);
            if (format != original) ranges.push_back({fragment.position(), fragment.length(), format});
        }
        for (const auto& range : ranges) {
            cursor.setPosition(range.position);
            cursor.setPosition(range.position + range.length, QTextCursor::KeepAnchor);
            cursor.setCharFormat(range.format);
        }
        m_restylePosition = block.position() + block.length();
        block = block.next();
    }
    cursor.endEditBlock();
    if (block.isValid()) queueRestyle();
}
} // namespace TrenchBroom::View
