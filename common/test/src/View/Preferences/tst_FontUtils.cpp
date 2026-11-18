#include "TestSourcePath.h"
#include "IO/PathQt.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include "View/Theme/QSSBuilder.h"
#include "View/Widgets/Lists/ControlListBox.h"
#include "View/Widgets/ElidedLabel.h"
#include "View/Preferences/FontUtils.h"
#include "View/Preferences/ViewPreferencePane.h"
#include "View/Inspector/Layer/LayerTreeItemRenderer.h"
#include "View/Theme/QtUtils.h"
#include "View/Theme/TrenchBroomProxyStyle.h"
#include "View/Widgets/SliderWithLabel.h"

#include <QApplication>
#include <QFile>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStyleFactory>
#include <QStyledItemDelegate>
#include <QTreeView>
#include <QTextEdit>
#include <QListWidget>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QRawFont>
#include <QScopeGuard>
#include <QStandardItemModel>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QVBoxLayout>

#include <catch2/catch.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace TrenchBroom::View {
namespace {
class FontSettingsScope {
    const std::filesystem::path m_path = pref(Preferences::UIFontPath);
    const bool m_global = pref(Preferences::UIGlobalTypography);
    const int m_size = pref(Preferences::UIFontSize);
    const int m_minimum = pref(Preferences::UIFontMinimumSize);
    const float m_opacity = pref(Preferences::UIDetailTextOpacity);
    const int m_weight = pref(Preferences::UIListFontWeight);
    const float m_opticalSize = pref(Preferences::UIListOpticalSize);
    const bool m_typoMetrics = pref(Preferences::UIListTypoLineMetrics);
    const bool m_subpixel = pref(Preferences::UIListSubpixelPositioning);

public:
    FontSettingsScope() {
        auto& prefs = PreferenceManager::instance();
        prefs.set(Preferences::UIGlobalTypography, false);
        prefs.set(Preferences::UIFontSize, 13);
        prefs.set(Preferences::UIDetailTextOpacity, 0.8f);
        prefs.set(Preferences::UIFontMinimumSize, 11);
        prefs.set(Preferences::UIListFontWeight, 0);
        prefs.set(Preferences::UIListOpticalSize, 0.0f);
        prefs.set(Preferences::UIListTypoLineMetrics, false);
        prefs.set(Preferences::UIListSubpixelPositioning, false);
    }

    ~FontSettingsScope() {
        auto& prefs = PreferenceManager::instance();
        prefs.set(Preferences::UIFontPath, m_path);
        prefs.set(Preferences::UIGlobalTypography, m_global);
        prefs.set(Preferences::UIFontSize, m_size);
        prefs.set(Preferences::UIFontMinimumSize, m_minimum);
        prefs.set(Preferences::UIDetailTextOpacity, m_opacity);
        prefs.set(Preferences::UIListFontWeight, m_weight);
        prefs.set(Preferences::UIListOpticalSize, m_opticalSize);
        prefs.set(Preferences::UIListTypoLineMetrics, m_typoMetrics);
        prefs.set(Preferences::UIListSubpixelPositioning, m_subpixel);
    }
};

std::filesystem::path fontFile(const char* name) {
    return testSourceRoot()
        / "app/resources/fonts" / name;
}

void flushFontLayout() {
    QCoreApplication::sendPostedEvents();
    QCoreApplication::processEvents();
    QCoreApplication::sendPostedEvents();
    QCoreApplication::processEvents();
}

class FontList : public ControlListBox {
public:
    FontList() : ControlListBox("Empty", false) { reload(); }

private:
    size_t itemCount() const override { return 1; }

    ControlListBoxItemRenderer* createItemRenderer(QWidget* parent, size_t) override {
        auto* row = new ControlListBoxItemRenderer(parent);
        auto* layout = new QVBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(new QLabel("The row must fit its current font", row));
        return row;
    }
};
}

