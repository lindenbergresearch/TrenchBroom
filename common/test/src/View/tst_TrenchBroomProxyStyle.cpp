#include "View/TrenchBroomProxyStyle.h"

#include <QApplication>
#include <QFile>
#include <QImage>
#include <QMenu>
#include <QMenuBar>
#include <QPointer>
#include <QStyleOption>
#include <QResizeEvent>
#include <filesystem>
#include <vector>
#include <QToolButton>
#include <QToolTip>
#include <catch2/catch.hpp>

namespace TrenchBroom::View {
namespace {
QString popupStyleSheet() {
    const auto path = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path().parent_path()
        / "app/resources/stylesheets/base.qss";
    QFile file(QString::fromStdString(path.string()));
    REQUIRE(file.open(QIODevice::ReadOnly));
    const auto source = QString::fromUtf8(file.readAll());
    const auto start = source.indexOf("QToolTip {");
    const auto end = source.indexOf("/* --- INSPECTOR", start);
    REQUIRE(start >= 0);
    REQUIRE(end > start);
    auto result = source.mid(start, end - start);
    for (const auto& replacement : std::vector<std::pair<QString, QString>>{
        {"MENU_CORNER_RADIUS", QString::number(TrenchBroomProxyStyle::MenuCornerRadius) + "px"},
        {"TOOLTIP_CORNER_RADIUS", QString::number(TrenchBroomProxyStyle::ToolTipCornerRadius) + "px"},
        {"BASE_COLOR", "#212427"}, {"DARK_COLOR", "#303236"}, {"MIDLIGHT_COLOR", "#606870"},
        {"BUTTON_TEXT_COLOR", "#f0f0f0"}, {"HIGHLIGHTED_TEXT_COLOR", "#ffffff"},
        {"HIGHLIGHT_COLOR", "#5c82d0"}, {"DISABLED_TEXT_COLOR", "#909090"},
        {"UI_SMALL_FONT_SIZE", "11px"}, {"UI_FONT_SIZE", "13px"},
        {"NARROW_MARGIN", "4px"}, {"MEDIUM_MARGIN", "6px"}, {"WIDE_MARGIN", "8px"}})
        result.replace("#" + replacement.first, replacement.second);
    return result;
}

struct PopupStyleFixture {
    PopupStyleFixture() { qApp->setStyleSheet(popupStyleSheet()); }
    ~PopupStyleFixture() { QToolTip::hideText(); qApp->setStyleSheet({}); }
};

void checkPopup(QWidget& widget) {
    widget.ensurePolished();
    CHECK(widget.testAttribute(Qt::WA_TranslucentBackground));
    CHECK_FALSE(widget.testAttribute(Qt::WA_OpaquePaintEvent));
    CHECK(widget.windowFlags().testFlag(Qt::FramelessWindowHint));
    CHECK(widget.mask().isEmpty());
}

void checkRoundedPixels(QWidget& widget, const QString& artifact) {
    widget.resize(widget.sizeHint().expandedTo(QSize(100, 30)));
    const auto image = widget.grab().toImage().convertToFormat(QImage::Format_ARGB32);
    REQUIRE_FALSE(image.isNull());
    CHECK(image.pixelColor(0, 0).alpha() == 0);
    CHECK(image.pixelColor(image.width()-1, 0).alpha() == 0);
    CHECK(image.pixelColor(0, image.height()-1).alpha() == 0);
    CHECK(image.pixelColor(image.width()-1, image.height()-1).alpha() == 0);
    CHECK(image.pixelColor(image.width()/2, image.height()/2).alpha() == 255);
    bool antialiased = false;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x) {
            const int alpha = image.pixelColor(x, y).alpha();
            antialiased |= alpha > 0 && alpha < 255;
        }
    CHECK(antialiased);
    if (!artifact.isEmpty()) image.save(artifact);
}

QWidget* toolTipWidget() {
    for (auto* widget : QApplication::topLevelWidgets())
        if (widget->windowType() == Qt::ToolTip && widget->isVisible()) return widget;
    return nullptr;
}
}

TEST_CASE_METHOD(PopupStyleFixture, "Popup style covers menus from every owner", "[popup-style]") {
    QMenu context;
    context.addAction("Context menu");
    checkPopup(context);
    CHECK(context.windowType() == Qt::Popup);
    QMenuBar menuBar;
    auto* main = menuBar.addMenu("Main");
    auto* nested = main->addMenu("Nested");
    nested->addAction("Nested action");
    checkPopup(*main);
    checkPopup(*nested);
    QToolButton button;
    auto* owned = new QMenu(&button);
    owned->addAction("Button menu");
    button.setMenu(owned);
    checkPopup(*owned);
    context.popup(QPoint(120, 120));
    QCoreApplication::processEvents();
    REQUIRE(context.isVisible());
    checkRoundedPixels(context, "/tmp/tb-popup-context.png");
    context.hide();
    checkRoundedPixels(*nested, "/tmp/tb-popup-submenu.png");
}

TEST_CASE_METHOD(PopupStyleFixture, "Popup style clears obsolete masks on menu reuse and style reload", "[popup-style]") {
    QMenu menu;
    menu.addAction("Resize and reuse");
    checkPopup(menu);
    menu.setMask(QRegion(QRect(0, 0, 8, 8)));
    QResizeEvent resize(QSize(200, 60), QSize(100, 30));
    QCoreApplication::sendEvent(&menu, &resize);
    CHECK(menu.mask().isEmpty());
    qApp->setStyleSheet(popupStyleSheet());
    checkPopup(menu);
    checkRoundedPixels(menu, {});
    CHECK(qApp->style()->styleHint(QStyle::SH_MenuBar_AltKeyNavigation) == 0);
}

TEST_CASE_METHOD(PopupStyleFixture, "Popup style gives real tooltips an opaque body and smooth transparent corners", "[popup-style]") {
    QWidget host;
    QToolTip::showText(QPoint(100, 100), "Readable tooltip", &host);
    QCoreApplication::processEvents();
    QPointer<QWidget> tip = toolTipWidget();
    REQUIRE(tip);
    checkPopup(*tip);
    CHECK(tip->windowType() == Qt::ToolTip);
    checkRoundedPixels(*tip, "/tmp/tb-popup-tooltip.png");
    QToolTip::showText(QPoint(100, 100), "Longer tooltip with a second line\nand additional documentation", &host);
    QCoreApplication::processEvents();
    REQUIRE(toolTipWidget());
    CHECK(toolTipWidget() == tip);
    checkPopup(*toolTipWidget());
    checkRoundedPixels(*toolTipWidget(), "/tmp/tb-popup-tooltip-multiline.png");
}

TEST_CASE("Popup style also rounds the unstyled fallback", "[popup-style]") {
    qApp->setStyleSheet({});
    QMenu menu;
    menu.addAction("Fallback menu");
    checkPopup(menu);
    checkRoundedPixels(menu, "/tmp/tb-popup-fallback.png");
}
} // namespace TrenchBroom::View
