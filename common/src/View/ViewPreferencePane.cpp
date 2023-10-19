/*
 Copyright (C) 2010-2017 Kristian Duske

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ViewPreferencePane.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QtGlobal>
#include <QFontDatabase>

#include "PreferenceManager.h"
#include "Preferences.h"
#include "Renderer/GL.h"
#include "View/ColorButton.h"
#include "View/FormWithSectionsLayout.h"
#include "View/QtUtils.h"
#include "View/SliderWithLabel.h"
#include "View/ViewConstants.h"
#include "IO/SystemPaths.h"
#include "IO/DiskFileSystem.h"
#include "IO/TraversalMode.h"
#include "IO/PathMatcher.h"
#include "Error.h"
#include "IO/File.h"
#include "IO/PathInfo.h"

#include <kdl/overload.h>
#include <vecmath/scalar.h>
#include <TrenchBroomApp.h>

#include <array>
#include <string>
#include <filesystem>

namespace TrenchBroom::View {
namespace {
struct TextureMode {
  int minFilter;
  int magFilter;
  std::string name;

  TextureMode(const int i_minFilter, const int i_magFilter, std::string i_name) : minFilter{i_minFilter}, magFilter{
      i_magFilter
  }, name{std::move(i_name)} {
  }
};

const auto TextureModes = std::array<TextureMode, 6>{
    TextureMode{GL_NEAREST, GL_NEAREST, "Nearest"}, TextureMode{
        GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, "Nearest (mipmapped)"
    }, TextureMode{
        GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST, "Nearest (mipmapped, interpolated)"
    }, TextureMode{GL_LINEAR, GL_LINEAR, "Linear"}, TextureMode{
        GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR, "Linear (mipmapped)"
    }, TextureMode{
        GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, "Linear (mipmapped, interpolated)"
    },
};

constexpr int brightnessToUI(const float value) {
    return int(vm::round(100.0f * (value - 1.0f)));
}

constexpr float brightnessFromUI(const int value) {
    return (float(value) / 100.0f) + 1.0f;
}

static_assert(0 == brightnessToUI(brightnessFromUI(0)));
} // namespace

ViewPreferencePane::ViewPreferencePane(QWidget *parent) : PreferencePane{parent} {
    createGui();
    bindEvents();
}

void ViewPreferencePane::createGui() {
    auto *viewPreferences = createViewPreferences();

    auto *layout = new QVBoxLayout{};
    layout->setContentsMargins(QMargins{});
    layout->setSpacing(0);

    layout->addWidget(viewPreferences, 1);
    layout->addSpacing(LayoutConstants::MediumVMargin);
    setLayout(layout);
}

QWidget *ViewPreferencePane::createViewPreferences() {
    auto *viewBox = new QWidget{this};

    auto *viewPrefsHeader = new QLabel{"Map Views"};
    makeEmphasized(viewPrefsHeader);

    m_themeCombo = new QComboBox{};
    m_themeCombo->addItems({Preferences::systemTheme(), Preferences::darkTheme()});
    auto *themeInfo = new QLabel{};
    themeInfo->setText(tr("Requires restart after changing"));
    makeInfo(themeInfo);
    auto *themeLayout = new QHBoxLayout{};
    themeLayout->addWidget(m_themeCombo);
    themeLayout->addSpacing(LayoutConstants::NarrowHMargin);
    themeLayout->addWidget(themeInfo);
    themeLayout->setContentsMargins(0, 0, 0, 0);

    m_layoutCombo = new QComboBox{};
    m_layoutCombo->setToolTip("Sets the layout of the editing views.");
    m_layoutCombo->addItem("One Pane");
    m_layoutCombo->addItem("Two Panes");
    m_layoutCombo->addItem("Three Panes");
    m_layoutCombo->addItem("Four Panes");

    m_link2dCameras = new QCheckBox{"Sync 2D views"};
    m_link2dCameras->setToolTip("All 2D views pan and zoom together.");

    auto *viewLayoutLayout = new QHBoxLayout{};
    viewLayoutLayout->addWidget(m_layoutCombo);
    viewLayoutLayout->addSpacing(LayoutConstants::NarrowHMargin);
    viewLayoutLayout->addWidget(m_link2dCameras);
    viewLayoutLayout->setContentsMargins(0, 0, 0, 0);

    m_brightnessSlider = new SliderWithLabel{brightnessToUI(0.0f), brightnessToUI(2.0f), 0, "%d%%", 350, 45};
    m_brightnessSlider->setToolTip("Sets the brightness for textures and model skins in the 3D editing view.");

    m_UIBrightnessSlider = new SliderWithLabel{brightnessToUI(0.5f), brightnessToUI(1.5f), 0, "%d%%", 350, 45};
    m_UIBrightnessSlider->setToolTip("Sets the contrast for UI controls");

    m_gridAlphaSlider = new SliderWithLabel{-50, +50, 0, "%d%%", 350, 45};
    m_gridAlphaSlider->setToolTip("Sets the visibility of the grid lines in the 3D editing view.");

    m_gridWidthSlider = new SliderWithLabel{10, 300, 10e-3f, "%.1f px", 350, 45};
    m_gridWidthSlider->setToolTip("Sets the thickness of the grid-lines.");

    m_fovSlider = new SliderWithLabel{50, 150, 0, "%d°", 350, 45};
    m_fovSlider->setToolTip("Sets the field of vision in the 3D editing view.");

    m_showAxes = new QCheckBox{};
    m_showAxes->setToolTip("Toggle showing the coordinate system axes in the 3D editing view.");

    m_unitsDisplayType = new QComboBox{};
    m_unitsDisplayType->addItem("Units");
    m_unitsDisplayType->addItem("Meters");
    m_unitsDisplayType->addItem("Both");
    m_unitsDisplayType->setToolTip("How to display units.");

    m_metricConversationFactor = new QLineEdit{};
    m_metricConversationFactor->setToolTip("Specifies how many units equal 1 meter.");
    m_metricConversationFactor->setMaximumWidth(70);
    m_metricConversationFactor->setAlignment(Qt::AlignRight);
    m_metricConversationFactor->setValidator(new QDoubleValidator{1.0, 60.0, 4, m_metricConversationFactor});

    auto *unitsFactorLayout = new QHBoxLayout{};
    auto unitLabel = new QLabel{"units/meter"};
    makeSmall(unitLabel);
    unitsFactorLayout->setContentsMargins(0, 0, 0, 0);
    unitsFactorLayout->addWidget(m_metricConversationFactor);
    unitsFactorLayout->addSpacing(0);
    unitsFactorLayout->addWidget(unitLabel);

    m_autoBrightnessTypeCombo = new QComboBox;
    m_autoBrightnessTypeCombo->setToolTip("Sets how color tinting on objects should behave.");
    m_autoBrightnessTypeCombo->addItem("as is");
    m_autoBrightnessTypeCombo->addItem("average");
    m_autoBrightnessTypeCombo->addItem("peak");

    m_textureModeCombo = new QComboBox{};
    m_textureModeCombo->setToolTip("Sets the texture filtering mode in the editing views.");
    for (const auto &textureMode: TextureModes) {
        m_textureModeCombo->addItem(QString::fromStdString(textureMode.name));
    }

    m_enableMsaa = new QCheckBox{};
    m_enableMsaa->setToolTip("Enable multisampling");

    m_textureBrowserIconSizeCombo = new QComboBox{};
    m_textureBrowserIconSizeCombo->addItem("25%");
    m_textureBrowserIconSizeCombo->addItem("50%");
    m_textureBrowserIconSizeCombo->addItem("100%");
    m_textureBrowserIconSizeCombo->addItem("150%");
    m_textureBrowserIconSizeCombo->addItem("200%");
    m_textureBrowserIconSizeCombo->addItem("250%");
    m_textureBrowserIconSizeCombo->addItem("300%");
    m_textureBrowserIconSizeCombo->setToolTip("Sets the icon size in the texture browser.");

    m_rendererFontCombo = new QComboBox;
    m_rendererFontCombo->setEditable(false);

    m_UIFontCombo = new QComboBox;
    m_UIFontCombo->setEditable(false);

    m_ConsoleFontCombo = new QComboBox;
    m_ConsoleFontCombo->setEditable(false);

    // find fonts folder
    auto fontsPath = IO::SystemPaths::findResourceDirectories(std::filesystem::path("fonts").parent_path());

    if (!fontsPath.empty()) {
        auto m_fs = std::make_unique<IO::DiskFileSystem>(fontsPath.back());

        // default font configured
        m_rendererFontCombo->addItem("[default]");
        m_UIFontCombo->addItem("[default]");
        m_ConsoleFontCombo->addItem("[default]");
        font_files.push_back("[default]");

        // default system font
        QFont font;
        m_rendererFontCombo->addItem("[system: " + font.defaultFamily() + "]");
        m_UIFontCombo->addItem("[system: " + font.defaultFamily() + "]");
        m_ConsoleFontCombo->addItem("[system: " + font.defaultFamily() + "]");
        font_files.push_back(font.defaultFamily().toStdString());

        // search compatible fonts
        auto res = m_fs->find("fonts", IO::TraversalMode::Recursive, IO::makeExtensionPathMatcher({".ttf", ".otf"}));

        auto tmp_files = std::vector<std::filesystem::path>();

        if (res.is_success()) {
            for (const auto &item: res.value()) {
                tmp_files.push_back(item);
            }

            std::sort(tmp_files.begin(), tmp_files.end());

            for (const auto &item: tmp_files) {
                auto filename = "[" + QString::fromStdString(item.filename()) + "]";
                m_rendererFontCombo->addItem(filename);
                m_UIFontCombo->addItem(filename);
                m_ConsoleFontCombo->addItem(filename);
                font_files.push_back(item);
            }
        }

        // installed fonts
        QFontDatabase database;
        for (auto family: database.families()) {
            m_rendererFontCombo->addItem(family);
            m_UIFontCombo->addItem(family);
            m_ConsoleFontCombo->addItem(family);
            font_files.push_back(family.toStdString().c_str());
        }

        if (font_files.empty()) {
            m_rendererFontCombo->addItem("no local fonts found.");
            m_UIFontCombo->addItem("no local fonts found.");
            m_ConsoleFontCombo->addItem("no fonts found.");
        }
    }

    m_rendererFontSizeCombo = new QComboBox{};
    m_rendererFontSizeCombo->setEditable(true);
    m_rendererFontSizeCombo->setToolTip("Sets the font size for various labels in the editing views.");
    m_rendererFontSizeCombo->addItems({
            "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "22", "24", "26", "28", "32", "36", "40", "48", "56", "64", "72"
        }
    );
    m_rendererFontSizeCombo->setValidator(new QIntValidator{1, 96, m_rendererFontSizeCombo});

    m_UIFontSizeCombo = new QComboBox{};
    m_UIFontSizeCombo->setEditable(true);
    m_UIFontSizeCombo->setToolTip("Sets the font size UI elements.");
    m_UIFontSizeCombo->addItems({"8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20"});
    m_UIFontSizeCombo->setValidator(new QIntValidator{1, 20, m_UIFontSizeCombo});

    m_ConsoleFontSizeCombo = new QComboBox{};
    m_ConsoleFontSizeCombo->setEditable(true);
    m_ConsoleFontSizeCombo->setToolTip("Sets the font size for the console panel.");
    m_ConsoleFontSizeCombo->addItems({
            "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "22", "24", "26", "28", "32", "36", "40", "48", "56", "64", "72"
        }
    );
    m_ConsoleFontSizeCombo->setValidator(new QIntValidator{1, 96, m_ConsoleFontSizeCombo});


    m_ToolbarIconSizeCombo = new QComboBox{};
    m_ToolbarIconSizeCombo->setEditable(false);
    m_ToolbarIconSizeCombo->setToolTip("Sets the icon size for the main toolbar.");
    m_ToolbarIconSizeCombo->addItems({
            "14x14", "16x16", "18x18", "20x20", "22x22", "24x24", "26x26", "28x28", "30x30", "32x32"
        }
    );

    /*************************************************************************************************************************/

    auto *layout = new FormWithSectionsLayout{};
    layout->setContentsMargins(LayoutConstants::MediumHMargin, LayoutConstants::WideHMargin, 0, 0);
    layout->setVerticalSpacing(LayoutConstants::MediumVMargin);
    layout->setHorizontalSpacing(LayoutConstants::WideHMargin * 2);
    // override the default to make the sliders take up maximum width
    layout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    layout->addSection("User Interface");
    layout->addRow("Theme", themeLayout);
    layout->addRow("Brightness", m_UIBrightnessSlider);
    layout->addRow("Toolbar Icon-Size", m_ToolbarIconSizeCombo);

    layout->addSection("Fonts");
    layout->addRow("UI Font", m_UIFontCombo, m_UIFontSizeCombo);
    layout->addRow("Console Font", m_ConsoleFontCombo, m_ConsoleFontSizeCombo);
    layout->addRow("Renderer Font", m_rendererFontCombo, m_rendererFontSizeCombo);

    layout->addSection("Map Views");
    layout->addRow("Layout", viewLayoutLayout);
    layout->addRow("View Brightness", m_brightnessSlider);
    layout->addRow("Tint Brightness", m_autoBrightnessTypeCombo);
    layout->addRow("Grid Alpha", m_gridAlphaSlider);
    layout->addRow("Grid Width", m_gridWidthSlider);
    layout->addRow("FOV", m_fovSlider);
    layout->addRow("Unit Labeling", m_unitsDisplayType);
    layout->addRow("Metric Conversation Factor", unitsFactorLayout);
    layout->addRow("Show Axes", m_showAxes);
    layout->addRow("Texture Mode", m_textureModeCombo);
    layout->addRow("Enable Multisampling", m_enableMsaa);

    layout->addSection("Texture Browser");
    layout->addRow("Icon-Size", m_textureBrowserIconSizeCombo);


    viewBox->setMinimumWidth(550);
    viewBox->setLayout(layout);

    return viewBox;
}

