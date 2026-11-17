#pragma once

#include <QColor>
#include <QDialog>
#include <QPlainTextEdit>
#include <QString>

#include <vector>

namespace TrenchBroom::Model {
class Node;
}

namespace TrenchBroom::View {
class MapDocument;

struct NodeSourcePage {
    QString title;
    QString location;
    QString text;
    bool fgd = false;
    int revealLine = 0;
};

// Produces snapshots without changing the editor selection or MAP file positions.
std::vector<NodeSourcePage> nodeSourcePages(const MapDocument& document, const Model::Node& node);

class SourceTextEdit : public QPlainTextEdit {
    Q_OBJECT
    Q_PROPERTY(QColor keywordColor MEMBER m_keywordColor NOTIFY syntaxColorsChanged)
    Q_PROPERTY(QColor stringColor MEMBER m_stringColor NOTIFY syntaxColorsChanged)
    Q_PROPERTY(QColor numberColor MEMBER m_numberColor NOTIFY syntaxColorsChanged)
    Q_PROPERTY(QColor commentColor MEMBER m_commentColor NOTIFY syntaxColorsChanged)
    Q_PROPERTY(QColor propertyColor MEMBER m_propertyColor NOTIFY syntaxColorsChanged)
  private:
    QColor m_keywordColor, m_stringColor, m_numberColor, m_commentColor, m_propertyColor;

  public:
    explicit SourceTextEdit(bool fgd, QWidget* parent = nullptr);
    QColor keywordColor() const { return m_keywordColor; }
    QColor stringColor() const { return m_stringColor; }
    QColor numberColor() const { return m_numberColor; }
    QColor commentColor() const { return m_commentColor; }
    QColor propertyColor() const { return m_propertyColor; }
  signals:
    void syntaxColorsChanged();
};

class NodeSourceDialog : public QDialog {
    Q_OBJECT
  public:
    NodeSourceDialog(const MapDocument& document, const Model::Node& node, QWidget* parent = nullptr);
};
} // namespace TrenchBroom::View