TEST_CASE_METHOD(FontSettingsScope, "UI font roles respect the saved minimum and pixel sizes", "[fonts]") {
    CHECK(uiFontPixelSize() == 13);
    CHECK(uiFontPixelSize(-2) == 11);
    CHECK(uiFontPixelSize(-20) == 11);
    PreferenceManager::instance().set(Preferences::UIFontMinimumSize, 16);
    CHECK(uiFontPixelSize() == 16);
    CHECK(uiFontPixelSize(5) == 18);

    QFont pixels;
    pixels.setPixelSize(22);
    pixels.setWeight(QFont::DemiBold);
    pixels.setItalic(true);
    pixels.setUnderline(true);
    const auto adjusted = adjustedFont(pixels, -3);
    CHECK(adjusted.pixelSize() == 19);
    CHECK(adjusted.weight() == pixels.weight());
    CHECK(adjusted.italic());
    CHECK(adjusted.underline());
    CHECK(adjusted.family() == pixels.family());
    CHECK(detailFont(pixels, 3).weight() == QFont::Normal);
    CHECK(detailFont(pixels, 100).pixelSize() == 16);
    CHECK(pixels.pixelSize() == 22);

    QFont points;
    points.setPointSizeF(22.5);
    points.setItalic(true);
    const auto normalized = adjustedFont(points, 0);
    REQUIRE(normalized.pixelSize() > 16);
    CHECK(detailFont(points, 2).pixelSize() == normalized.pixelSize() - 2);
    CHECK(normalized.italic());
    CHECK(points.pointSizeF() == Approx(22.5));
}

TEST_CASE_METHOD(FontSettingsScope, "A missing font preserves requested size and can be loaded when it appears", "[fonts]") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto file = IO::pathFromQString(directory.filePath("late-font.otf"));
    CHECK(loadFont(file, 19).pixelSize() == 19);
    CHECK(loadFont(file, 25).pixelSize() == 25);

    REQUIRE(QFile::copy(IO::pathAsQString(fontFile("Inter-SemiBold.otf")), IO::pathAsQString(file)));
    const QRawFont expected(IO::pathAsQString(file), 19);
    REQUIRE(expected.isValid());
    const auto loaded = loadFont(file, 19);
    const auto actual = QRawFont::fromFont(loaded);
    REQUIRE(actual.isValid());
    CHECK(actual.familyName() == expected.familyName());
    CHECK(actual.weight() == expected.weight());
}

TEST_CASE_METHOD(FontSettingsScope, "Existing small labels and headings follow live typography changes", "[fonts]") {
    const auto originalFont = QApplication::font();
    const auto originalStyle = qApp->styleSheet();
    const auto restore = qScopeGuard([&] {
        qApp->setStyleSheet(originalStyle);
        QApplication::setFont(originalFont);
    });
    QFile source(IO::pathAsQString(fontFile("../stylesheets/base.qss")));
    REQUIRE(source.open(QIODevice::ReadOnly));
    const auto stylesheet = QString::fromUtf8(source.readAll());
    const auto first = stylesheet.indexOf("/* ==== TYPOGRAPHY ==== */");
    const auto last = stylesheet.indexOf("/* ==== WINDOW AND STATUS BAR ==== */");
    REQUIRE(first >= 0); REQUIRE(last > first);
    const auto applyTypography = [&](const QFont& font) {
        auto text = stylesheet.mid(first, last-first);
        auto theme = ThemeManager::preferenceValues(font, font, font);
        QSSBuilder builder;
        builder.setText(text);
        REQUIRE(builder.update(theme.values));
        auto inheritedFont = font;
        inheritedFont.setStyleName({});
        QApplication::setFont(inheritedFont);
        qApp->setStyleSheet(builder.getRenderedText());
    };

    const auto inter = loadFont(fontFile("Inter-Regular.otf"), 13);
    applyTypography(inter);
    QLabel detail("Existing detail label");
    QLabel title("Existing heading");
    makeItalic(makeSmall(&detail));
    setTextStyleRole(&title,"dialogTitle");
    CHECK(detail.font().pixelSize() == 11);
    CHECK(title.font().pixelSize() == 20);

    PreferenceManager::instance().set(Preferences::UIFontMinimumSize, 16);
    const auto system = loadFont("[system]", 16);
    applyTypography(system);
    CHECK(detail.font().pixelSize() == 16);
    CHECK(detail.font().family() == system.family());
    CHECK(detail.font().italic());
    CHECK_FALSE(detail.font().bold());
    CHECK(title.font().pixelSize() == 24);
    CHECK(title.font().bold());

    PreferenceManager::instance().set(Preferences::UIFontMinimumSize, 11);
    applyTypography(inter);
    CHECK(detail.font().pixelSize() == 11);
    CHECK(detail.font().family() == inter.family());
    CHECK(detail.font().italic());
    CHECK(title.font().pixelSize() == 20);
    // Styling remains composable and can be overridden entirely in QSS.
    makeEmphasized(&detail);
    CHECK(detail.font().bold()); CHECK(detail.font().italic());
    detail.setStyleSheet("QLabel[uiTextRole=\"small\"] { font-size: 23px; color: rgb(25, 120, 200); }");
    detail.ensurePolished();
    CHECK(detail.font().pixelSize() == 23);
    CHECK(detail.palette().color(QPalette::WindowText) == QColor(25,120,200));
}

