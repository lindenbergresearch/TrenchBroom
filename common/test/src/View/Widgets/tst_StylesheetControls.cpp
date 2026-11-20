#include "TestSourcePath.h"
#include "View/Theme/QSSBuilder.h"
#include "View/Theme/ThemeManager.h"
#include "View/Compilation/TextOutputAdapter.h"
#include "View/Preferences/IconSelectionDialog.h"
#include "View/Widgets/BorderLine.h"
#include "View/Widgets/BorderPanel.h"
#include "View/Widgets/ColorButton.h"
#include "View/Widgets/Info/SemanticTextEdit.h"
#include "View/Widgets/Lists/StyledItemDelegate.h"
#include "View/Theme/QtUtils.h"
#include <QApplication>
#include <QLabel>
#include <QListWidget>
#include <QStandardItemModel>
#include <QStyleOptionViewItem>
#include <QTableView>
#include <QTextBlock>
#include <QToolButton>
#include <QTreeWidget>
#include <catch2/catch.hpp>

namespace TrenchBroom::View {
namespace {
    QTextCharFormat characterFormat(QTextEdit& edit, int position) {
        QTextCursor cursor(edit.document());
        cursor.setPosition(position);
        cursor.setPosition(position + 1, QTextCursor::KeepAnchor);
        return cursor.charFormat();
    }
    QString controlStyleSheet() {
        ThemeSnapshot snapshot;
        QStringList errors;
        const QFont font = QApplication::font();
        REQUIRE(ThemeManager::prepare(font, font, font, testSourceRoot() / "app/resources/stylesheets/theme.json", {}, snapshot, errors));
        QSSBuilder builder;
        REQUIRE(builder.loadFile(testSourceRoot() / "app/resources/stylesheets/base.qss"));
        REQUIRE(builder.update(snapshot.values));
        return builder.getRenderedText();
    }
    class CellDelegate : public StyledItemDelegate {
      public:
        using StyledItemDelegate::StyledItemDelegate;
        QStyleOptionViewItem options(QAbstractItemView& view, const QModelIndex& index) const {
            QStyleOptionViewItem option;
            option.initFrom(&view);
            option.widget = &view;
            initStyleOption(&option, index);
            return option;
        }
    };
} // namespace
TEST_CASE("QSS styles existing messages while preserving external ANSI output", "[qss-controls]") {
    SemanticTextEdit edit;
    edit.setStyleSheet("TrenchBroom--View--SemanticTextEdit { font-size: 15px; qproperty-errorColor: #cc1122; qproperty-systemFontWeight: 75; }");
    edit.ensurePolished();
    TextOutputAdapter output(&edit);
    output.pushSystemMessage("Error\n", MessageStyle::Error);
    output << "\033[32mExternal\033[0m\n";
    CHECK(characterFormat(edit, 0).foreground().color() == QColor("#cc1122"));
    CHECK(characterFormat(edit, 0).fontWeight() == QFont::Bold);
    const auto ansi = characterFormat(edit, 6).foreground().color();
    CHECK_FALSE(characterFormat(edit, 6).hasProperty(SemanticTextEdit::RoleProperty));
    for (int i = 0; i < 300; ++i)
        output.pushSystemMessage("Error\n", MessageStyle::Error);
    edit.setStyleSheet("TrenchBroom--View--SemanticTextEdit { font-size: 19px; qproperty-errorColor: #2255bb; qproperty-systemFontWeight: 50; }");
    for (int i = 0; i < 20; ++i)
        QApplication::processEvents();
    CHECK(characterFormat(edit, 0).foreground().color() == QColor("#2255bb"));
    CHECK(characterFormat(edit, 0).fontWeight() == QFont::Normal);
    CHECK(characterFormat(edit, edit.toPlainText().lastIndexOf("Error")).foreground().color() == QColor("#2255bb"));
    CHECK(characterFormat(edit, 6).foreground().color() == ansi);
    CHECK(edit.font().pixelSize() == 19);
    CHECK_FALSE(characterFormat(edit, 0).hasProperty(QTextFormat::FontPixelSize));
}
TEST_CASE("Table cell fonts follow QSS without replacing data colors", "[qss-controls]") {
    QTableView view;
    QStandardItemModel model(1000, 1);
    view.setModel(&model);
    view.show();
    QApplication::processEvents();
    CellDelegate delegate("preferences", &view);
    for (int i = 0; i < model.rowCount(); ++i) {
        model.setData(model.index(i, 0), "value", StyledItemDelegate::TextStyleRole);
        model.setData(model.index(i, 0), QColor("#ee9933"), Qt::ForegroundRole);
    }
    view.setStyleSheet("QWidget#ItemCellStyle[cellContext=\"preferences\"][cellRole=\"value\"] { font-size: 17px; font-weight: bold; color: #22bb33; qproperty-overrideTextColor: false; }");
    for (int i = 0; i < model.rowCount(); ++i)
        CHECK(delegate.options(view, model.index(i, 0)).font.pixelSize() == 17);
    auto option = delegate.options(view, model.index(0, 0));
    CHECK(option.font.bold());
    CHECK(option.palette.color(QPalette::Text) == QColor("#ee9933"));
    CHECK(view.findChildren<ItemCellStyle*>().size() == 1);
    view.setStyleSheet("QWidget#ItemCellStyle[cellContext=\"preferences\"][cellRole=\"value\"] { font-size: 21px; color: #22bb33; qproperty-overrideTextColor: true; }");
    option = delegate.options(view, model.index(0, 0));
    CHECK(option.font.pixelSize() == 21);
    CHECK(option.palette.color(QPalette::Text) == QColor("#22bb33"));
    model.setData(model.index(0, 0), "Editable value", Qt::EditRole);
    view.setStyleSheet(view.styleSheet() + "\nQWidget#StyledCellEditor[cellContext=\"preferences\"][cellRole=\"value\"] { font-size: 22px; }");
    auto* editor = delegate.createEditor(view.viewport(), option, model.index(0, 0));
    REQUIRE(editor);
    editor->ensurePolished();
    CHECK(editor->property("cellRole").toString() == "value");
    CHECK(editor->font().pixelSize() == 22);
}
TEST_CASE("Shared controls get sizes borders and icons from QSS", "[qss-controls]") {
    QWidget host;
    host.setStyleSheet(controlStyleSheet());
    host.ensurePolished();
    auto* button = createBitmapButton("Group.svg", "Group", &host);
    button->ensurePolished();
    CHECK(button->iconSize() == QSize(16, 16));
    host.setStyleSheet(controlStyleSheet() + "\nQToolButton#toolButton_borderless { qproperty-iconSize: 27px 27px; }");
    CHECK(button->iconSize() == QSize(27, 27));
    BorderLine line(BorderLine::Direction::Horizontal, 1, &host);
    line.setStyleSheet("QFrame#borderLine { qproperty-thickness: 7; background-color: #12ab34; }");
    line.ensurePolished();
    CHECK(line.sizeHint().height() == 7);
    BorderPanel panel(BorderPanel::AllSides, 1, &host);
    panel.setStyleSheet("TrenchBroom--View--BorderPanel { qproperty-borderWidth: 4; qproperty-borderColor: #ee1133; background-color: #223344; }");
    panel.resize(30, 30);
    panel.ensurePolished();
    const auto image = panel.grab().toImage();
    CHECK(panel.borderWidth() == 4);
    CHECK(image.pixelColor(0, 15) == QColor("#ee1133"));
    CHECK(image.pixelColor(15, 15) == QColor("#223344"));
    ColorButton color;
    color.setStyleSheet(controlStyleSheet());
    color.resize(200, 40);
    color.show();
    QApplication::processEvents();
    color.setColor(QColor("#34cc56"));
    auto* swatch = color.findChild<QWidget*>("ColorSwatch");
    REQUIRE(swatch);
    CHECK(swatch->styleSheet().isEmpty());
    const auto swatchImage = swatch->grab().toImage();
    REQUIRE_FALSE(swatchImage.isNull());
    CHECK(swatchImage.pixelColor(swatchImage.rect().center()) == QColor("#34cc56"));
}
TEST_CASE("History and diagnostic rows use shared theme fonts and semantic QSS colors", "[qss-controls][history][load-diagnostics]") {
    QTreeWidget view;
    view.setObjectName("HistoryList");
    view.setColumnCount(2);
    auto* row = new QTreeWidgetItem(&view, {"Move objects", "12:00:00"});
    row->setData(0, StyledItemDelegate::TextStyleRole, "action");
    row->setData(0, StyledItemDelegate::TextStatesRole, QStringList{"current"});
    row->setData(1, StyledItemDelegate::TextStyleRole, "time");
    view.setStyleSheet(controlStyleSheet());
    view.ensurePolished();
    CellDelegate history("history", &view);
    const auto index = view.model()->index(0, 0);
    CHECK(history.options(view, index).font.bold());
    view.setStyleSheet(controlStyleSheet() + "\nQWidget#ItemCellStyle[cellContext=\"history\"][cellRole=\"time\"] { font-size: 19px; color: #12ab34; }");
    const auto time = history.options(view, view.model()->index(0, 1));
    CHECK(time.font.pixelSize() == 19);
    CHECK(time.palette.color(QPalette::Text) == QColor("#12ab34"));
    row->setData(0, StyledItemDelegate::TextStyleRole, "error");
    row->setData(0, StyledItemDelegate::TextStatesRole, QStringList{});
    view.setObjectName("MapLoadDiagnosticsList");
    view.setStyleSheet(controlStyleSheet() + "\nQWidget#ItemCellStyle[cellContext=\"loadDiagnostics\"][cellRole=\"error\"] { color: #bc1234; }");
    CellDelegate diagnostics("loadDiagnostics", &view);
    CHECK(diagnostics.options(view, index).palette.color(QPalette::Text) == QColor("#bc1234"));
}
TEST_CASE("SVG picker grid and preview dimensions follow a stylesheet override", "[qss-controls]") {
    IconSelectionDialog dialog("Group.svg");
    dialog.setStyleSheet(controlStyleSheet() + "\nQListWidget#EditorIconGrid { qproperty-iconSize: 64px 64px; qproperty-gridSize: 144px 112px; }\nQLabel#EditorIconPreview { min-width: "
                                               "96px; max-width: 96px; min-height: 96px; max-height: 96px; }");
    auto* grid = dialog.findChild<QListWidget*>("EditorIconGrid");
    auto* preview = dialog.findChild<QLabel*>("EditorIconPreview");
    REQUIRE(grid);
    REQUIRE(preview);
    dialog.show();
    QApplication::processEvents();
    QApplication::processEvents();
    CHECK(grid->iconSize() == QSize(64, 64));
    CHECK(grid->gridSize() == QSize(144, 112));
    REQUIRE_FALSE(preview->pixmap().isNull());
    CHECK(preview->pixmap().width() / preview->pixmap().devicePixelRatioF() == Approx(preview->contentsRect().width()));
}
} // namespace TrenchBroom::View
