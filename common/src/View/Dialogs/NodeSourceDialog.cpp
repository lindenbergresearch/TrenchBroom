#include "View/Dialogs/NodeSourceDialog.h"

#include "Assets/EntityDefinition.h"
#include "IO/NodeWriter.h"
#include "Model/EntityNode.h"
#include "Model/GroupNode.h"
#include "Model/LayerNode.h"
#include "Model/WorldNode.h"
#include "View/Document/MapDocument.h"
#include "View/Theme/QtUtils.h"

#include <QDialogButtonBox>
#include <QFile>
#include <QLabel>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTabWidget>
#include <QTextBlock>
#include <QVBoxLayout>

#include <algorithm>
#include <memory>
#include <sstream>

namespace TrenchBroom::View {
namespace {
    void preserveSourceIds(const Model::Node& source, Model::Node& copy) {
        if (const auto* group = dynamic_cast<const Model::GroupNode*>(&source); group && group->persistentId())
            static_cast<Model::GroupNode&>(copy).setPersistentId(*group->persistentId());
        if (const auto* layer = dynamic_cast<const Model::LayerNode*>(&source); layer && layer->persistentId())
            static_cast<Model::LayerNode&>(copy).setPersistentId(*layer->persistentId());
        for (size_t i = 0; i < copy.childCount(); ++i) preserveSourceIds(*source.children()[i], *copy.children()[i]);
    }

    std::unique_ptr<Model::Node> cloneSourceNode(const MapDocument& document, const Model::Node& node, const bool recursive) {
        auto copy = std::unique_ptr<Model::Node>{recursive ? node.cloneRecursively(document.worldBounds(), Model::SetLinkId::keep)
                                                         : node.clone(document.worldBounds(), Model::SetLinkId::keep)};
        // Clipboard clones intentionally get new IDs. A source snapshot must retain
        // the original group/layer IDs and their parent references.
        preserveSourceIds(node, *copy);
        return copy;
    }

    QString currentMapSource(const MapDocument& document, const Model::Node& node) {
        // MAP serialization updates file positions. Serialize an isolated branch so merely
        // inspecting source never overwrites the original nodes' and brush faces' positions.
        auto world = std::unique_ptr<Model::WorldNode>{static_cast<Model::WorldNode*>(document.world()->clone(document.worldBounds(), Model::SetLinkId::keep))};
        auto ancestors = std::vector<const Model::Node*>{};
        for (auto* parent = node.parent(); parent && parent != document.world(); parent = parent->parent())
            ancestors.push_back(parent);
        auto* parent = static_cast<Model::Node*>(world.get());
        for (auto it = ancestors.rbegin(); it != ancestors.rend(); ++it) {
            if (*it == document.world()->defaultLayer())
                parent = world->defaultLayer();
            else {
                auto clone = cloneSourceNode(document, **it, false);
                parent->addChild(clone.get());
                parent = clone.release();
            }
        }
        if (&node == document.world()->defaultLayer()) {
            for (const auto* child : node.children())
                world->defaultLayer()->addChild(cloneSourceNode(document, *child, true).release());
        } else if (&node == document.world()) {
            world.reset(static_cast<Model::WorldNode*>(cloneSourceNode(document, node, true).release()));
        } else {
            auto clone = cloneSourceNode(document, node, true);
            parent->addChild(clone.get());
            clone.release();
        }
        auto stream = std::ostringstream{};
        IO::NodeWriter{*world, stream}.writeMap();
        return QString::fromStdString(stream.str());
    }

    class SourceHighlighter : public QSyntaxHighlighter {
      private:
        SourceTextEdit& m_editor;
        bool m_fgd;

      public:
        SourceHighlighter(SourceTextEdit& editor, const bool fgd) : QSyntaxHighlighter(editor.document()), m_editor{editor}, m_fgd{fgd} {}