TEST_CASE_METHOD(FontSettingsScope, "File fonts retain their own face when the family is already registered", "[fonts]") {
    for (const auto* name : {"Inter-Regular.otf", "Inter-SemiBold.otf", "Inter-SemiBoldItalic.otf"}) {
        CAPTURE(name);
        const auto source = fontFile(name);
        const QRawFont expected(IO::pathAsQString(source), 18);
        REQUIRE(expected.isValid());
        auto loaded = loadFont(source, 18);
        const auto actual = QRawFont::fromFont(loaded);
        REQUIRE(actual.isValid());
        CHECK(loaded.pixelSize() == 18);
        CHECK(actual.familyName() == expected.familyName());
        CHECK(actual.weight() == expected.weight());
        CHECK(actual.style() == expected.style());

        loaded.setPixelSize(31);
        const auto smaller = loadFont(source.parent_path() / "." / source.filename(), 14);
        CHECK(smaller.pixelSize() == 14);
        CHECK(QRawFont::fromFont(smaller).weight() == expected.weight());
    }

    const auto family = loadFont(fontFile("Inter-Regular.otf"), 18).family();
    const QFontDatabase database;
    REQUIRE(database.hasFamily(family));
    CHECK(loadFont(IO::pathFromQString(family), 17).family() == family);
    CHECK(loadFont(IO::pathFromQString(family), 17).pixelSize() == 17);
}

TEST_CASE_METHOD(FontSettingsScope, "Font changes resize existing widget list rows", "[fonts]") {
    FontList list;
    QFont font;
    font.setPixelSize(12);
    list.setFont(font);
    list.resize(640, 160);
    list.show();
    flushFontLayout();
    auto* items = list.findChild<QListWidget*>();
    REQUIRE(items);
    REQUIRE(items->count() == 1);
    auto* originalItem = items->item(0);
    auto* originalRow = items->itemWidget(originalItem);
    const auto previousHeight = originalItem->sizeHint().height();

    font.setPixelSize(28);
    list.setFont(font);
    flushFontLayout();
    CHECK(items->item(0) == originalItem);
    CHECK(items->itemWidget(originalItem) == originalRow);
    CHECK(originalItem->sizeHint().height() > previousHeight);
    CHECK(originalItem->sizeHint().height() >= originalRow->minimumSizeHint().height());
}

