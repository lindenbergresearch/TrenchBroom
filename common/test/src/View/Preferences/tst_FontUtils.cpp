#include "TestSourcePath.h"
#include "IO/PathQt.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include "View/Theme/QSSBuilder.h"
#include "View/Widgets/Lists/ControlListBox.h"
#include "View/Widgets/ElidedLabel.h"
#include "View/Preferences/FontUtils.h"
#include "View/Inspector/Layer/LayerTreeItemRenderer.h"
#include "View/Theme/QtUtils.h"

#include <QApplication>
#include <QFile>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QRawFont>
#include <QScopeGuard>
#include <QStandardItemModel>
#include <QTemporaryDir>
#include <QVBoxLayout>

#include <catch2/catch.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace TrenchBroom::View {
namespace {
class FontSettingsScope {
    const int m_size = pref(Preferences::UIFontSize);
    const int m_minimum = pref(Preferences::UIFontMinimumSize);
    const float m_opacity = pref(Preferences::UIDetailTextOpacity);

public:
    FontSettingsScope() {
        auto& prefs = PreferenceManager::instance();
        prefs.set(Preferences::UIFontSize, 13);
        prefs.set(Preferences::UIFontMinimumSize, 11);
    }

    ~FontSettingsScope() {
        auto& prefs = PreferenceManager::instance();
        prefs.set(Preferences::UIFontSize, m_size);
        prefs.set(Preferences::UIFontMinimumSize, m_minimum);
        prefs.set(Preferences::UIDetailTextOpacity, m_opacity);
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
} // namespace TrenchBroom::View
