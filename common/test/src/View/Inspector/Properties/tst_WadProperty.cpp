#include "TestSourcePath.h"
#include "Error.h"
#include "IO/GameConfigParser.h"
#include "Logger.h"
#include "Model/GameImpl.h"
#include "View/Inspector/Properties/EntityPropertyEditor.h"
#include "View/Inspector/Properties/EntityPropertyModel.h"
#include "View/Inspector/Properties/EntityPropertyTable.h"
#include "View/Document/MapDocumentCommandFacade.h"
#include "View/Inspector/Properties/SmartWadEditor.h"

#include <QCoreApplication>
#include <QFile>
#include <QStyleOptionViewItem>
#include <QSplitter>
#include <catch2/catch.hpp>
#include <kdl/result.h>

namespace TrenchBroom::View {
TEST_CASE("WAD property preview preserves the full value and activates the selected editor", "[wad-property]") {
    const auto configPath = testSourceRoot() / "app/resources/games/Quake/GameConfig.cfg";
    QFile file(QString::fromStdString(configPath.string()));
    REQUIRE(file.open(QIODevice::ReadOnly));
    auto config = IO::GameConfigParser(file.readAll().toStdString(), configPath).parse();
    NullLogger logger;
    auto game = std::make_shared<Model::GameImpl>(config, std::filesystem::path{}, logger);
    auto document = MapDocumentCommandFacade::newMapDocument();
    REQUIRE(document->newDocument(Model::MapFormat::Standard, vm::bbox3(8192.0), game).is_success());
    document->deselectAll();
    const std::string paths = "/some/really/long/directory/first.wad;C:\\long\\directory\\second.wad;maps/third.wad";
    REQUIRE(document->setProperty("wad", paths));

    EntityPropertyEditor editor(document);
    QCoreApplication::processEvents();
    auto* table = editor.findChild<EntityPropertyTable*>();
    auto* wadEditor = editor.findChild<SmartWadEditor*>();
    REQUIRE(table);
    REQUIRE(wadEditor);
    QModelIndex wadValue, classname;
    auto* model = table->model();
    for (int row = 0; row < model->rowCount(); ++row) {
        const auto key = model->index(row, EntityPropertyModel::ColumnKey);
        if (key.data().toString() == "wad") wadValue = model->index(row, EntityPropertyModel::ColumnValue);
        if (key.data().toString() == "classname") classname = key;
    }
    REQUIRE(wadValue.isValid());
    REQUIRE(classname.isValid());
    CHECK(wadValue.data().toString() == "first.wad, second.wad (+1 more)");
    CHECK(wadValue.data(Qt::EditRole).toString() == QString::fromStdString(paths));

    // The row selection and the keyboard cursor can differ.
    table->selectionModel()->setCurrentIndex(classname, QItemSelectionModel::ClearAndSelect);
    CHECK(wadEditor->parentWidget()->isHidden());
    editor.resize(480, 600);
    editor.show();
    auto* splitter = editor.findChild<QSplitter*>("EntityAttributeEditor_Splitter");
    REQUIRE(splitter);
    splitter->setChildrenCollapsible(true);
    splitter->setSizes({400, 0, 60});
    splitter->setChildrenCollapsible(false);
    table->selectionModel()->select(wadValue, QItemSelectionModel::ClearAndSelect);
    CHECK(table->currentIndex() == classname);
    CHECK_FALSE(wadEditor->isHidden());
    CHECK_FALSE(wadEditor->parentWidget()->isHidden());

    QCoreApplication::processEvents();
    CHECK(splitter->sizes()[1] >= wadEditor->parentWidget()->minimumHeight());

    QStyleOptionViewItem option;
    option.initFrom(table);
    option.features |= QStyleOptionViewItem::WrapText;
    option.rect = QRect(0, 0, 55, 1000);
    CHECK(table->itemDelegate()->sizeHint(option, wadValue).height() < 2 * table->fontMetrics().height());
    REQUIRE(document->setProperty("wad", paths + ";maps/fourth.wad"));
    QCoreApplication::processEvents();
    CHECK_FALSE(wadEditor->parentWidget()->isHidden());
}
} // namespace TrenchBroom::View
