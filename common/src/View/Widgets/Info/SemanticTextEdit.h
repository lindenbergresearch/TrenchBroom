#pragma once

#include <QTextEdit>
#include <array>

namespace TrenchBroom::View {
enum class MessageStyle { Normal, Trace, Debug, Info, Warning, Error, Success, System };

// Rich-text fragments carry a semantic role instead of a fixed UI color/font.
// ANSI colors supplied by external processes are intentionally left untagged.
class SemanticTextEdit : public QTextEdit {
    Q_OBJECT
    Q_PROPERTY(QColor traceColor READ traceColor WRITE setTraceColor)
    Q_PROPERTY(QColor debugColor READ debugColor WRITE setDebugColor)
    Q_PROPERTY(QColor infoColor READ infoColor WRITE setInfoColor)
    Q_PROPERTY(QColor warningColor READ warningColor WRITE setWarningColor)
    Q_PROPERTY(QColor errorColor READ errorColor WRITE setErrorColor)
    Q_PROPERTY(QColor successColor READ successColor WRITE setSuccessColor)
    Q_PROPERTY(QColor systemColor READ systemColor WRITE setSystemColor)
    Q_PROPERTY(int systemFontWeight READ systemFontWeight WRITE setSystemFontWeight)
  public:
    static constexpr int RoleProperty = QTextFormat::UserProperty + 96;
    static constexpr int EmphasisProperty = RoleProperty + 1;
    explicit SemanticTextEdit(QWidget* parent = nullptr);

    QColor traceColor() const { return messageColor(MessageStyle::Trace); }
    QColor debugColor() const { return messageColor(MessageStyle::Debug); }
    QColor infoColor() const { return messageColor(MessageStyle::Info); }
    QColor warningColor() const { return messageColor(MessageStyle::Warning); }
    QColor errorColor() const { return messageColor(MessageStyle::Error); }
    QColor successColor() const { return messageColor(MessageStyle::Success); }
    QColor systemColor() const { return messageColor(MessageStyle::System); }
    int systemFontWeight() const { return m_systemFontWeight; }
    void setTraceColor(QColor color) { setMessageColor(MessageStyle::Trace, color); }
    void setDebugColor(QColor color) { setMessageColor(MessageStyle::Debug, color); }
    void setInfoColor(QColor color) { setMessageColor(MessageStyle::Info, color); }
    void setWarningColor(QColor color) { setMessageColor(MessageStyle::Warning, color); }
    void setErrorColor(QColor color) { setMessageColor(MessageStyle::Error, color); }
    void setSuccessColor(QColor color) { setMessageColor(MessageStyle::Success, color); }
    void setSystemColor(QColor color) { setMessageColor(MessageStyle::System, color); }
    void setSystemFontWeight(int weight);
    static QTextCharFormat messageFormat(const QTextEdit& edit, MessageStyle role, bool emphasized = false);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    std::array<QColor, 8> m_colors;
    int m_systemFontWeight = QFont::Bold;
    int m_restylePosition = 0;
    bool m_restyleQueued = false;
    QColor messageColor(MessageStyle role) const;
    void setMessageColor(MessageStyle role, QColor color);
    void scheduleRestyle();
    void queueRestyle();
    void restyleChunk();
};
} // namespace TrenchBroom::View