void ViewPreferencePane::bindEvents() {
    connect(m_layoutCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::layoutChanged);
    connect(m_link2dCameras, &QCheckBox::stateChanged, this, &ViewPreferencePane::link2dCamerasChanged);
    connect(m_brightnessSlider, &SliderWithLabel::valueChanged, this, &ViewPreferencePane::brightnessChanged);
    connect(m_UIBrightnessSlider, &SliderWithLabel::valueChanged, this, &ViewPreferencePane::UIBrightnessChanged);
    connect(m_gridAlphaSlider, &SliderWithLabel::valueChanged, this, &ViewPreferencePane::gridAlphaChanged);
    connect(m_fovSlider, &SliderWithLabel::valueChanged, this, &ViewPreferencePane::fovChanged);
    connect(m_unitsDisplayType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::unitsDisplayTypeIndexChanged);
    connect(m_metricConversationFactor, &QLineEdit::textChanged, this, &ViewPreferencePane::metricConversationFactorChanged);
    connect(m_showAxes, &QCheckBox::stateChanged, this, &ViewPreferencePane::showAxesChanged);
    connect(m_enableMsaa, &QCheckBox::stateChanged, this, &ViewPreferencePane::enableMsaaChanged);
    connect(m_themeCombo, QOverload<int>::of(&QComboBox::activated), this, &ViewPreferencePane::themeChanged);
    connect(m_textureModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::textureModeChanged);
    connect(m_textureBrowserIconSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::textureBrowserIconSizeChanged);

    connect(m_autoBrightnessTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::editorFaceAutoBrightnessChanged);
    connect(m_gridWidthSlider, &SliderWithLabel::valueChanged, this, &ViewPreferencePane::editorGridWithChanged);

    connect(m_rendererFontCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::renderFontFileChanged);
    connect(m_rendererFontSizeCombo, &QComboBox::currentTextChanged, this, &ViewPreferencePane::rendererFontSizeChanged);

    connect(m_UIFontCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::editorUIFontFileChanged);
    connect(m_UIFontSizeCombo, &QComboBox::currentTextChanged, this, &ViewPreferencePane::editorUIFontSizeChanged);

    connect(m_ConsoleFontCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::editorConsoleFontFileChanged);
    connect(m_ConsoleFontSizeCombo, &QComboBox::currentTextChanged, this, &ViewPreferencePane::editorConsoleFontSizeChanged);

    connect(m_ToolbarIconSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::editorToolbarIconSizeChanged);
}

