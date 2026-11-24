#include "Error.h"
#include "FontStashPreferences.h"
#include "IO/IconCatalog.h"
#include "IO/PathQt.h"
#include "IO/ResourceUtils.h"
#include "IO/SystemPaths.h"
#include "Logger.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include "TestSourcePath.h"
#include "View/Actions/Actions.h"
#include "View/Actions/MainMenuBuilder.h"
#include "View/Preferences/EditorCustomizationPreferencePane.h"
#include "View/Preferences/IconSelectionDialog.h"
#include "View/Widgets/ColorButton.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScopeGuard>
#include <QSet>
#include <QSpinBox>
#include <QStyleOptionViewItem>
#include <QTemporaryDir>
#include <QTimer>

#include <algorithm>

#include <catch2/catch.hpp>

namespace TrenchBroom::View {

TEST_CASE("Icon picker preserves resource paths and its current selection", "[icon-picker]") {
    const auto& catalog = IO::IconCatalog::instance();
    auto names = QSet<QString>{};
    for (const auto& entry : catalog.entries()) names.insert(entry.name);
    REQUIRE(names.contains("document"));
    CHECK(names.size() == catalog.entries().size());
    const auto path = IO::SystemPaths::findResourceFile("images/document.svg");
    REQUIRE_FALSE(path.empty());
    const auto canonical = IO::loadIcon("document", 16);
    REQUIRE_FALSE(canonical.isNull());
    CHECK(canonical.pixmap(16).toImage() == IO::loadIcon("document.svg", 16).pixmap(16).toImage());
    CHECK(canonical.pixmap(16).toImage() == IO::loadIcon(path, 16).pixmap(16).toImage());

    IconSelectionDialog relative{catalog, "document.svg"};
    CHECK(relative.selectedIcon() == "document");
    IconSelectionDialog absolute{catalog, IO::pathAsQString(path)};
    CHECK(absolute.selectedIcon() == "document");
    absolute.reject();
    CHECK(absolute.result() == QDialog::Rejected);
}

TEST_CASE("Icon picker searches filenames and only accepts visible matches", "[icon-picker]") {
    IconSelectionDialog dialog{IO::IconCatalog::instance(), "document.svg"};
    auto* filter = dialog.findChild<QLineEdit*>("EditorIconFilter");
    auto* grid = dialog.findChild<QListView*>("EditorIconGrid");
    auto* buttons = dialog.findChild<QDialogButtonBox*>("EditorIconButtons");
    REQUIRE(filter);
    REQUIRE(grid);
    REQUIRE(buttons);
    REQUIRE(grid->model()->rowCount() > 0);
    filter->setText("__no_such_icon__");
    CHECK(dialog.selectedIcon().isEmpty());
    CHECK_FALSE(buttons->button(QDialogButtonBox::Ok)->isEnabled());
    dialog.accept();
    CHECK(dialog.result() == QDialog::Rejected);
    filter->setText("DOCUMENT.SVG");
    REQUIRE_FALSE(dialog.selectedIcon().isEmpty());
    CHECK(dialog.selectedIcon() == "document");
    CHECK(buttons->button(QDialogButtonBox::Ok)->isEnabled());
    CHECK(grid->viewMode() == QListView::IconMode);
    buttons->button(QDialogButtonBox::Ok)->click();
    CHECK(dialog.result() == QDialog::Accepted);
}

TEST_CASE("Icon picker zooms its tiles and highlights matches in the caption", "[icon-picker]") {
    IconSelectionDialog dialog{IO::IconCatalog::instance(), "document.svg"};
    auto* grid = dialog.findChild<QListView*>("EditorIconGrid");
    auto* size = dialog.findChild<QSpinBox*>("EditorIconSize");
    auto* filter = dialog.findChild<QLineEdit*>("EditorIconFilter");
    auto* name = dialog.findChild<QLabel*>("EditorIconName");
    REQUIRE(grid);
    REQUIRE(size);
    REQUIRE(filter);
    REQUIRE(name);
    CHECK(name->text().contains("24 × 24 px"));

    // Both queries match the same SVG. Only the second matches its caption.
    const auto mark = QColor("#fa1f78");
    const auto drawTile = [&] {
        auto option = QStyleOptionViewItem{};
        option.initFrom(grid);
        option.state |= QStyle::State_Selected;
        option.palette.setColor(QPalette::Base, mark);
        option.rect = QRect(QPoint{}, grid->gridSize());
        auto image = QImage(option.rect.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        auto painter = QPainter(&image);
        grid->itemDelegate()->paint(&painter, option, grid->currentIndex());
        return image;
    };
    const auto markedPixels = [&](const QImage& image) {
        auto pixels = 0;
        for (auto y = image.height() - 2 * grid->fontMetrics().height() - 3; y < image.height(); ++y)
            for (auto x = 0; x < image.width(); ++x)
                pixels += image.pixelColor(x, y) == mark;
        return pixels;
    };
    filter->setText("document.svg");
    CHECK(markedPixels(drawTile()) == 0);
    filter->setText("CuMe");
    REQUIRE(dialog.selectedIcon() == "document");
    CHECK(markedPixels(drawTile()) > 0);

    dialog.show();
    QApplication::processEvents();
    const auto originalRect = grid->visualRect(grid->currentIndex());
    REQUIRE_FALSE(originalRect.isEmpty());
    const auto original = grid->gridSize();
    size->setValue(grid->iconSize().width() + 24);
    QApplication::processEvents();
    CHECK(grid->iconSize() == QSize(size->value(), size->value()));
    CHECK(grid->gridSize().height() >= original.height() + 24);
    CHECK(grid->visualRect(grid->currentIndex()).height() >= originalRect.height() + 24);
    CHECK(name->text().contains("24 × 24 px"));
    CHECK(dialog.iconStyle() == IO::IconStyle{});

    grid->setGridSize(QSize(58, 86));
    filter->setText("ShapeTool_Cylinder");
    REQUIRE(dialog.selectedIcon() == "ShapeTool_Cylinder");
    CHECK(markedPixels(drawTile()) > 0);
}

TEST_CASE("The icon catalog indexes configured glyph names and renders them at the requested density", "[icon-picker]") {
    const auto restoreRenderer = TemporarilySetPref{fontStashPreference(), QString{}};
    const auto toolbarSize = TemporarilySetPref{Preferences::ToolBarIconsSize, 29};
    const auto directory = QTemporaryDir{};
    REQUIRE(directory.isValid());
    REQUIRE(QDir(directory.path()).mkpath("images/nested"));
    REQUIRE(QDir(directory.path()).mkpath("fonts/icons/vendor"));
    const auto write = [&](const QString& path, const QByteArray& data) {
        auto file = QFile(directory.filePath(path));
        REQUIRE(file.open(QIODevice::WriteOnly));
        REQUIRE(file.write(data) == data.size());
    };
    const auto svg = QByteArray{R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><rect width="16" height="16"/></svg>)"};
    for (const auto* path : {"images/A.svg", "images/A_1.svg", "images/nested/A.svg"}) write(path, svg);
    const auto source = IO::pathAsQString(testSourceRoot() / "app/resources/fonts/JetBrainsMono/JetBrainsMono-Regular.ttf");
    for (const auto* name : {"One.ttf", "Two.ttf", "Unused.ttf"})
        REQUIRE(QFile::copy(source, directory.filePath(QString("fonts/icons/vendor/%1").arg(name))));
    write("fonts/icons/icon-fonts.json", R"({"fonts":["vendor/One.ttf","vendor/Two.ttf"]})");

    class TestLogger : public Logger {
        void doLog(LogLevel level, const LogMessage* message) override {
            if (level == LogLevel::Info) infos.append(message->message);
            if (level == LogLevel::Warn) warnings.append(message->message);
        }

    public:
        QStringList infos;
        QStringList warnings;
    };
    auto logger = TestLogger{};
    logger.setLogLevel(LogLevel::Info);
    const auto catalog = IO::IconCatalog({IO::pathFromQString(directory.filePath("images"))},
        {IO::pathFromQString(directory.filePath("fonts/icons"))}, logger);
    CHECK(catalog.canonicalName("A.svg") == "A");
    CHECK(catalog.canonicalName("A_1.svg") == "A_1");
    CHECK(catalog.canonicalName("nested/A.svg") == "A_2");
    CHECK(logger.infos.size() == 4);
    CHECK(std::ranges::any_of(logger.infos, [](const auto& s) { return s.contains("Indexed 1 SVG icons") && s.endsWith("/nested"); }));
    CHECK(std::ranges::any_of(logger.infos, [](const auto& s) { return s.contains("icons in font") && s.contains("One.ttf"); }));
    CHECK(std::ranges::any_of(logger.infos, [](const auto& s) { return s.contains("icons in font") && s.contains("Two.ttf"); }));
    CHECK(std::ranges::any_of(logger.warnings, [](const auto& s) { return s.endsWith("indexed as A_3"); }));
    CHECK(std::ranges::none_of(catalog.entries(), [](const auto& entry) { return entry.source.contains("Unused.ttf"); }));

    const auto icon = catalog.icon("A_3");
    REQUIRE_FALSE(icon.isNull());
    REQUIRE_FALSE(catalog.icon("A_4").isNull());
    for (const auto density : {1.0, 2.0}) {
        const auto pixmap = icon.pixmap(QSize{23, 23}, density);
        REQUIRE_FALSE(pixmap.isNull());
        CHECK(pixmap.size() == QSize{23, 23} * density);
        CHECK(pixmap.devicePixelRatio() == density);
        const auto image = pixmap.toImage();
        auto visiblePixels = 0;
        for (auto y = 0; y < image.height(); ++y)
            for (auto x = 0; x < image.width(); ++x)
                visiblePixels += image.pixelColor(x, y).alpha() > 127;
        CHECK(visiblePixels > 0);
        CHECK(visiblePixels < image.width() * image.height());
    }
    const auto sdf = icon.pixmap(QSize{23, 23}, 2.0).toImage();
    applyFontStashPreferences({false, {}});
    CHECK(icon.pixmap(QSize{23, 23}, 2.0).toImage() != sdf);
    applyFontStashPreferences({true, {}});
    CHECK(icon.pixmap(QSize{23, 23}, 2.0).toImage() == sdf);

    IconSelectionDialog dialog{catalog, "A_3"};
    auto* filter = dialog.findChild<QLineEdit*>("EditorIconFilter");
    auto* grid = dialog.findChild<QListView*>("EditorIconGrid");
    auto* name = dialog.findChild<QLabel*>("EditorIconName");
    REQUIRE(filter);
    REQUIRE(grid);
    REQUIRE(name);
    CHECK(dialog.selectedIcon() == "A_3");
    const auto raw = QRawFont(source, 64.0, QFont::PreferNoHinting);
    REQUIRE(raw.isValid());
    const auto glyph = raw.glyphIndexesForString("A");
    REQUIRE(glyph.size() == 1);
    auto outlineSize = raw.pathForGlyph(glyph.front()).boundingRect().size();
    outlineSize.scale(QSizeF(29, 29), Qt::KeepAspectRatio);
    CHECK(catalog.sourceSize("A_3", 29) == outlineSize.toSize());
    CHECK(name->text().contains(QString("%1 × %2 px").arg(qRound(outlineSize.width())).arg(qRound(outlineSize.height()))));
    const auto styled = catalog.icon("A_3", {.color = Qt::red, .brightness = 0.6, .opacity = 0.4});
    const auto rendered = styled.pixmap(QSize{23, 23}, 2.0).toImage();
    auto strongest = QColor(Qt::transparent);
    for (auto y = 0; y < rendered.height(); ++y)
        for (auto x = 0; x < rendered.width(); ++x)
            if (const auto pixel = rendered.pixelColor(x, y); pixel.alpha() > strongest.alpha()) strongest = pixel;
    CHECK(strongest.red() == Approx(153).margin(3));
    CHECK(strongest.green() == 0);
    CHECK(strongest.alpha() == Approx(102).margin(1));
    filter->setText("ONE.TTF");
    CHECK(dialog.selectedIcon() == "A_3");
    const auto* model = grid->model();
    REQUIRE(model->rowCount() > 0);
    for (auto row = 0; row < model->rowCount(); ++row)
        CHECK(model->index(row, 0).data(Qt::ToolTipRole).toString().contains("One.ttf"));
}

TEST_CASE("Action preferences save the chosen icon style and preserve it when the picker is cancelled", "[icon-picker][editor-preferences]") {
    auto& manager = ActionManager::instance();
    const auto oldConfiguration = manager.configuration();
    const auto oldPreference = pref(Preferences::EditorConfiguration);
    const auto restore = qScopeGuard([&] {
        QStringList errors;
        manager.setConfiguration(oldConfiguration, errors);
        PreferenceManager::instance().set(Preferences::EditorConfiguration, oldPreference);
        manager.reloadConfiguration();
    });
    const QString actionId = "Menu/View/Grid/Show Grid";
    auto configuration = oldConfiguration;
    REQUIRE(configuration.actions.contains(actionId));
    configuration.actions[actionId].icon = "document.svg";
    QStringList errors;
    REQUIRE(manager.setConfiguration(configuration, errors));

    EditorCustomizationPreferencePane pane{false};
    auto* entries = pane.findChild<QListWidget*>("EditorConfigurationList");
    auto* browse = pane.findChild<QPushButton*>("EditorActionBrowseIcon");
    auto* icon = pane.findChild<QLineEdit*>("EditorActionIcon");
    REQUIRE(entries);
    REQUIRE(browse);
    REQUIRE(icon);
    for (auto row = 0; row < entries->count(); ++row)
        if (entries->item(row)->data(Qt::UserRole).toString() == actionId) entries->setCurrentRow(row);
    REQUIRE(icon->text() == "document.svg");

    const QString chosenIcon = "ShapeTool_Cylinder";
    const auto chosenStyle = IO::IconStyle{.color = QColor("#df2e76"), .brightness = 0.63, .opacity = 0.4};
    auto opened = false, selected = false;
    QTimer::singleShot(0, &pane, [&] {
        auto* picker = pane.findChild<IconSelectionDialog*>("EditorIconSelection");
        opened = picker != nullptr;
        if (!picker) {
            if (auto* modal = qobject_cast<QDialog*>(QApplication::activeModalWidget())) modal->reject();
            return;
        }
        if (auto* filter = picker->findChild<QLineEdit*>("EditorIconFilter")) filter->setText(chosenIcon);
        auto* color = picker->findChild<ColorButton*>("EditorIconColor");
        auto* brightness = picker->findChild<QSpinBox*>("EditorIconBrightness");
        auto* opacity = picker->findChild<QSpinBox*>("EditorIconOpacity");
        auto* size = picker->findChild<QSpinBox*>("EditorIconSize");
        if (color) emit color->colorChangedByUser(chosenStyle.color);
        if (brightness) brightness->setValue(63);
        if (opacity) opacity->setValue(40);
        if (size) size->setValue(86);
        selected = size && picker->selectedIcon() == chosenIcon && picker->iconStyle() == chosenStyle;
        if (selected)
            picker->accept();
        else
            picker->reject();
    });
    browse->click();
    REQUIRE(opened);
    REQUIRE(selected);
    CHECK(manager.configuration().actions[actionId].icon == chosenIcon);
    CHECK(manager.configuration().actions[actionId].iconStyle == chosenStyle);
    CHECK(icon->text() == chosenIcon);
    REQUIRE(manager.reloadConfiguration());
    CHECK(manager.configuration().actions[actionId].iconStyle == chosenStyle);
    const auto* action = manager.action(actionId);
    REQUIRE(action);
    CHECK(action->icon(24).cacheKey() == IO::loadIcon(IO::pathFromQString(chosenIcon), 24, chosenStyle).cacheKey());
    auto overriddenStyle = chosenStyle;
    overriddenStyle.color = Qt::green;
    overriddenStyle.opacity = 1.0;
    CHECK(action->icon(24, {.color = Qt::green, .opacity = 1.0}).cacheKey()
        == IO::loadIcon(IO::pathFromQString(chosenIcon), 24, overriddenStyle).cacheKey());
    QAction shared;
    MenuBuilderBase::updateActionKeySeqeunce(&shared, action);
    CHECK(shared.icon().cacheKey() == action->icon(24).cacheKey());
    auto context = QMenu{};
    CHECK(addContextMenuAction(context, *action, [] {}, true)->icon().cacheKey() == action->icon(16).cacheKey());

    opened = selected = false;
    QTimer::singleShot(0, &pane, [&] {
        auto* picker = pane.findChild<IconSelectionDialog*>("EditorIconSelection");
        opened = picker != nullptr;
        if (picker) {
            const auto* size = picker->findChild<QSpinBox*>("EditorIconSize");
            selected = size && size->value() != 86 && picker->iconStyle() == chosenStyle;
            if (auto* filter = picker->findChild<QLineEdit*>("EditorIconFilter")) filter->setText("document.svg");
            if (auto* reset = picker->findChild<QPushButton*>("EditorIconDefaultColor")) reset->click();
            if (auto* opacity = picker->findChild<QSpinBox*>("EditorIconOpacity")) opacity->setValue(100);
            picker->reject();
        } else if (auto* modal = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
            modal->reject();
        }
    });
    browse->click();
    REQUIRE(opened);
    REQUIRE(selected);
    CHECK(manager.configuration().actions[actionId].icon == chosenIcon);
    CHECK(manager.configuration().actions[actionId].iconStyle == chosenStyle);
    CHECK(icon->text() == chosenIcon);
}

} // namespace TrenchBroom::View