TEST_CASE_METHOD(FontSettingsScope, "Elided labels recompute their text when only the font changes", "[fonts]") {
    const auto text = QStringLiteral("A long filename that should remain readable.map");
    ElidedLabel label(text, Qt::ElideRight);
    QFont font;
    font.setPixelSize(12);
    label.setFont(font);
    label.resize(180, 80);
    label.show();
    flushFontLayout();
    auto* displayed = label.findChild<QLabel*>();
    REQUIRE(displayed);
    const auto smallerText = displayed->text();
    font.setPixelSize(25);
    label.setFont(font);
    flushFontLayout();
    CHECK(label.width() == 180);
    CHECK(label.text() == text);
    CHECK(displayed->text() != smallerText);
    CHECK(displayed->text() == label.fontMetrics().elidedText(text, Qt::ElideRight, 180));
    CHECK(displayed->toolTip() == text);
}

TEST_CASE_METHOD(FontSettingsScope, "Tree details use the minimum font and saved opacity in every palette state", "[fonts]") {
    LayerTreeGroupRenderer renderer;
    QStandardItemModel model(1, 1);
    const auto index = model.index(0, 0);
    model.setData(index, "MMMM", LayerTreeItemDelegate::DetailRole);
    QStyleOptionViewItem option;
    option.text = "Title";
    option.font.setPointSizeF(12.5);
    PreferenceManager::instance().set(Preferences::UIFontMinimumSize, 16);
    const auto minimumTextHeight = QFontMetricsF(option.font).height() + QFontMetricsF(detailFont(option.font)).height();
    CHECK(renderer.sizeHint(option, index).height() >= int(std::ceil(minimumTextHeight)));

    option.font = loadFont(fontFile("Inter-Regular.otf"), 22);
    const std::array<QStyle::State, 3> states{
        QStyle::State_Enabled | QStyle::State_Active,
        QStyle::State_Enabled | QStyle::State_Selected | QStyle::State_Active,
        QStyle::State_None};
    for (const auto state : states) {
        CAPTURE(state);
        option.state = state;
        option.rect = QRect(0, 0, 300, 100);
        option.palette.setColor(QPalette::Active, QPalette::Text, Qt::red);
        option.palette.setColor(QPalette::Active, QPalette::HighlightedText, Qt::green);
        option.palette.setColor(QPalette::Disabled, QPalette::Text, Qt::blue);
        const auto detailAlpha = [&](float opacity) {
            PreferenceManager::instance().set(Preferences::UIDetailTextOpacity, opacity);
            QImage image(option.rect.size(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            {
                QPainter painter(&image);
                renderer.paint(painter, option, index);
            }
            int alpha = 0;
            for (int y = image.height() / 2 + 5; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x) alpha = std::max(alpha, image.pixelColor(x, y).alpha());
            }
            return alpha;
        };
        const auto opaqueAlpha = detailAlpha(1.0f);
        REQUIRE(opaqueAlpha > 0);
        CHECK(detailAlpha(0.4f) == Approx(0.4 * opaqueAlpha).margin(2));
        CHECK(detailAlpha(0.0f) == 0);
    }
}
TEST_CASE_METHOD(FontSettingsScope, "List font tuning changes outlines at intermediate weights and optical sizes", "[fonts][typography]") {
    const auto base = loadFont(fontFile("InterVariable.ttf"), 20);
    const auto axes = fontCapabilities(base);
    REQUIRE(axes.weight);
    REQUIRE(axes.opticalSize);
    CHECK(axes.weight->minimum == 100);
    CHECK(axes.weight->maximum == 900);
    CHECK(axes.opticalSize->minimum == 14);
    CHECK(axes.opticalSize->maximum == 32);
    const auto outline = [](const QFont& font) {
        const auto raw = QRawFont::fromFont(font);
        REQUIRE(raw.isValid());
        const auto glyphs = raw.glyphIndexesForString("e");
        REQUIRE(glyphs.size() == 1);
        return raw.pathForGlyph(glyphs.front());
    };
    const auto regular = listTextFont(base, {400, 14});
    const auto intermediate = listTextFont(base, {450, 14});
    const auto medium = listTextFont(base, {500, 14});
    CHECK(intermediate.variableAxisValue("wght") == 450);
    CHECK(outline(intermediate) != outline(regular));
    CHECK(outline(intermediate) != outline(medium));
    CHECK(outline(listTextFont(base, {450, 32})) != outline(intermediate));
    const auto clamped = listTextFont(base, {1000, 200, true});
    CHECK(clamped.variableAxisValue("wght") == 900);
    CHECK(clamped.variableAxisValue("opsz") == 32);
    CHECK(bool(clamped.styleStrategy() & QFont::PreferTypoLineMetrics));
    CHECK(listTextFont(base, {}).variableAxisValue("opsz") >= 14);
    auto heading = base;
    heading.setBold(true);
    CHECK(listTextFont(heading, {450, 14}, true).variableAxisValue("wght") == 700);
    CHECK(listTextFont(base, {450, 14}, true).variableAxisValue("wght") == 450);
    const auto staticFont = loadFont(fontFile("Inter-Regular.otf"), 13);
    REQUIRE_FALSE(fontCapabilities(staticFont).opticalSize);
    CHECK_FALSE(listTextFont(staticFont, {450, 25}).isVariableAxisSet("opsz"));
    // Variation settings never leak into the application font or raw font cache.
    CHECK_FALSE(base.isVariableAxisSet("wght"));
    CHECK_FALSE(loadFont(fontFile("InterVariable.ttf"), 20).isVariableAxisSet("opsz"));
}

TEST_CASE_METHOD(FontSettingsScope, "List text baselines align to physical pixels at fractional display scales", "[fonts][typography]") {
    const auto font = loadFont(fontFile("InterVariable.ttf"), 13);
    const QFontMetricsF metrics{font};
    const QRectF rect{1.2, 2.3, 140, 37.4};
    for (const auto density : {1.0, 1.25, 1.5, 2.0}) {
        CAPTURE(density);
        QImage image(400, 200, QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(density);
        QPainter painter(&image);
        painter.translate(0.17, 0.31);
        painter.scale(1.15, 1.07);
        const auto transform = painter.deviceTransform();
        const auto free = textLineBaseline(painter, rect, metrics, true);
        CHECK(free.x() == Approx(rect.left()));
        CHECK(free.y() == Approx(rect.top() + (rect.height() - metrics.height()) / 2 + metrics.ascent()));
        const auto aligned = transform.map(textLineBaseline(painter, rect, metrics, false));
        CHECK(aligned.x() == Approx(std::round(aligned.x())));
        CHECK(aligned.y() == Approx(std::round(aligned.y())));
        CHECK(std::abs(aligned.x() - transform.map(free).x()) <= 0.5);
        CHECK(std::abs(aligned.y() - transform.map(free).y()) <= 0.5);
        CHECK(painter.deviceTransform() == transform);
    }
}

TEST_CASE_METHOD(FontSettingsScope, "Font comparison stages the selected font and applies all settings together", "[fonts][typography]") {
    ViewPreferencePane pane{nullptr};
    const auto originalFont = QApplication::font();
    const auto originalPath = pref(Preferences::UIFontPath);
    auto* family = pane.findChild<QComboBox*>("UIFontFamily");
    auto* size = pane.findChild<QComboBox*>("UIFontSize");
    auto* minimum = pane.findChild<QSpinBox*>("UIFontMinimumSize");
    auto* opacity = pane.findChild<SliderWithLabel*>("UIDetailTextOpacity");
    auto* weight = pane.findChild<QSpinBox*>("UIListFontWeight");
    auto* optical = pane.findChild<QDoubleSpinBox*>("UIListOpticalSize");
    auto* metrics = pane.findChild<QCheckBox*>("UIListTypoLineMetrics");
    auto* position = pane.findChild<QComboBox*>("UIListTextPosition");
    auto* selected = pane.findChild<QWidget*>("UIFontPreview_Selected");
    auto* system = pane.findChild<QWidget*>("UIFontPreview_System");
    auto* status = pane.findChild<QLabel*>("UIFontPreview_SelectedInfo");
    auto* apply = pane.findChild<QPushButton*>("UIFontApply");
    REQUIRE(family); REQUIRE(size); REQUIRE(minimum); REQUIRE(opacity);
    REQUIRE(weight); REQUIRE(optical); REQUIRE(metrics); REQUIRE(position);
    REQUIRE(selected); REQUIRE(system); REQUIRE(status); REQUIRE(apply);
    CHECK(selected->property("previewTitleFont").value<QFont>().family() == loadFont(originalPath, 13).family());
    const auto variablePath = IO::pathAsQString(fontFile("InterVariable.ttf"));
    family->addItem("Variable test font", variablePath);
    family->setCurrentIndex(family->count() - 1);
    size->setCurrentText("16");
    minimum->setValue(15);
    opacity->setValue(65);
    weight->setValue(450);
    optical->setValue(24);
    metrics->setChecked(true);
    position->setCurrentIndex(1);
    CHECK(pref(Preferences::UIFontPath) == originalPath);
    CHECK(pref(Preferences::UIFontSize) == 13);
    CHECK(pref(Preferences::UIFontMinimumSize) == 11);
    CHECK(pref(Preferences::UIDetailTextOpacity) == Approx(0.8));
    CHECK(pref(Preferences::UIListFontWeight) == 0);
    CHECK(pref(Preferences::UIListOpticalSize) == 0.0f);
    CHECK_FALSE(pref(Preferences::UIListTypoLineMetrics));
    CHECK_FALSE(pref(Preferences::UIListSubpixelPositioning));
    CHECK_FALSE(pref(Preferences::UIGlobalTypography));
    CHECK(QApplication::font() == originalFont);
    const auto title = selected->property("previewTitleFont").value<QFont>();
    const auto detail = selected->property("previewDetailFont").value<QFont>();
    CHECK(title.family() == "Inter Variable");
    CHECK(title.pixelSize() == 16);
    CHECK(detail.pixelSize() == 15);
    CHECK(title.variableAxisValue("wght") == 450);
    CHECK(detail.variableAxisValue("wght") == 450);
    CHECK(title.variableAxisValue("opsz") == 24);
    CHECK(status->text().contains("450"));
    CHECK(status->text().contains(QLocale{}.toString(24.0, 'f', 1)));
    CHECK(logicalFontPixelSize(title) == logicalFontPixelSize(system->property("previewTitleFont").value<QFont>()));
    CHECK(logicalFontPixelSize(detail) == logicalFontPixelSize(system->property("previewDetailFont").value<QFont>()));
    pane.updateControls();
    CHECK(weight->value() == 450);
    CHECK(size->currentText() == "16");
    apply->click();
    CHECK(pref(Preferences::UIFontPath) == fontFile("InterVariable.ttf"));
    CHECK(pref(Preferences::UIFontSize) == 16);
    CHECK(pref(Preferences::UIFontMinimumSize) == 15);
    CHECK(pref(Preferences::UIDetailTextOpacity) == Approx(0.65));
    CHECK(pref(Preferences::UIListFontWeight) == 450);
    CHECK(pref(Preferences::UIListOpticalSize) == 24.0f);
    CHECK(pref(Preferences::UIListTypoLineMetrics));
    CHECK(pref(Preferences::UIListSubpixelPositioning));
    CHECK(pref(Preferences::UIGlobalTypography));
    CHECK_FALSE(apply->isEnabled());
    weight->setValue(475);
    family->setCurrentIndex(family->findData("[system]"));
    CHECK(selected->property("previewTitleFont").value<QFont>().family() == loadFont("[system]", 16).family());
    CHECK(pref(Preferences::UIListFontWeight) == 450);
    CHECK(pref(Preferences::UIFontPath) == fontFile("InterVariable.ttf"));
    ViewPreferencePane reopened{nullptr};
    CHECK(reopened.findChild<QSpinBox*>("UIListFontWeight")->value() == 450);
    CHECK(pref(Preferences::UIListFontWeight) == 450);
    CHECK(pref(Preferences::UIFontPath) == fontFile("InterVariable.ttf"));
    CHECK(apply->isEnabled());
}

TEST_CASE_METHOD(FontSettingsScope, "Global typography respects styled weights and live font inheritance", "[fonts][typography]") {
    const auto originalFont = QApplication::font();
    const auto originalStyleName = qApp->style()->objectName();
    const auto originalSheet = qApp->styleSheet();
    const auto restore = qScopeGuard([&] {
        PreferenceManager::instance().set(Preferences::UIGlobalTypography, false);
        qApp->setStyleSheet({});
        qApp->setStyle(QStyleFactory::create(originalStyleName));
        QApplication::setFont(originalFont);
        qApp->setStyleSheet(originalSheet);
    });
    auto& prefs = PreferenceManager::instance();
    prefs.set(Preferences::UIListFontWeight, 450);
    prefs.set(Preferences::UIListOpticalSize, 24.0f);
    prefs.set(Preferences::UIListTypoLineMetrics, true);
    prefs.set(Preferences::UIGlobalTypography, true);
    auto base = fontForStyleSheet(listTextFont(loadFont(fontFile("InterVariable.ttf"), 16), listFontSettings()));
    QApplication::setFont(base);
    auto* style = new TrenchBroomProxyStyle("Fusion");
    qApp->setStyleSheet({});
    qApp->setStyle(style);
    QWidget window;
    auto* layout = new QVBoxLayout(&window);
    auto* plain = new QLabel("Body text");
    auto* bold = new QLabel("Heading");
    bold->setStyleSheet("font-weight: 700;");
    auto* regular = new QLabel("Explicit normal");
    regular->setStyleSheet("font-weight: 400;");
    auto* edit = new QLineEdit("Editable text");
    auto* button = new QPushButton("Button");
    auto* rich = new QLabel("Body <b>bold</b>");
    auto* document = new QTextEdit("Text document");
    for (auto* widget : std::array<QWidget*, 7>{plain, bold, regular, edit, button, rich, document}) layout->addWidget(widget);
    window.show();
    flushFontLayout();
    style->refreshTypography();
    for (auto* widget : std::array<QWidget*, 3>{plain, edit, button}) {
        CAPTURE(widget->metaObject()->className());
        CHECK(widget->font().variableAxisValue("wght") == 450);
        CHECK(widget->font().variableAxisValue("opsz") == 24);
        CHECK(widget->font().pixelSize() == 16);
        CHECK(bool(widget->font().styleStrategy() & QFont::PreferTypoLineMetrics));
    }
    CHECK(bold->font().weight() == QFont::Bold);
    CHECK(bold->font().variableAxisValue("wght") == 700);
    CHECK(regular->font().variableAxisValue("wght") == 400);
    CHECK_FALSE(QApplication::font().isVariableAxisSet("wght"));
    CHECK_FALSE(rich->font().isVariableAxisSet("wght"));
    CHECK_FALSE(document->font().isVariableAxisSet("wght"));
    // Model-provided emphasis must not inherit the view's normal weight axis.
    QTreeView tree;
    tree.ensurePolished();
    flushFontLayout();
    QStandardItemModel model(2, 1);
    model.setData(model.index(0, 0), "Identical tree text");
    model.setData(model.index(1, 0), "Identical tree text");
    QFont itemBold;
    itemBold.setBold(true);
    model.setData(model.index(1, 0), itemBold, Qt::FontRole);
    QStyledItemDelegate delegate;
    const auto paintItem = [&](int row) {
        QImage image(300, 40, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::white);
        QStyleOptionViewItem option;
        option.initFrom(&tree);
        option.rect = QRect(0, 0, 300, 40);
        option.widget = &tree;
        QPainter painter(&image);
        delegate.paint(&painter, option, model.index(row, 0));
        return image;
    };
    CHECK(paintItem(0) != paintItem(1));
    base.setPixelSize(20);
    QApplication::setFont(base);
    flushFontLayout();
    CHECK(plain->font().pixelSize() == 20);
    CHECK(edit->font().pixelSize() == 20);
    CHECK(button->font().pixelSize() == 20);
    CHECK(bold->font().variableAxisValue("wght") == 700);
    auto* late = new QLabel("Created after Apply", &window);
    late->show();
    flushFontLayout();
    CHECK(late->font().variableAxisValue("wght") == 450);
    CHECK(late->font().pixelSize() == 20);
    // Exercise the real stylesheet defaults as well as QApplication inheritance.
    QFile source(IO::pathAsQString(fontFile("../stylesheets/base.qss")));
    REQUIRE(source.open(QIODevice::ReadOnly));
    const auto stylesheet = QString::fromUtf8(source.readAll());
    const auto first = stylesheet.indexOf("/* ==== TYPOGRAPHY ==== */");
    const auto last = stylesheet.indexOf("/* ==== WINDOW AND STATUS BAR ==== */");
    REQUIRE(first >= 0); REQUIRE(last > first);
    QSSBuilder builder;
    builder.setText(stylesheet.mid(first, last - first));
    REQUIRE(builder.update(ThemeManager::preferenceValues(base, base, base).values));
    qApp->setStyleSheet(builder.getRenderedText());
    flushFontLayout();
    style->refreshTypography();
    CHECK(plain->font().family() == "Inter Variable");
    CHECK(edit->font().family() == "Inter Variable");
    CHECK(plain->font().variableAxisValue("wght") == 450);
    CHECK(bold->font().variableAxisValue("wght") == 700);
    CHECK(regular->font().variableAxisValue("wght") == 400);
    CHECK_FALSE(rich->font().isVariableAxisSet("wght"));
    CHECK(paintItem(0) != paintItem(1));
    prefs.set(Preferences::UIGlobalTypography, false);
    style->refreshTypography();
    CHECK_FALSE(plain->font().isVariableAxisSet("wght"));
    CHECK_FALSE(edit->font().isVariableAxisSet("opsz"));
    CHECK(bold->font().weight() == QFont::Bold);
}

TEST_CASE_METHOD(FontSettingsScope, "Layer rows use exact list weights while retaining stylesheet emphasis", "[fonts][typography]") {
    const auto originalFont = QApplication::font();
    const auto restore = qScopeGuard([&] { QApplication::setFont(originalFont); });
    auto base = loadFont(fontFile("InterVariable.ttf"), 20);
    base.setWeight(QFont::Medium);
    base.setStyleName({});
    base.setHintingPreference(QFont::PreferNoHinting);
    QApplication::setFont(base);
    CHECK(listTextFont(base, {450, 14}, true).variableAxisValue("wght") == 450);
    auto heading = base;
    heading.setWeight(QFont::Bold);
    CHECK(listTextFont(heading, {450, 14}, true).variableAxisValue("wght") == 700);

    LayerTreeGroupRenderer renderer;
    QStandardItemModel model(1, 1);
    const auto index = model.index(0, 0);
    model.setData(index, "6 brushes · 32 faces · stone_wall", LayerTreeItemDelegate::DetailRole);
    QStyleOptionViewItem option;
    option.font = base;
    option.text = "Group Main Hall";
    option.state = QStyle::State_Enabled | QStyle::State_Active;
    option.palette.setColor(QPalette::Active, QPalette::Text, Qt::white);
    option.rect = QRect(0, 0, 420, 90);
    const auto render = [&](int weight) {
        PreferenceManager::instance().set(Preferences::UIListFontWeight, weight);
        QImage image(option.rect.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        renderer.paint(painter, option, index);
        painter.end();
        return image;
    };
    const auto regular = render(400);
    const auto intermediate = render(450);
    CHECK(intermediate != regular);
    CHECK(intermediate != render(700));
}

} // namespace TrenchBroom::View