void ViewPreferencePane::unBindEvents() {
    disconnect(m_layoutCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::layoutChanged);
    disconnect(m_link2dCameras, &QCheckBox::stateChanged, this, &ViewPreferencePane::link2dCamerasChanged);
    disconnect(m_brightnessSlider, &SliderWithLabel::valueChanged, this, &ViewPreferencePane::brightnessChanged);
    disconnect(m_UIBrightnessSlider, &SliderWithLabel::valueChanged, this, &ViewPreferencePane::UIBrightnessChanged);
    disconnect(m_gridAlphaSlider, &SliderWithLabel::valueChanged, this, &ViewPreferencePane::gridAlphaChanged);
    disconnect(m_fovSlider, &SliderWithLabel::valueChanged, this, &ViewPreferencePane::fovChanged);
    disconnect(m_unitsDisplayType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::unitsDisplayTypeIndexChanged);
    disconnect(m_metricConversationFactor, &QLineEdit::textChanged, this, &ViewPreferencePane::metricConversationFactorChanged);
    disconnect(m_showAxes, &QCheckBox::stateChanged, this, &ViewPreferencePane::showAxesChanged);
    disconnect(m_enableMsaa, &QCheckBox::stateChanged, this, &ViewPreferencePane::enableMsaaChanged);
    disconnect(m_themeCombo, QOverload<int>::of(&QComboBox::activated), this, &ViewPreferencePane::themeChanged);
    disconnect(m_textureModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::textureModeChanged);
    disconnect(m_textureBrowserIconSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::textureBrowserIconSizeChanged);

    disconnect(m_autoBrightnessTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::editorFaceAutoBrightnessChanged);
    disconnect(m_gridWidthSlider, &SliderWithLabel::valueChanged, this, &ViewPreferencePane::editorGridWithChanged);

    disconnect(m_rendererFontCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::renderFontFileChanged);
    disconnect(m_rendererFontSizeCombo, &QComboBox::currentTextChanged, this, &ViewPreferencePane::rendererFontSizeChanged);

    disconnect(m_UIFontCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::editorUIFontFileChanged);
    disconnect(m_UIFontSizeCombo, &QComboBox::currentTextChanged, this, &ViewPreferencePane::editorUIFontSizeChanged);

    disconnect(m_ConsoleFontCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::editorConsoleFontFileChanged);
    disconnect(m_ConsoleFontSizeCombo, &QComboBox::currentTextChanged, this, &ViewPreferencePane::editorConsoleFontSizeChanged);

    disconnect(m_ToolbarIconSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::editorToolbarIconSizeChanged);
}

