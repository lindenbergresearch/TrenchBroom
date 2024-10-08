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

#pragma once

#include "View/PreferencePane.h"

class QCheckBox;

class QComboBox;

namespace TrenchBroom::View {
class SliderWithLabel;

class ViewPreferencePane : public PreferencePane {
  Q_OBJECT
  private:
    QComboBox *m_layoutCombo = nullptr;
    QCheckBox *m_link2dCameras = nullptr;
    SliderWithLabel *m_UIBrightnessSlider = nullptr;
    SliderWithLabel *m_gridAlphaSlider = nullptr;
    SliderWithLabel *m_fovSlider = nullptr;
    SliderWithLabel *m_gridWidthSlider = nullptr;
    QComboBox *m_unitsDisplayType = nullptr;
    QComboBox *m_unitsGridType = nullptr;
    QComboBox *m_autoBrightnessTypeCombo = nullptr;
    QLineEdit *m_metricConversationFactor = nullptr;
    QCheckBox *m_showAxes = nullptr;
    QComboBox *m_textureModeCombo = nullptr;
    QCheckBox *m_enableMsaa = nullptr;
    QComboBox *m_textureBrowserIconSizeCombo = nullptr;
    QComboBox *m_rendererFontSizeCombo = nullptr;
    QComboBox *m_rendererFontCombo = nullptr;
    QComboBox *m_UIFontSizeCombo = nullptr;
    QComboBox *m_UIFontCombo = nullptr;
    QComboBox *m_ConsoleFontSizeCombo = nullptr;
    QComboBox *m_ConsoleFontCombo = nullptr;
    QComboBox *m_ToolbarIconSizeCombo = nullptr;

    std::vector<std::filesystem::path> font_files;

  public:
    explicit ViewPreferencePane(QWidget *parent = nullptr);

  private:
    void createGui();

    void reloadUIStyle(bool reloadFonts = true);

    QWidget *createViewPreferences();

    void bindEvents();

    void unBindEvents();

    bool doCanResetToDefaults() override;

    void doResetToDefaults() override;

    void doUpdateControls() override;

    bool doValidate() override;

    size_t findTextureMode(int minFilter, int magFilter) const;

  private slots:

    void layoutChanged(int index);

    void link2dCamerasChanged(int state);

    void UIBrightnessChanged(int value);

    void editorFaceAutoBrightnessChanged(int value);

    void editorGridWithChanged(int value);

    void gridAlphaChanged(int value);

    void fovChanged(int value);

    void showAxesChanged(int state);

    void unitsDisplayTypeIndexChanged(int index);

    void unitsGridTypeIndexChanged(int index);

    void metricConversationFactorChanged(const QString &text);

    void enableMsaaChanged(int state);

    void textureModeChanged(int index);

    void textureBrowserIconSizeChanged(int index);

    void rendererFontSizeChanged(const QString &text);

    void renderFontFileChanged(int index);

    void editorUIFontSizeChanged(const QString &text);

    void editorUIFontFileChanged(int index);

    void editorConsoleFontSizeChanged(const QString &text);

    void editorConsoleFontFileChanged(int index);

    void editorToolbarIconSizeChanged(int index);
};
} // namespace TrenchBroom::View