      protected:
        void highlightBlock(const QString& text) override {
            setCurrentBlockState(0);
            const auto paint = [&](const int start, const int length, const QColor& color) {
                if (color.isValid()) setFormat(start, length, color);
            };
            auto i = 0;
            auto firstString = true;
            auto inComment = previousBlockState() == 1;
            while (i < text.size()) {
                if (inComment || QStringView{text}.mid(i, 2) == u"/*") {
                    const auto end = text.indexOf("*/", i + (inComment ? 0 : 2));
                    paint(i, end < 0 ? text.size() - i : end + 2 - i, m_editor.commentColor());
                    if (end < 0) {
                        setCurrentBlockState(1);
                        return;
                    }
                    i = end + 2;
                    inComment = false;
                } else if (QStringView{text}.mid(i, 2) == u"//") {
                    paint(i, text.size() - i, m_editor.commentColor());
                    return;
                } else if (text[i] == '"') {
                    const auto start = i++;
                    while (i < text.size()) {
                        if (text[i] == '\\')
                            i += 2;
                        else if (text[i++] == '"')
                            break;
                    }
                    paint(start, std::min(i, int(text.size())) - start, !m_fgd && firstString ? m_editor.propertyColor() : m_editor.stringColor());
                    firstString = false;
                } else if (text[i].isLetter() || text[i] == '@' || text[i] == '_') {
                    const auto start = i++;
                    while (i < text.size() && (text[i].isLetterOrNumber() || text[i] == '_'))
                        ++i;
                    if (m_fgd) paint(start, i - start, m_editor.keywordColor());
                } else if (text[i].isDigit() || ((text[i] == '-' || text[i] == '+' || text[i] == '.') && i + 1 < text.size() && text[i + 1].isDigit())) {
                    static const auto number = QRegularExpression{R"([+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?)"};
                    const auto match = number.match(text, i, QRegularExpression::NormalMatch, QRegularExpression::AnchoredMatchOption);
                    if (match.hasMatch()) {
                        paint(i, match.capturedLength(), m_editor.numberColor());
                        i += match.capturedLength();
                    } else
                        ++i;
                } else
                    ++i;
            }
            if (inComment) setCurrentBlockState(1);
        }
    };
} // namespace

std::vector<NodeSourcePage> nodeSourcePages(const MapDocument& document, const Model::Node& node) {
    auto pages = std::vector<NodeSourcePage>{};
    if (const auto* entity = dynamic_cast<const Model::EntityNode*>(&node)) {
        const auto* definition = entity->entity().definition();
        if (definition && definition->source()) {
            const auto& source = *definition->source();
            pages.push_back(
                {QObject::tr("FGD definition"),
                 source.path.empty() ? QObject::tr("Game definition · line %1").arg(source.line) : QString::fromStdString(source.path.string()) + QObject::tr(" · line %1").arg(source.line),
                 QString::fromStdString(source.text), true});
        }
    }
    pages.push_back({QObject::tr("Current MAP fragment"), QObject::tr("Snapshot of the current editor state, including unsaved changes"), currentMapSource(document, node)});
    // Keep the on-disk file separate: after edits its line positions may no longer match.
    auto file = QFile{QString::fromStdString(document.path().string())};
    if (document.path().is_absolute() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        constexpr auto maxBytes = qint64{16 * 1024 * 1024};
        if (file.size() <= maxBytes)
            pages.push_back({QObject::tr("Saved MAP"), QString::fromStdString(document.path().string()) + QObject::tr(" · last saved version"), QString::fromUtf8(file.readAll()), false,
                             static_cast<int>(node.lineNumber())});
    }
    return pages;
}

SourceTextEdit::SourceTextEdit(const bool fgd, QWidget* parent) : QPlainTextEdit{parent} {
    setObjectName("nodeSourceText");
    setReadOnly(true);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    makeMono(this);
    auto* highlighter = new SourceHighlighter{*this, fgd};
    connect(this, &SourceTextEdit::syntaxColorsChanged, highlighter, &QSyntaxHighlighter::rehighlight);
}

NodeSourceDialog::NodeSourceDialog(const MapDocument& document, const Model::Node& node, QWidget* parent) : QDialog{parent, Qt::Tool} {
    setObjectName("nodeSourceDialog");
    setWindowTitle(tr("Object source"));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(680, 480);
    auto* layout = new QVBoxLayout{this};
    auto* tabs = new QTabWidget{this};
    tabs->setObjectName("nodeSourceTabs");
    for (const auto& page : nodeSourcePages(document, node)) {
        auto* container = new QWidget{tabs};
        auto* pageLayout = new QVBoxLayout{container};
        auto* location = new QLabel{page.location, container};
        location->setObjectName("nodeSourceLocation");
        location->setWordWrap(true);
        location->setTextFormat(Qt::PlainText);
        location->setTextInteractionFlags(Qt::TextSelectableByMouse);
        auto* editor = new SourceTextEdit{page.fgd, container};
        editor->setPlainText(page.text);
        if (page.revealLine > 0) {
            const auto block = editor->document()->findBlockByNumber(page.revealLine - 1);
            if (block.isValid()) {
                editor->setTextCursor(QTextCursor{block});
                editor->centerCursor();
            }
        }
        pageLayout->addWidget(location);
        pageLayout->addWidget(editor, 1);
        tabs->addTab(container, page.title);
    }
    layout->addWidget(tabs);
    auto* buttons = new QDialogButtonBox{QDialogButtonBox::Close, this};
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    layout->addWidget(buttons);
}
} // namespace TrenchBroom::View