bool ViewPreferencePane::doCanResetToDefaults() {
    return true;
}

void ViewPreferencePane::doResetToDefaults() {
    unBindEvents();

    auto &prefs = PreferenceManager::instance();
    prefs.resetToDefault(Preferences::MapViewLayout);
    prefs.resetToDefault(Preferences::Link2DCameras);
    prefs.resetToDefault(Preferences::Brightness);
    prefs.resetToDefault(Preferences::GridLineWidth);
    prefs.resetToDefault(Preferences::FaceAutoBrightness);
    prefs.resetToDefault(Preferences::GridAlpha);
    prefs.resetToDefault(Preferences::CameraFov);
    prefs.resetToDefault(Preferences::UnitsDisplayType);
    prefs.resetToDefault(Preferences::MetricConversationFactor);
    prefs.resetToDefault(Preferences::ShowAxes);
    prefs.resetToDefault(Preferences::EnableMSAA);
    prefs.resetToDefault(Preferences::TextureMinFilter);
    prefs.resetToDefault(Preferences::TextureMagFilter);
    prefs.resetToDefault(Preferences::Theme);
    prefs.resetToDefault(Preferences::TextureBrowserIconSize);
    prefs.resetToDefault(Preferences::RendererFontSize);
    prefs.resetToDefault(Preferences::UIFontSize);
    prefs.resetToDefault(Preferences::RendererFontPath);
    prefs.resetToDefault(Preferences::UIFontPath);

    bindEvents();
}

