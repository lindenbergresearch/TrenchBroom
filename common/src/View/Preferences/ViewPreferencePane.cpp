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

#include "View/Preferences/ViewPreferencePane.h"
#include <QSpinBox>
#include <QSignalBlocker>

#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QFontInfo>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QUrl>
#include <QtGlobal>

#include "Error.h"
#include "IO/DiskFileSystem.h"
#include "IO/File.h"
#include "IO/PathInfo.h"
#include "IO/PathMatcher.h"
#include "IO/PathQt.h"
#include "IO/SystemPaths.h"
#include "IO/TraversalMode.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include "Renderer/GL.h"
#include "View/Widgets/ColorButton.h"
#include "View/Overlay/ContextOverlay.h"
#include "View/Preferences/FontUtils.h"
#include "View/Widgets/FormWithSectionsLayout.h"
#include "View/Theme/QtUtils.h"
#include "View/Widgets/SliderWithLabel.h"
#include "View/Theme/ViewConstants.h"

#include <kdl/overload.h>

#include <vm/scalar.h>

#include <TrenchBroomApp.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <string>

namespace TrenchBroom::View {
namespace {
struct TextureMode {
  int minFilter;
  int magFilter;
  std::string name;

  TextureMode(const int i_minFilter, const int i_magFilter, std::string i_name) : minFilter{i_minFilter}, magFilter{i_magFilter}, name{std::move(i_name)} {
  }
};

const auto TextureModes = std::array<TextureMode, 6>{
    TextureMode{GL_NEAREST, GL_NEAREST, "Nearest"}, TextureMode{GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, "Nearest (mipmapped)"},
    TextureMode{GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST, "Nearest (mipmapped, interpolated)"}, TextureMode{GL_LINEAR, GL_LINEAR, "Linear"}, TextureMode{GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR, "Linear (mipmapped)"},
    TextureMode{GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, "Linear (mipmapped, interpolated)"},
};

constexpr int brightnessToUI(const float value) {
    return int(vm::round(100.0f * (value - 1.0f)));
}

constexpr float brightnessFromUI(const int value) {
    return (float(value) / 100.0f) + 1.0f;
}

void selectFont(QComboBox& combo, const std::filesystem::path& path) {
    const auto value = IO::pathAsQString(path);
    auto index = combo.findData(value);
    if (index < 0) {
        combo.addItem(value, value);
        index = combo.count() - 1;
    }
    const QSignalBlocker blocker(&combo);
    combo.setCurrentIndex(index);
}

void selectFontSize(QComboBox& combo, const int size) {
    const auto value = QString::number(size);
    const QSignalBlocker blocker(&combo);
    if (combo.findText(value) < 0) combo.addItem(value);
    combo.setCurrentText(value);
}

static_assert(0 == brightnessToUI(brightnessFromUI(0)));
} // namespace

class FontComparisonPreview : public QFrame {
    QFont m_titleFont;
    QFont m_detailFont;
    ListFontSettings m_settings;
    qreal m_detailOpacity = 1;

public:
    explicit FontComparisonPreview(QWidget* parent) : QFrame(parent) {
        setProperty("fontPreview", true);
        setBackgroundRole(QPalette::Base);
        setAutoFillBackground(true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    QString family() const { return QFontInfo{m_titleFont}.family(); }

    QString description() const {
        const auto axes = fontCapabilities(m_titleFont);
        const auto weight = m_titleFont.isVariableAxisSet("wght")
            ? qRound(m_titleFont.variableAxisValue("wght")) : QFontInfo{m_titleFont}.weight();
        return axes.opticalSize
            ? tr("Weight %1 · Optical size %2 pt").arg(weight)
                .arg(QLocale{}.toString(m_titleFont.variableAxisValue("opsz"), 'f', 1))
            : tr("Weight %1 · Optical size unavailable").arg(weight);
    }

    void refresh(const std::filesystem::path& path, const int size, const int minimum,
                 const qreal opacity, const ListFontSettings& settings) {
        m_settings = settings;
        m_detailOpacity = std::clamp(opacity, 0.0, 1.0);
        const auto base = loadFont(path, std::max(size, minimum));
        m_titleFont = listTextFont(base, m_settings);
        auto detail = base;
        detail.setPixelSize(std::max(minimum, base.pixelSize() - 2));
        detail.setWeight(QFont::Normal);
        detail.setStyleName({});
        m_detailFont = listTextFont(detail, m_settings);
        setProperty("previewTitleFont", m_titleFont);
        setProperty("previewDetailFont", m_detailFont);
        setAccessibleDescription(tr("Title %1 pixels, details %2 pixels. %3")
            .arg(logicalFontPixelSize(m_titleFont)).arg(logicalFontPixelSize(m_detailFont)).arg(description()));
        updateGeometry();
        update();
    }

    QSize sizeHint() const override {
        const QFontMetricsF title{m_titleFont}, detail{m_detailFont};
        return {220, int(std::ceil(title.height() + 2 * detail.height()
            + 4 * LayoutConstants::MediumVMargin + 2 * frameWidth()))};
    }

    QSize minimumSizeHint() const override { return {160, sizeHint().height()}; }

protected:
    void paintEvent(QPaintEvent* event) override {
        QFrame::paintEvent(event);
        QPainter painter(this);
        const auto bounds = QRectF(contentsRect()).marginsRemoved(QMarginsF{
            LayoutConstants::MediumHMargin, LayoutConstants::MediumVMargin,
            LayoutConstants::MediumHMargin, LayoutConstants::MediumVMargin});
        const QFontMetricsF titleMetrics{m_titleFont}, detailMetrics{m_detailFont};
        auto row = QRectF(bounds.left(), bounds.top(), bounds.width(), titleMetrics.height());
        painter.setPen(palette().color(QPalette::Text));
        paintTextLine(painter, row, tr("Group Main Hall"), m_titleFont, m_settings.subpixelPositioning);
        painter.setOpacity(m_detailOpacity);
        for (const auto& text : {tr("6 brushes · 32 faces · stone_wall"), tr("Position 128, 64, 32 · light 100")}) {
            row = QRectF(bounds.left(), row.bottom() + LayoutConstants::MediumVMargin, bounds.width(), detailMetrics.height());
            paintTextLine(painter, row, text, m_detailFont, m_settings.subpixelPositioning);
        }
    }
};

ViewPreferencePane::ViewPreferencePane(QWidget *parent) : PreferencePane{parent} {
    createGui();
    doUpdateControls();
}

void ViewPreferencePane::createGui() {
    auto *viewPreferences = createViewPreferences();
    auto *scrollArea = new QScrollArea{this};
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setMinimumWidth(viewPreferences->minimumWidth() + 24);
    scrollArea->setWidget(viewPreferences);

    auto *layout = new QVBoxLayout{};
    layout->setContentsMargins(QMargins{});
    layout->setSpacing(0);

    layout->addWidget(scrollArea, 1);
    layout->addSpacing(LayoutConstants::MediumVMargin);
    setLayout(layout);
}

QWidget *ViewPreferencePane::createViewPreferences() {
    auto *viewBox = new QWidget{this};

    auto *viewPrefsHeader = new QLabel{"Map Views"};
    makeEmphasized(viewPrefsHeader);

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

    m_UIBrightnessSlider = new SliderWithLabel{brightnessToUI(0.5f), brightnessToUI(1.5f), 0, "%d%%", 350, 45};
    m_UIBrightnessSlider->setObjectName("UIBrightness");
    m_UIBrightnessSlider->setTracking(false);
    m_UIBrightnessSlider->setToolTip(tr("Sets the contrast for UI controls. Applied when the slider is released."));

    m_fovSlider = new SliderWithLabel{50, 150, 0, "%d°", 350, 45};
    m_fovSlider->setToolTip("Sets the field of vision in the 3D editing view.");

    m_showAxes = new QCheckBox{};
    m_showAxes->setToolTip("Toggle showing the coordinate system axes in the 3D editing view.");

    m_maxFPS = new QSpinBox{};
    m_maxFPS->setRange(0, 1000);
    m_maxFPS->setSuffix(tr(" FPS"));
    m_maxFPS->setSpecialValueText(tr("Unlimited"));
    m_maxFPS->setKeyboardTracking(false);
    m_maxFPS->setToolTip(tr("Limits rendering in each view. 0 disables the limit. Changes apply immediately."));

    m_unitsDisplayType = new QComboBox{};
    m_unitsDisplayType->addItem("Units");
    m_unitsDisplayType->addItem("Meters");
    m_unitsDisplayType->addItem("Both");
    m_unitsDisplayType->setToolTip("How to display length units.");

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
    for (const auto &textureMode : TextureModes) {
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
    m_UIFontCombo->setObjectName("UIFontFamily");
    m_UIFontCombo->setEditable(false);

    m_ConsoleFontCombo = new QComboBox;
    m_ConsoleFontCombo->setEditable(false);

    m_rendererFontCombo->addItem(tr("[default]"), QStringLiteral("[default]"));
    m_UIFontCombo->addItem(tr("[default]"), QStringLiteral("[default]"));
    m_ConsoleFontCombo->addItem(tr("[default]"), QStringLiteral("[default]"));
    m_UIFontCombo->addItem(tr("System font"), QStringLiteral("[system]"));

    const auto fontsPath = IO::SystemPaths::findResourceDirectories(std::filesystem::path("fonts").parent_path());
    QSet<QString> listedFonts;
    for (const auto& root : fontsPath) {
        if (!QDir(IO::pathAsQString(root)).exists("fonts")) continue;
        const auto filesystem = IO::DiskFileSystem(root);
        const auto result = filesystem.find("fonts", IO::TraversalMode::Recursive,
            IO::makeExtensionPathMatcher({".ttf", ".otf"}));
        if (result.is_success()) {
            auto files = result.value();
            std::sort(files.begin(), files.end());
            for (const auto& path : files) {
                const auto label = "[" + IO::pathAsQString(path.filename()) + "]";
                const auto value = IO::pathAsQString(path);
                if (listedFonts.contains(value)) continue;
                listedFonts.insert(value);
                m_rendererFontCombo->addItem(label, value);
                m_UIFontCombo->addItem(label, value);
                m_ConsoleFontCombo->addItem(label, value);
            }
        }
    }

    for (const auto& family : QFontDatabase::families()) {
        m_UIFontCombo->addItem(family, family);
        m_ConsoleFontCombo->addItem(family, family);
    }

    QStringList fontSizesBig = {
        "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "22", "24", "26", "28", "32", "36", "40", "48", "56", "64", "72"
    };

    QStringList fontSizesSmall = {
        "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "22", "24", "26", "28", "32"
    };

    m_rendererFontSizeCombo = new QComboBox{};
    m_rendererFontSizeCombo->setEditable(false);
    m_rendererFontSizeCombo->setToolTip("Sets the font size for various labels in the editing views.");
    m_rendererFontSizeCombo->addItems(fontSizesBig);
    m_rendererFontSizeCombo->setValidator(new QIntValidator{1, 96, m_rendererFontSizeCombo});

    m_UIFontSizeCombo = new QComboBox{};
    m_UIFontSizeCombo->setObjectName("UIFontSize");
    m_UIFontSizeCombo->setEditable(false);
    m_UIFontSizeCombo->setToolTip(tr("Sets the UI font size in logical screen pixels."));
    m_UIFontSizeCombo->addItems(fontSizesSmall);
    m_UIFontSizeCombo->setValidator(new QIntValidator{1, 20, m_UIFontSizeCombo});

    m_UIFontMinimumSize = new QSpinBox;
    m_UIFontMinimumSize->setObjectName("UIFontMinimumSize");
    m_UIFontMinimumSize->setRange(8, 32);
    m_UIFontMinimumSize->setSuffix(tr(" px"));
    m_UIFontMinimumSize->setKeyboardTracking(false);
    m_UIFontMinimumSize->setAccessibleName(tr("Minimum UI font size"));
    m_UIFontMinimumSize->setToolTip(tr("Uses this as the minimum UI font size, in logical screen pixels."));

    m_UIDetailTextOpacity = new SliderWithLabel{0, 100, 0, "%d%%", 350, 45};
    m_UIDetailTextOpacity->setObjectName("UIDetailTextOpacity");
    m_UIDetailTextOpacity->setAccessibleName(tr("Detail text opacity"));
    m_UIDetailTextOpacity->setToolTip(tr("Sets the visibility of secondary information such as object counts and texture names."));

    m_listFontWeight = new QSpinBox;
    m_listFontWeight->setObjectName("UIListFontWeight");
    m_listFontWeight->setAccessibleName(tr("Text weight"));
    m_listFontWeight->setRange(0, 1000);
    m_listFontWeight->setSingleStep(25);
    m_listFontWeight->setSpecialValueText(tr("Font default"));
    m_listFontWeight->setKeyboardTracking(false);
    m_listFontWeight->setToolTip(tr("Fine-tunes UI text weight. Variable fonts support intermediate weights; other fonts use the closest available face. Stylesheet emphasis is preserved."));
    m_listOpticalSize = new QDoubleSpinBox;
    m_listOpticalSize->setObjectName("UIListOpticalSize");
    m_listOpticalSize->setAccessibleName(tr("Optical size"));
    m_listOpticalSize->setRange(0, 144);
    m_listOpticalSize->setDecimals(1);
    m_listOpticalSize->setSingleStep(1);
    m_listOpticalSize->setSpecialValueText(tr("Automatic"));
    m_listOpticalSize->setSuffix(tr(" pt"));
    m_listOpticalSize->setKeyboardTracking(false);
    m_listOpticalSize->setToolTip(tr("Automatic follows each text size. Values are limited to the selected font's supported optical range; fonts without optical sizing remain unchanged."));
    m_listTypoLineMetrics = new QCheckBox(tr("Use typographic line spacing"));
    m_listTypoLineMetrics->setObjectName("UIListTypoLineMetrics");
    m_listTypoLineMetrics->setToolTip(tr("Uses the font's preferred line spacing. Some fonts already use these metrics."));
    m_listTextPosition = new QComboBox;
    m_listTextPosition->setObjectName("UIListTextPosition");
    m_listTextPosition->setAccessibleName(tr("Text positioning"));
    m_listTextPosition->addItems({tr("Pixel aligned"), tr("Subpixel positioning")});
    m_listTextPosition->setToolTip(tr("Pixel alignment places text baselines on physical screen pixels. Subpixel positioning uses unhinted text and permits fractional baselines."));

    auto* comparison = new QWidget;
    comparison->setObjectName("UIFontComparison");
    auto* comparisonLayout = new QGridLayout(comparison);
    comparisonLayout->setContentsMargins(0, 0, 0, 0);
    comparisonLayout->setSpacing(LayoutConstants::MediumHMargin);
    const auto addPreview = [&](const int column, const QString& title, const QString& name, QLabel*& info,
                               QLabel** selectedHeading = nullptr) {
        auto* heading = new QLabel(title);
        heading->setProperty("fontPreviewHeading", true);
        auto* header = new QHBoxLayout;
        header->addWidget(heading, 1);
        if (selectedHeading) {
            *selectedHeading = heading;
            m_applyFont = new QPushButton(tr("Apply"));
            m_applyFont->setObjectName("UIFontApply");
            m_applyFont->setAutoDefault(false);
            m_applyFont->setToolTip(tr("Apply the previewed font settings throughout the interface."));
            connect(m_applyFont, &QPushButton::clicked, this, &ViewPreferencePane::applyFontDraft);
            header->addWidget(m_applyFont);
        }
        auto* preview = new FontComparisonPreview(comparison);
        preview->setObjectName(name);
        preview->setAccessibleName(title);
        info = new QLabel(preview->description());
        info->setObjectName(name + "Info");
        info->setProperty("fontPreviewInfo", true);
        info->setWordWrap(true);
        comparisonLayout->addLayout(header, 0, column);
        comparisonLayout->addWidget(preview, 1, column);
        comparisonLayout->addWidget(info, 2, column);
        comparisonLayout->setColumnStretch(column, 1);
        return preview;
    };
    m_systemFontPreview = addPreview(0, tr("System font"), "UIFontPreview_System", m_systemFontInfo);
    m_selectedFontPreview = addPreview(1, tr("Selected font"), "UIFontPreview_Selected", m_selectedFontInfo, &m_selectedFontHeading);
    auto* comparisonScope = new QLabel(tr("Changes affect only these samples. Apply uses them throughout the interface."));
    comparisonScope->setObjectName("UIFontComparisonScope");
    comparisonScope->setWordWrap(true);

    m_ConsoleFontSizeCombo = new QComboBox{};
    m_ConsoleFontSizeCombo->setEditable(false);
    m_ConsoleFontSizeCombo->setToolTip("Sets the font size for the console panel.");
    m_ConsoleFontSizeCombo->addItems(fontSizesBig);
    m_ConsoleFontSizeCombo->setValidator(new QIntValidator{1, 96, m_ConsoleFontSizeCombo});

    m_ToolbarIconSizeCombo = new QComboBox{};
    m_ToolbarIconSizeCombo->setEditable(false);
    m_ToolbarIconSizeCombo->setToolTip("Sets the icon size for the main toolbar.");
    m_ToolbarIconSizeCombo->addItems(
        {
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
    layout->addRow("Brightness", m_UIBrightnessSlider);
    layout->addRow("Toolbar Icon-Size", m_ToolbarIconSizeCombo);

    m_contextMenuIcons = new QCheckBox(tr("Show action icons"));
    m_contextMenuIcons->setObjectName("ContextMenuIcons");
    m_contextMenuShortcuts = new QCheckBox(tr("Show keyboard shortcuts"));
    m_contextMenuShortcuts->setObjectName("ContextMenuShortcuts");
    layout->addSection(tr("Context Menus"));
    layout->addRow(tr("Icons"),m_contextMenuIcons);
    layout->addRow(tr("Shortcuts"),m_contextMenuShortcuts);

    layout->addSection(tr("Map Loading"));
    m_showLoadDiagnostics = new QCheckBox(tr("Show warnings and errors after loading a map"));
    m_showLoadDiagnostics->setObjectName("ShowMapLoadDiagnostics");
    layout->addRow(tr("Load summary"), m_showLoadDiagnostics);

    layout->addSection("Fonts");
    layout->addRow("UI Font", m_UIFontCombo, m_UIFontSizeCombo);
    layout->addRow(tr("Minimum UI size"), m_UIFontMinimumSize);
    layout->addRow(tr("Detail text opacity"), m_UIDetailTextOpacity);
    layout->addSection(tr("Font comparison"));
    layout->addRow(comparisonScope);
    layout->addRow(tr("Text weight"), m_listFontWeight);
    layout->addRow(tr("Optical size"), m_listOpticalSize);
    layout->addRow(tr("Line spacing"), m_listTypoLineMetrics);
    layout->addRow(tr("Text positioning"), m_listTextPosition);
    layout->addRow(comparison);
    layout->addSection(tr("Other fonts"));
    layout->addRow("Console Font", m_ConsoleFontCombo, m_ConsoleFontSizeCombo);
    layout->addRow("Renderer Font", m_rendererFontCombo, m_rendererFontSizeCombo);

    layout->addSection("Map Views");
    layout->addRow("Layout", viewLayoutLayout);
    layout->addRow("Tint Brightness", m_autoBrightnessTypeCombo);
    layout->addRow("FOV", m_fovSlider);
    layout->addRow("Length Label Unit", m_unitsDisplayType);
    layout->addRow("Metric Conversation Factor", unitsFactorLayout);
    layout->addRow("Show Axes", m_showAxes);
    layout->addRow("Texture Mode", m_textureModeCombo);
    layout->addRow("Enable Multisampling", m_enableMsaa);
    layout->addRow(tr("Maximum FPS"), m_maxFPS);

    layout->addSection(tr("3D Context Overlay"));
    m_showContextOverlay = new QCheckBox(tr("Show tool and selection information"));
    m_contextOverlayTheme = new QLineEdit;
    m_contextOverlayTheme->setPlaceholderText(tr("Built-in theme"));
    m_contextOverlayTheme->setToolTip(tr("Theme folder with overlay.json, HTML templates and a QSS stylesheet. Saved changes reload automatically. Leave empty for the built-in theme."));
    auto* browseTheme = new QPushButton(tr("Browse…"));
    auto* createTheme = new QPushButton(tr("Create theme…"));
    auto* openTheme = new QPushButton(tr("Open folder"));
    auto* themePathLayout = new QHBoxLayout;
    themePathLayout->setContentsMargins(0, 0, 0, 0);
    themePathLayout->addWidget(m_contextOverlayTheme);
    themePathLayout->addWidget(browseTheme);
    auto* themeButtons = new QHBoxLayout;
    themeButtons->setContentsMargins(0, 0, 0, 0);
    themeButtons->addWidget(createTheme);
    themeButtons->addWidget(openTheme);
    themeButtons->addStretch();
    layout->addRow(tr("Display"), m_showContextOverlay);
    layout->addRow(tr("Theme folder"), themePathLayout);
    layout->addRow(QString{}, themeButtons);
    connect(browseTheme, &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getExistingDirectory(this, tr("Choose overlay theme"), m_contextOverlayTheme->text());
        if (!path.isEmpty()) {
            m_contextOverlayTheme->setText(path);
            contextOverlayThemeChanged();
        }
    });
    connect(createTheme, &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getExistingDirectory(this, tr("Choose an empty folder for your overlay theme"));
        if (path.isEmpty()) return;
        const QDir target(path);
        if (!target.entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden).isEmpty()) {
            QMessageBox::information(this, tr("Create overlay theme"), tr("Please choose an empty folder. Existing files will be preserved."));
            return;
        }
        const QDir source(ContextOverlay::defaultThemeDirectory());
        const auto files = source.entryList(QDir::Files);
        if (files.isEmpty()) {
            QMessageBox::warning(this, tr("Create overlay theme"), tr("The built-in theme could not be found."));
            return;
        }
        for (const auto& name : files) {
            if (!QFile::copy(source.filePath(name), target.filePath(name))) {
                QMessageBox::warning(this, tr("Create overlay theme"), tr("Could not copy %1 into the theme folder.").arg(name));
                return;
            }
        }
        m_contextOverlayTheme->setText(path);
        contextOverlayThemeChanged();
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
    connect(openTheme, &QPushButton::clicked, this, [this] {
        const auto path = m_contextOverlayTheme->text().trimmed();
        QDesktopServices::openUrl(QUrl::fromLocalFile(path.isEmpty() ? ContextOverlay::defaultThemeDirectory() : path));
    });

    layout->addSection("Texture Browser");
    layout->addRow("Icon-Size", m_textureBrowserIconSizeCombo);

    viewBox->setMinimumWidth(550);
    viewBox->setLayout(layout);

    return viewBox;
}

void ViewPreferencePane::bindEvents() {
    connect(m_showLoadDiagnostics, &QCheckBox::toggled, this, [](bool value) { setPref(Preferences::ShowMapLoadDiagnostics, value); });
    connect(m_contextMenuIcons,&QCheckBox::toggled,this,[](bool value) { setPref(Preferences::ContextMenuIcons,value); });
    connect(m_contextMenuShortcuts,&QCheckBox::toggled,this,[](bool value) { setPref(Preferences::ContextMenuShortcuts,value); });
    connect(m_layoutCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::layoutChanged);
    connect(m_link2dCameras, &QCheckBox::stateChanged, this, &ViewPreferencePane::link2dCamerasChanged);
    connect(m_UIBrightnessSlider, &SliderWithLabel::valueChanged, this, &ViewPreferencePane::UIBrightnessChanged);
    connect(m_fovSlider, &SliderWithLabel::valueChanged, this, &ViewPreferencePane::fovChanged);
    connect(m_unitsDisplayType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::unitsDisplayTypeIndexChanged);
    connect(m_metricConversationFactor, &QLineEdit::textChanged, this, &ViewPreferencePane::metricConversationFactorChanged);
    connect(m_showAxes, &QCheckBox::stateChanged, this, &ViewPreferencePane::showAxesChanged);
    connect(m_enableMsaa, &QCheckBox::stateChanged, this, &ViewPreferencePane::enableMsaaChanged);
    connect(m_maxFPS, QOverload<int>::of(&QSpinBox::valueChanged), this, &ViewPreferencePane::maxFPSChanged);
    connect(m_showContextOverlay, &QCheckBox::stateChanged, this, &ViewPreferencePane::contextOverlayEnabledChanged);
    connect(m_contextOverlayTheme, &QLineEdit::editingFinished, this, &ViewPreferencePane::contextOverlayThemeChanged);
    connect(m_textureModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::textureModeChanged);
    connect(m_textureBrowserIconSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::textureBrowserIconSizeChanged);
    connect(m_autoBrightnessTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::editorFaceAutoBrightnessChanged);
    connect(m_rendererFontCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::renderFontFileChanged);
    connect(m_rendererFontSizeCombo, &QComboBox::currentTextChanged, this, &ViewPreferencePane::rendererFontSizeChanged);
    connect(m_UIFontCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::editorUIFontFileChanged);
    connect(m_UIFontSizeCombo, &QComboBox::currentTextChanged, this, &ViewPreferencePane::editorUIFontSizeChanged);
    connect(m_UIFontMinimumSize, QOverload<int>::of(&QSpinBox::valueChanged), this, &ViewPreferencePane::editorUIFontMinimumSizeChanged);
    connect(m_UIDetailTextOpacity, &SliderWithLabel::valueChanged, this, &ViewPreferencePane::editorUIDetailTextOpacityChanged);
    connect(m_listFontWeight, &QSpinBox::valueChanged, this, &ViewPreferencePane::editorUIListFontWeightChanged);
    connect(m_listOpticalSize, &QDoubleSpinBox::valueChanged, this, &ViewPreferencePane::editorUIListOpticalSizeChanged);
    connect(m_listTypoLineMetrics, &QCheckBox::toggled, this, &ViewPreferencePane::editorUIListTypoMetricsChanged);
    connect(m_listTextPosition, &QComboBox::currentIndexChanged, this, &ViewPreferencePane::editorUIListTextPositionChanged);
    connect(m_ConsoleFontCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::editorConsoleFontFileChanged);
    connect(m_ConsoleFontSizeCombo, &QComboBox::currentTextChanged, this, &ViewPreferencePane::editorConsoleFontSizeChanged);
    connect(m_ToolbarIconSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::editorToolbarIconSizeChanged);
}

void ViewPreferencePane::unBindEvents() {
    disconnect(m_showLoadDiagnostics, nullptr, this, nullptr);
    disconnect(m_contextMenuIcons,nullptr,this,nullptr);
    disconnect(m_contextMenuShortcuts,nullptr,this,nullptr);
    disconnect(m_layoutCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::layoutChanged);
    disconnect(m_link2dCameras, &QCheckBox::stateChanged, this, &ViewPreferencePane::link2dCamerasChanged);
    disconnect(m_UIBrightnessSlider, &SliderWithLabel::valueChanged, this, &ViewPreferencePane::UIBrightnessChanged);
    disconnect(m_fovSlider, &SliderWithLabel::valueChanged, this, &ViewPreferencePane::fovChanged);
    disconnect(m_unitsDisplayType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::unitsDisplayTypeIndexChanged);
    disconnect(m_metricConversationFactor, &QLineEdit::textChanged, this, &ViewPreferencePane::metricConversationFactorChanged);
    disconnect(m_showAxes, &QCheckBox::stateChanged, this, &ViewPreferencePane::showAxesChanged);
    disconnect(m_enableMsaa, &QCheckBox::stateChanged, this, &ViewPreferencePane::enableMsaaChanged);
    disconnect(m_maxFPS, QOverload<int>::of(&QSpinBox::valueChanged), this, &ViewPreferencePane::maxFPSChanged);
    disconnect(m_showContextOverlay, &QCheckBox::stateChanged, this, &ViewPreferencePane::contextOverlayEnabledChanged);
    disconnect(m_contextOverlayTheme, &QLineEdit::editingFinished, this, &ViewPreferencePane::contextOverlayThemeChanged);
    disconnect(m_textureModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::textureModeChanged);
    disconnect(m_textureBrowserIconSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::textureBrowserIconSizeChanged);
    disconnect(m_autoBrightnessTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::editorFaceAutoBrightnessChanged);
    disconnect(m_rendererFontCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::renderFontFileChanged);
    disconnect(m_rendererFontSizeCombo, &QComboBox::currentTextChanged, this, &ViewPreferencePane::rendererFontSizeChanged);
    disconnect(m_UIFontCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewPreferencePane::editorUIFontFileChanged);
    disconnect(m_UIFontSizeCombo, &QComboBox::currentTextChanged, this, &ViewPreferencePane::editorUIFontSizeChanged);
    disconnect(m_UIFontMinimumSize, QOverload<int>::of(&QSpinBox::valueChanged), this, &ViewPreferencePane::editorUIFontMinimumSizeChanged);
    disconnect(m_UIDetailTextOpacity, &SliderWithLabel::valueChanged, this, &ViewPreferencePane::editorUIDetailTextOpacityChanged);
    disconnect(m_listFontWeight, nullptr, this, nullptr);
    disconnect(m_listOpticalSize, nullptr, this, nullptr);
    disconnect(m_listTypoLineMetrics, nullptr, this, nullptr);
    disconnect(m_listTextPosition, nullptr, this, nullptr);
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
    prefs.resetToDefault(Preferences::ShowMapLoadDiagnostics);
    prefs.resetToDefault(Preferences::MapViewLayout);
    prefs.resetToDefault(Preferences::Link2DCameras);
    prefs.resetToDefault(Preferences::Brightness);
    prefs.resetToDefault(Preferences::RendererMaxFPS);
    prefs.resetToDefault(Preferences::ShowContextOverlay);
    prefs.resetToDefault(Preferences::ContextOverlayTheme);
    prefs.resetToDefault(Preferences::FaceAutoBrightness);
    prefs.resetToDefault(Preferences::CameraFov);
    prefs.resetToDefault(Preferences::LengthUnitSystem);
    prefs.resetToDefault(Preferences::MetricConversationFactor);
    prefs.resetToDefault(Preferences::ShowAxes);
    prefs.resetToDefault(Preferences::EnableMSAA);
    prefs.resetToDefault(Preferences::TextureMinFilter);
    prefs.resetToDefault(Preferences::TextureMagFilter);
    prefs.resetToDefault(Preferences::TextureBrowserIconSize);
    prefs.resetToDefault(Preferences::RendererFontSize);
    prefs.resetToDefault(Preferences::RendererFontPath);
    prefs.resetToDefault(Preferences::ConsoleFontPath);
    prefs.resetToDefault(Preferences::ConsoleFontSize);

    // Reset typography in the draft too; only Apply changes the global font.
    selectFont(*m_UIFontCombo, Preferences::UIFontPath.defaultValue());
    selectFontSize(*m_UIFontSizeCombo, Preferences::UIFontSize.defaultValue());
    m_UIFontMinimumSize->setValue(Preferences::UIFontMinimumSize.defaultValue());
    m_UIDetailTextOpacity->setValue(qRound(100 * Preferences::UIDetailTextOpacity.defaultValue()));
    m_listFontWeight->setValue(Preferences::UIListFontWeight.defaultValue());
    m_listOpticalSize->setValue(Preferences::UIListOpticalSize.defaultValue());
    m_listTypoLineMetrics->setChecked(Preferences::UIListTypoLineMetrics.defaultValue());
    m_listTextPosition->setCurrentIndex(Preferences::UIListSubpixelPositioning.defaultValue() ? 1 : 0);
    m_fontDraftDirty = true;
    updateFontPreviews();

    reloadUIStyle();
    bindEvents();
}

void ViewPreferencePane::doUpdateControls() {
    unBindEvents();

    m_showLoadDiagnostics->setChecked(pref(Preferences::ShowMapLoadDiagnostics));
    m_layoutCombo->setCurrentIndex(pref(Preferences::MapViewLayout));
    m_link2dCameras->setChecked(pref(Preferences::Link2DCameras));
    m_UIBrightnessSlider->setValue(brightnessToUI(pref(Preferences::UIBrightness)));
    m_contextMenuIcons->setChecked(pref(Preferences::ContextMenuIcons));
    m_contextMenuShortcuts->setChecked(pref(Preferences::ContextMenuShortcuts));
    m_fovSlider->setValue(int(pref(Preferences::CameraFov)));

    m_unitsDisplayType->setCurrentIndex(pref(Preferences::LengthUnitSystem));
    m_metricConversationFactor->setText(QString::asprintf("%.4f", pref(Preferences::MetricConversationFactor)));

    m_autoBrightnessTypeCombo->setCurrentIndex(pref(Preferences::FaceAutoBrightness));

    const auto textureModeIndex = findTextureMode(pref(Preferences::TextureMinFilter), pref(Preferences::TextureMagFilter));
    m_textureModeCombo->setCurrentIndex(int(textureModeIndex));

    m_showAxes->setChecked(pref(Preferences::ShowAxes));
    m_enableMsaa->setChecked(pref(Preferences::EnableMSAA));
    m_maxFPS->setValue(pref(Preferences::RendererMaxFPS));
    m_showContextOverlay->setChecked(pref(Preferences::ShowContextOverlay));
    m_contextOverlayTheme->setText(pref(Preferences::ContextOverlayTheme));
    // m_themeCombo->setCurrentIndex(findThemeIndex(pref(Preferences::Theme)));

    const auto textureBrowserIconSize = pref(Preferences::TextureBrowserIconSize);
    if (textureBrowserIconSize == 0.25f) {
        m_textureBrowserIconSizeCombo->setCurrentIndex(0);
    } else if (textureBrowserIconSize == 0.5f) {
        m_textureBrowserIconSizeCombo->setCurrentIndex(1);
    } else if (textureBrowserIconSize == 1.5f) {
        m_textureBrowserIconSizeCombo->setCurrentIndex(3);
    } else if (textureBrowserIconSize == 2.0f) {
        m_textureBrowserIconSizeCombo->setCurrentIndex(4);
    } else if (textureBrowserIconSize == 2.5f) {
        m_textureBrowserIconSizeCombo->setCurrentIndex(5);
    } else if (textureBrowserIconSize == 3.0f) {
        m_textureBrowserIconSizeCombo->setCurrentIndex(6);
    } else {
        m_textureBrowserIconSizeCombo->setCurrentIndex(2);
    }

    auto iconSize = (pref(Preferences::ToolBarIconsSize) - 14) / 2;
    m_ToolbarIconSizeCombo->setCurrentIndex(iconSize);

    selectFontSize(*m_rendererFontSizeCombo, pref(Preferences::RendererFontSize));
    selectFontSize(*m_ConsoleFontSizeCombo, pref(Preferences::ConsoleFontSize));
    selectFont(*m_rendererFontCombo, pref(Preferences::RendererFontPath));
    selectFont(*m_ConsoleFontCombo, pref(Preferences::ConsoleFontPath));
    if (!m_fontDraftDirty) {
        selectFont(*m_UIFontCombo, pref(Preferences::UIFontPath));
        selectFontSize(*m_UIFontSizeCombo, pref(Preferences::UIFontSize));
        const QSignalBlocker minimumBlocker(m_UIFontMinimumSize);
        const QSignalBlocker opacityBlocker(m_UIDetailTextOpacity);
        m_UIFontMinimumSize->setValue(pref(Preferences::UIFontMinimumSize));
        m_UIDetailTextOpacity->setValue(qRound(std::clamp(pref(Preferences::UIDetailTextOpacity), 0.0f, 1.0f) * 100.0f));
        const auto settings = listFontSettings();
        m_listFontWeight->setValue(settings.weight);
        m_listOpticalSize->setValue(settings.opticalSize);
        m_listTypoLineMetrics->setChecked(settings.typoLineMetrics);
        m_listTextPosition->setCurrentIndex(settings.subpixelPositioning ? 1 : 0);
    }
    updateFontPreviews();

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

void ViewPreferencePane::UIBrightnessChanged(const int value) {
    auto &prefs = PreferenceManager::instance();
    prefs.set(Preferences::UIBrightness, brightnessFromUI(value));
    // Also coalesce wheel/key repeats. Never repolish the whole application
    // synchronously from the slider's valueChanged handler.
    TrenchBroomApp::instance().requestThemeRefresh(75);
}

void ViewPreferencePane::fovChanged(const int value) {
    auto &prefs = PreferenceManager::instance();
    prefs.set(Preferences::CameraFov, float(value));
}

void ViewPreferencePane::unitsDisplayTypeIndexChanged(const int index) {
    auto &prefs = PreferenceManager::instance();
    prefs.set(Preferences::LengthUnitSystem, index);
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

void ViewPreferencePane::maxFPSChanged(const int value) {
    setPref(Preferences::RendererMaxFPS, value);
}

void ViewPreferencePane::contextOverlayEnabledChanged(const int state) {
    PreferenceManager::instance().set(Preferences::ShowContextOverlay, state == Qt::Checked);
}

void ViewPreferencePane::contextOverlayThemeChanged() {
    PreferenceManager::instance().set(Preferences::ContextOverlayTheme, m_contextOverlayTheme->text().trimmed());
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
    if (index < 0) return;
    const auto path = IO::pathFromQString(m_rendererFontCombo->itemData(index).toString());
    auto &prefs = PreferenceManager::instance();

    if (path == "[default]") {
        prefs.resetToDefault(Preferences::RendererFontPath);
        prefs.resetToDefault(Preferences::RendererFontSize);
    } else {
        prefs.set(Preferences::RendererFontPath, path);
    }
    selectFont(*m_rendererFontCombo, pref(Preferences::RendererFontPath));
    selectFontSize(*m_rendererFontSizeCombo, pref(Preferences::RendererFontSize));
}

void ViewPreferencePane::reloadUIStyle() {
    // Apply once after the current control/popup has finished processing input.
    // The app reloads only font faces/sizes that actually changed.
    if (auto* app = dynamic_cast<TrenchBroomApp*>(qApp)) app->requestThemeRefresh(30);
}

void ViewPreferencePane::updateFontPreviews() {
    const auto size = std::clamp(m_UIFontSizeCombo->currentText().toInt(), 1, 96);
    const auto minimum = m_UIFontMinimumSize->value();
    const auto opacity = m_UIDetailTextOpacity->value() / 100.0;
    const ListFontSettings settings{m_listFontWeight->value(), m_listOpticalSize->value(),
        m_listTypoLineMetrics->isChecked(), m_listTextPosition->currentIndex() == 1};
    m_systemFontPreview->refresh("[system]", size, minimum, opacity, settings);
    m_selectedFontPreview->refresh(IO::pathFromQString(m_UIFontCombo->currentData().toString()),
        size, minimum, opacity, settings);
    m_systemFontInfo->setText(m_systemFontPreview->description());
    m_selectedFontInfo->setText(m_selectedFontPreview->description());
    const auto title = tr("Selected font: %1").arg(m_selectedFontPreview->family());
    m_selectedFontHeading->setText(title);
    m_selectedFontHeading->setWordWrap(true);
    m_selectedFontPreview->setAccessibleName(title);
    m_applyFont->setEnabled(m_fontDraftDirty || !pref(Preferences::UIGlobalTypography));
}

void ViewPreferencePane::editorUIListFontWeightChanged(const int) {
    m_fontDraftDirty = true;
    updateFontPreviews();
}

void ViewPreferencePane::editorUIListOpticalSizeChanged(const double) {
    m_fontDraftDirty = true;
    updateFontPreviews();
}

void ViewPreferencePane::editorUIListTypoMetricsChanged(const bool) {
    m_fontDraftDirty = true;
    updateFontPreviews();
}

void ViewPreferencePane::editorUIListTextPositionChanged(const int) {
    m_fontDraftDirty = true;
    updateFontPreviews();
}

void ViewPreferencePane::applyFontDraft() {
    m_UIFontMinimumSize->interpretText();
    m_listFontWeight->interpretText();
    m_listOpticalSize->interpretText();
    auto& prefs = PreferenceManager::instance();
    prefs.set(Preferences::UIFontPath, IO::pathFromQString(m_UIFontCombo->currentData().toString()));
    prefs.set(Preferences::UIFontSize, std::clamp(m_UIFontSizeCombo->currentText().toInt(), 1, 96));
    prefs.set(Preferences::UIFontMinimumSize, m_UIFontMinimumSize->value());
    prefs.set(Preferences::UIDetailTextOpacity, float(m_UIDetailTextOpacity->value()) / 100.0f);
    prefs.set(Preferences::UIListFontWeight, m_listFontWeight->value());
    prefs.set(Preferences::UIListOpticalSize, float(m_listOpticalSize->value()));
    prefs.set(Preferences::UIListTypoLineMetrics, m_listTypoLineMetrics->isChecked());
    prefs.set(Preferences::UIListSubpixelPositioning, m_listTextPosition->currentIndex() == 1);
    prefs.set(Preferences::UIGlobalTypography, true);
    prefs.saveChanges();
    m_fontDraftDirty = false;
    reloadUIStyle();
    updateFontPreviews();
}

void ViewPreferencePane::editorUIFontSizeChanged(const QString &text) {
    bool ok;
    text.toInt(&ok);
    if (!ok) return;
    m_fontDraftDirty = true;
    updateFontPreviews();
}

void ViewPreferencePane::editorUIFontFileChanged(const int index) {
    if (index < 0) return;
    auto path = IO::pathFromQString(m_UIFontCombo->itemData(index).toString());
    if (path == "[default]") {
        selectFontSize(*m_UIFontSizeCombo, Preferences::UIFontSize.defaultValue());
        path = Preferences::UIFontPath.defaultValue();
    }
    selectFont(*m_UIFontCombo, path);
    m_fontDraftDirty = true;
    updateFontPreviews();
}

void ViewPreferencePane::editorUIFontMinimumSizeChanged(const int) {
    m_fontDraftDirty = true;
    updateFontPreviews();
}

void ViewPreferencePane::editorUIDetailTextOpacityChanged(const int) {
    m_fontDraftDirty = true;
    updateFontPreviews();
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
    if (index < 0) return;
    const auto path = IO::pathFromQString(m_ConsoleFontCombo->itemData(index).toString());
    auto &prefs = PreferenceManager::instance();

    if (path == "[default]") {
        prefs.resetToDefault(Preferences::ConsoleFontPath);
        prefs.resetToDefault(Preferences::ConsoleFontSize);
    } else {
        prefs.set(Preferences::ConsoleFontPath, path);
    }
    selectFont(*m_ConsoleFontCombo, pref(Preferences::ConsoleFontPath));
    selectFontSize(*m_ConsoleFontSizeCombo, pref(Preferences::ConsoleFontSize));
    reloadUIStyle();
}

void ViewPreferencePane::editorToolbarIconSizeChanged(int index) {
    auto &prefs = PreferenceManager::instance();

    // start at font-size 14px
    auto size = 14 + index * 2;
    prefs.set(Preferences::ToolBarIconsSize, size);

    if (TrenchBroomApp::instance().getCurrentMapFrame()) {
        TrenchBroomApp::instance().getCurrentMapFrame()->updateToolbar();
    }

    reloadUIStyle();
}

void ViewPreferencePane::editorFaceAutoBrightnessChanged(int value) {
    auto &prefs = PreferenceManager::instance();
    prefs.set(Preferences::FaceAutoBrightness, value);
}

} // namespace TrenchBroom::View
