#include "FontStashPreferences.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include "Renderer/ActiveShader.h"
#include "Renderer/FontDescriptor.h"
#include "Renderer/FontManager.h"
#include "Renderer/GL.h"
#include "Renderer/Shaders.h"
#include "Renderer/TextureFont.h"
#include "View/MapView/GLContextManager.h"
#include "View/Preferences/ViewPreferencePane.h"
#include "View/Theme/WidgetTextPaint.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QPushButton>
#include <QScopeGuard>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>
#include <catch2/catch.hpp>

#include <algorithm>
#include <vector>

namespace TrenchBroom::View {

TEST_CASE("SDF controls update the sample and publish all values on Apply", "[sdf-text]") {
    const auto restoreSdf = TemporarilySetPref{fontStashPreference(), QString{}};
    const auto typography = TemporarilySetPref{Preferences::UIGlobalTypography, true};
    auto pane = ViewPreferencePane{};
    pane.updateControls();
    pane.resize(860, 1200);

    auto* weight = pane.findChild<QDoubleSpinBox*>("SdfTextWeight");
    auto* units = pane.findChild<QSpinBox*>("SdfUnitsPerPixel");
    auto* distance = pane.findChild<QDoubleSpinBox*>("SdfDistancePerTexel");
    auto* padding = pane.findChild<QSpinBox*>("SdfPadding");
    auto* sameFont = pane.findChild<QCheckBox*>("UIFontCompareSameFont");
    auto* reference = pane.findChild<QWidget*>("UIFontPreview_System");
    auto* preview = pane.findChild<QWidget*>("UIFontPreview_Selected");
    auto* apply = pane.findChild<QPushButton*>("UIFontApply");
    REQUIRE(weight);
    REQUIRE(units);
    REQUIRE(distance);
    REQUIRE(padding);
    REQUIRE(sameFont);
    REQUIRE(reference);
    REQUIRE(preview);
    REQUIRE(apply);
    CHECK(weight->value() == 0.80);
    CHECK(units->value() == 32);
    CHECK(distance->value() == 16.0);
    CHECK(padding->value() == 8);

    sameFont->setChecked(true);
    CHECK(reference->property("previewTitleFont") == preview->property("previewTitleFont"));
    const auto before = preview->grab().toImage();
    const auto qtBefore = reference->grab().toImage();
    weight->setValue(0.60);
    units->setValue(24);
    distance->setValue(20);
    padding->setValue(10);
    CHECK(fontStashPreferences() == FontStashPreferences{});
    CHECK(preview->grab().toImage() != before);
    CHECK(reference->grab().toImage() == qtBefore);
    CHECK(apply->isEnabled());
    apply->click();
    const auto applied = FontStashPreferences{true, {0.60, 24, 20, 10}};
    CHECK(fontStashPreferences() == applied);
    CHECK_FALSE(apply->isEnabled());
    pane.updateControls();
    CHECK(weight->value() == applied.sdf.textWeight);
    CHECK(units->value() == applied.sdf.unitsPerPixel);
    CHECK(distance->value() == applied.sdf.distancePerTexel);
    CHECK(padding->value() == applied.sdf.padding);
}

TEST_CASE("Qt widget SDF keeps native layout and editing", "[sdf-text]") {
    if (!widgetFontStashAvailable()) {
        SUCCEED("Qt private paint interface unavailable in this SDK");
        return;
    }
    const auto restore = TemporarilySetPref{fontStashPreference(), QString{}};
    qApp->setProperty("testFontStashWidgetPaint", true);
    const auto restorePaint = qScopeGuard([] { qApp->setProperty("testFontStashWidgetPaint", {}); });
    auto panel = QWidget{};
    panel.setStyleSheet("QWidget { font-size: 15px; color: #324f78; }");
    auto* layout = new QVBoxLayout(&panel);
    auto* label = new QLabel("Named brush · 32 faces", &panel);
    auto* button = new QPushButton("Current &layer", &panel);
    auto* input = new QLineEdit("target_name", &panel);
    input->setSelection(0, 6);
    auto* list = new QListWidget(&panel);
    list->addItems({"Nested brushes", "Static props"});
    auto* rich = new QTextEdit(&panel);
    rich->setHtml("<b>Bold</b> and <i>italic</i>");
    auto* combo = new QComboBox(&panel);
    combo->addItem("Active texture");
    const auto widgets = QList<QWidget*>{label, button, input, list, rich, combo};
    for (auto* widget : widgets) layout->addWidget(widget);
    panel.resize(420, 460);
    layout->activate();
    {
        const auto useQt = ScopedQtTextPaint{};
        panel.grab();
    }
    const auto html = rich->toHtml();
    for (auto* widget : widgets) {
        CAPTURE(widget->metaObject()->className());
        const auto size = widget->size();
        const auto qt = [&] {
            const auto useQt = ScopedQtTextPaint{};
            return widget->grab().toImage();
        }();
        const auto sdf = widget->grab().toImage();
        CHECK(sdf.size() == qt.size());
        CHECK(sdf != qt);
        CHECK(widget->size() == size);
    }
    CHECK(input->selectedText() == "target");
    CHECK(rich->toHtml() == html);

    const auto qtLabel = [&] {
        const auto useQt = ScopedQtTextPaint{};
        return label->grab().toImage();
    }();
    applyFontStashPreferences({false, {}});
    CHECK(label->grab().toImage() == qtLabel);
}

TEST_CASE("The viewport rebuilds SDF atlas pixels with stable geometry", "[.sdf-gl]") {
    const auto restore = TemporarilySetPref{fontStashPreference(), QString{}};
    auto format = QSurfaceFormat{};
    format.setVersion(2, 1);
    format.setRenderableType(QSurfaceFormat::OpenGL);
    auto context = QOpenGLContext{};
    context.setFormat(format);
    REQUIRE(context.create());
    auto surface = QOffscreenSurface{};
    surface.setFormat(context.format());
    surface.create();
    REQUIRE(context.makeCurrent(&surface));
    const auto done = qScopeGuard([&] { context.doneCurrent(); });
    auto renderer = GLContextManager{};
    REQUIRE(renderer.initialize());
    auto& fonts = renderer.fontManager();
    const auto descriptor = Renderer::FontDescriptor("fonts/Inter-Regular.otf", 13);
    const auto readAtlas = [&](Renderer::TextureFont& font) {
        font.activate();
        auto side = GLint{};
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &side);
        REQUIRE(side > 0);
        REQUIRE(side <= 2048);
        auto bytes = std::vector<GLubyte>(size_t(side) * size_t(side));
        glGetTexImage(GL_TEXTURE_2D, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, bytes.data());
        REQUIRE(glGetError() == GL_NO_ERROR);
        font.deactivate();
        return bytes;
    };
    auto& before = fonts.font(descriptor);
    const auto quads = before.quads("Axis marks");
    const auto size = before.measure("Axis marks");
    const auto original = readAtlas(before);
    REQUIRE(std::ranges::any_of(original, [](auto value) { return value > 127; }));
    auto framebuffer = QOpenGLFramebufferObject(160, 48);
    REQUIRE(framebuffer.isValid());
    const auto render = [&](Renderer::TextureFont& font) {
        REQUIRE(framebuffer.bind());
        glViewport(0, 0, framebuffer.width(), framebuffer.height());
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, framebuffer.width(), 0, framebuffer.height(), -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        {
            const auto vertices = font.quads("Axis marks", true, vm::vec2f{10, 10});
            auto shader = Renderer::ActiveShader(renderer.shaderManager(), Renderer::Shaders::ColoredTextShader);
            font.setShaderUniforms(shader);
            font.activate();
            glColor4f(1, 1, 1, 1);
            glBegin(GL_QUADS);
            for (auto i = size_t{0}; i < vertices.size(); i += 2) {
                glTexCoord2f(vertices[i + 1].x(), vertices[i + 1].y());
                glVertex2f(vertices[i].x(), vertices[i].y());
            }
            glEnd();
            font.deactivate();
        }
        REQUIRE(glGetError() == GL_NO_ERROR);
        return framebuffer.toImage();
    };
    const auto originalLabel = render(before);
    auto visibleGlyph = false;
    for (auto y = 0; y < originalLabel.height(); ++y) {
        for (auto x = 0; x < originalLabel.width(); ++x)
            visibleGlyph |= originalLabel.pixelColor(x, y).alpha() > 100;
    }
    CHECK(visibleGlyph);
    applyFontStashPreferences({true, {0.70, 24, 20.0, 10}});
    auto& after = fonts.font(descriptor);
    CHECK(after.quads("Axis marks") == quads);
    CHECK(after.measure("Axis marks") == size);
    CHECK(readAtlas(after) != original);
    CHECK(render(after) != originalLabel);
}

} // namespace TrenchBroom::View