void ViewPreferencePane::doUpdateControls() {
    unBindEvents();

    m_layoutCombo->setCurrentIndex(pref(Preferences::MapViewLayout));
    m_link2dCameras->setChecked(pref(Preferences::Link2DCameras));
    m_brightnessSlider->setValue(brightnessToUI(pref(Preferences::Brightness)));
    m_UIBrightnessSlider->setValue(brightnessToUI(pref(Preferences::UIBrightness)));
    m_gridAlphaSlider->setRatio(pref(Preferences::GridAlpha));
    m_fovSlider->setValue(int(pref(Preferences::CameraFov)));

    m_unitsDisplayType->setCurrentIndex(pref(Preferences::UnitsDisplayType));
    m_metricConversationFactor->setText(QString::asprintf("%.4f", pref(Preferences::MetricConversationFactor)));

    m_gridWidthSlider->setValue(int(pref(Preferences::GridLineWidth) * 100));
    m_autoBrightnessTypeCombo->setCurrentIndex(pref(Preferences::FaceAutoBrightness));

    const auto textureModeIndex = findTextureMode(pref(Preferences::TextureMinFilter), pref(Preferences::TextureMagFilter));
    m_textureModeCombo->setCurrentIndex(int(textureModeIndex));

    m_showAxes->setChecked(pref(Preferences::ShowAxes));
    m_enableMsaa->setChecked(pref(Preferences::EnableMSAA));
    m_themeCombo->setCurrentIndex(findThemeIndex(pref(Preferences::Theme)));

    const auto textureBrowserIconSize = pref(Preferences::TextureBrowserIconSize);
    if (textureBrowserIconSize == 0.25f) {
        m_textureBrowserIconSizeCombo->setCurrentIndex(0);
    }
    else if (textureBrowserIconSize == 0.5f) {
        m_textureBrowserIconSizeCombo->setCurrentIndex(1);
    }
    else if (textureBrowserIconSize == 1.5f) {
        m_textureBrowserIconSizeCombo->setCurrentIndex(3);
    }
    else if (textureBrowserIconSize == 2.0f) {
        m_textureBrowserIconSizeCombo->setCurrentIndex(4);
    }
    else if (textureBrowserIconSize == 2.5f) {
        m_textureBrowserIconSizeCombo->setCurrentIndex(5);
    }
    else if (textureBrowserIconSize == 3.0f) {
        m_textureBrowserIconSizeCombo->setCurrentIndex(6);
    }
    else {
        m_textureBrowserIconSizeCombo->setCurrentIndex(2);
    }

    auto iconSize = (pref(Preferences::ToolBarIconsSize) - 14) / 2;
    m_ToolbarIconSizeCombo->setCurrentIndex(iconSize);

    m_rendererFontSizeCombo->setCurrentText(QString::asprintf("%i", pref(Preferences::RendererFontSize)));
    m_UIFontSizeCombo->setCurrentText(QString::asprintf("%i", pref(Preferences::UIFontSize)));
    m_ConsoleFontSizeCombo->setCurrentText(QString::asprintf("%i", pref(Preferences::ConsoleFontSize)));

    auto render_font = pref(Preferences::RendererFontPath);
    auto render_font_found = std::find(font_files.begin(), font_files.end(), render_font);

    if (render_font_found != font_files.end()) {
        auto index = render_font_found - font_files.begin();
        m_rendererFontCombo->setCurrentIndex(int(index));
    }

    auto ui_font = pref(Preferences::UIFontPath);
    auto ui_font_found = std::find(font_files.begin(), font_files.end(), ui_font);

    if (ui_font_found != font_files.end()) {
        auto index = ui_font_found - font_files.begin();
        m_UIFontCombo->setCurrentIndex(int(index));
    }

    auto console_font = pref(Preferences::ConsoleFontPath);
    auto console_font_found = std::find(font_files.begin(), font_files.end(), console_font);

    if (console_font_found != font_files.end()) {
        auto index = console_font_found - font_files.begin();
        m_ConsoleFontCombo->setCurrentIndex(int(index));
    }

    bindEvents();
}

bool ViewPreferencePane::doValidate() {
    return true;
}

size_t ViewPreferencePane::findTextureMode(const int minFilter, const int magFilter) const {
    for (size_t i = 0; i < TextureModes.size(); ++i) {
        if (TextureModes[i].minFilter == minFilter && TextureModes[i].magFilter == magFilter) {
            return i;
        }
    }
    return TextureModes.size();
}

int ViewPreferencePane::findThemeIndex(const QString &theme) {
    for (int i = 0; i < m_themeCombo->count(); ++i) {
        if (m_themeCombo->itemText(i) == theme) {
            return i;
        }
    }
    return 0;
}

void ViewPreferencePane::layoutChanged(const int index) {
    assert(index >= 0 && index < 4);

    auto &prefs = PreferenceManager::instance();
    prefs.set(Preferences::MapViewLayout, index);
}

void ViewPreferencePane::link2dCamerasChanged(const int state) {
    const auto value = state == Qt::Checked;
    auto &prefs = PreferenceManager::instance();
    prefs.set(Preferences::Link2DCameras, value);
}

void ViewPreferencePane::brightnessChanged(const int value) {
    auto &prefs = PreferenceManager::instance();
    prefs.set(Preferences::Brightness, brightnessFromUI(value));
}

void ViewPreferencePane::UIBrightnessChanged(const int value) {
    auto &prefs = PreferenceManager::instance();
    prefs.set(Preferences::UIBrightness, brightnessFromUI(value));
    reloadUIStyle(false);
}

void ViewPreferencePane::gridAlphaChanged(const int /* value */) {
    const auto ratio = m_gridAlphaSlider->ratio();
    auto &prefs = PreferenceManager::instance();
    prefs.set(Preferences::GridAlpha, ratio);
}

void ViewPreferencePane::fovChanged(const int value) {
    auto &prefs = PreferenceManager::instance();
    prefs.set(Preferences::CameraFov, float(value));
}

void ViewPreferencePane::unitsDisplayTypeIndexChanged(const int index) {
    auto &prefs = PreferenceManager::instance();
    prefs.set(Preferences::UnitsDisplayType, index);
}

void ViewPreferencePane::metricConversationFactorChanged(const QString &text) {
    bool ok;
    const auto value = text.toFloat(&ok);

    if (ok) {
        auto &prefs = PreferenceManager::instance();
        prefs.set(Preferences::MetricConversationFactor, value);
    }
}

void ViewPreferencePane::showAxesChanged(const int state) {
    const auto value = state == Qt::Checked;
    auto &prefs = PreferenceManager::instance();
    prefs.set(Preferences::ShowAxes, value);
}

void ViewPreferencePane::enableMsaaChanged(const int state) {
    const auto value = state == Qt::Checked;
    auto &prefs = PreferenceManager::instance();
    prefs.set(Preferences::EnableMSAA, value);
}

void ViewPreferencePane::textureModeChanged(const int value) {
    const auto index = static_cast<size_t>(value);
    assert(index < TextureModes.size());
    const auto minFilter = TextureModes[index].minFilter;
    const auto magFilter = TextureModes[index].magFilter;

    auto &prefs = PreferenceManager::instance();
    prefs.set(Preferences::TextureMinFilter, minFilter);
    prefs.set(Preferences::TextureMagFilter, magFilter);
}

void ViewPreferencePane::themeChanged(int /*index*/) {
    auto &prefs = PreferenceManager::instance();
    prefs.set(Preferences::Theme, m_themeCombo->currentText());
}

void ViewPreferencePane::textureBrowserIconSizeChanged(const int index) {
    auto &prefs = PreferenceManager::instance();

    switch (index) {
        case 0:
            prefs.set(Preferences::TextureBrowserIconSize, 0.25f);
            break;
        case 1:
            prefs.set(Preferences::TextureBrowserIconSize, 0.5f);
            break;
        case 2:
            prefs.set(Preferences::TextureBrowserIconSize, 1.0f);
            break;
        case 3:
            prefs.set(Preferences::TextureBrowserIconSize, 1.5f);
            break;
        case 4:
            prefs.set(Preferences::TextureBrowserIconSize, 2.0f);
            break;
        case 5:
            prefs.set(Preferences::TextureBrowserIconSize, 2.5f);
            break;
        case 6:
            prefs.set(Preferences::TextureBrowserIconSize, 3.0f);
            break;
    }
}

void ViewPreferencePane::rendererFontSizeChanged(const QString &str) {
    bool ok;
    const auto value = str.toInt(&ok);
    if (ok) {
        auto &prefs = PreferenceManager::instance();
        prefs.set(Preferences::RendererFontSize, value);
    }
}

void ViewPreferencePane::renderFontFileChanged(const int index) {
    assert(index >= 0 && size_t(index) < font_files.size());

    auto &prefs = PreferenceManager::instance();

    // default
    if (index == 0) {
        prefs.resetToDefault(Preferences::RendererFontPath);
        prefs.resetToDefault(Preferences::RendererFontSize);
    }
    else {
        prefs.set(Preferences::RendererFontPath, font_files[size_t(index)]);
    }
}

void ViewPreferencePane::reloadUIStyle(bool reloadFonts) {
    TrenchBroomApp::instance().reloadStyle(reloadFonts, true);
}

void ViewPreferencePane::editorUIFontSizeChanged(const QString &text) {
    bool ok;
    const auto value = text.toInt(&ok);

    if (ok) {
        auto &prefs = PreferenceManager::instance();
        prefs.set(Preferences::UIFontSize, value);
    }

    reloadUIStyle();
}


void ViewPreferencePane::editorUIFontFileChanged(const int index) {
    assert(index >= 0 && size_t(index) < font_files.size());

    auto &prefs = PreferenceManager::instance();

    // default
    if (index == 0) {
        prefs.resetToDefault(Preferences::UIFontPath);
        prefs.resetToDefault(Preferences::UIFontSize);
    }
    else {
        prefs.set(Preferences::UIFontPath, font_files[size_t(index)]);
    }

    reloadUIStyle();
}

void ViewPreferencePane::editorConsoleFontSizeChanged(const QString &text) {
    bool ok;
    const auto value = text.toInt(&ok);

    if (ok) {
        auto &prefs = PreferenceManager::instance();
        prefs.set(Preferences::ConsoleFontSize, value);
    }

    reloadUIStyle();
}

void ViewPreferencePane::editorConsoleFontFileChanged(int index) {
    assert(index >= 0 && size_t(index) < font_files.size());

    auto &prefs = PreferenceManager::instance();

    // default
    if (index == 0) {
        prefs.resetToDefault(Preferences::ConsoleFontPath);
        prefs.resetToDefault(Preferences::ConsoleFontSize);
    }
    else {
        prefs.set(Preferences::ConsoleFontPath, font_files[size_t(index)]);
    }

    reloadUIStyle();
}

void ViewPreferencePane::editorToolbarIconSizeChanged(int index) {
    auto &prefs = PreferenceManager::instance();

    // start at font-size 14px
    auto size = 14 + index * 2;
    prefs.set(Preferences::ToolBarIconsSize, size);

    reloadUIStyle();
}

void ViewPreferencePane::editorFaceAutoBrightnessChanged(int value) {
    auto &prefs = PreferenceManager::instance();
    prefs.set(Preferences::FaceAutoBrightness, value);
}

void ViewPreferencePane::editorGridWithChanged(int value) {
    auto &prefs = PreferenceManager::instance();
    prefs.set(Preferences::GridLineWidth, float(value) / 100.0f);
}
} // namespace TrenchBroom::View
