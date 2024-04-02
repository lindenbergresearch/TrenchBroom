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
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#include "PreferenceDialog.h"

#include <QBoxLayout>
#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QToolBar>
#include <QToolButton>

#include "IO/ResourceUtils.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include "TrenchBroomApp.h"
#include "View/BorderLine.h"
#include "View/ColorsPreferencePane.h"
#include "View/GamesPreferencePane.h"
#include "View/KeyboardPreferencePane.h"
#include "View/MousePreferencePane.h"
#include "View/PreferencePane.h"
#include "View/PreferencesPreferencePane.h"
#include "View/QtUtils.h"
#include "View/ViewPreferencePane.h"

#include <filesystem>

namespace TrenchBroom {
namespace View {
const QString PreferenceDialog::WINDOW_TITLE = "Editor Preferences";
const QSize PreferenceDialog::ICON_SIZE = QSize{32, 32};
const int PreferenceDialog::ICON_WIDTH = int(float(ICON_SIZE.width()) * 2.5f);

PreferenceDialog::PreferenceDialog(std::shared_ptr<MapDocument> document, QWidget *parent) : QDialog(parent), m_document(std::move(document)), m_toolBar(nullptr), m_stackedWidget(nullptr), m_buttonBox(nullptr) {
    setWindowTitle(WINDOW_TITLE);
    setWindowIconTB(this);
    createGui();
    switchToPane(PrefPane_First);
    currentPane()->updateControls();
}

void PreferenceDialog::closeEvent(QCloseEvent *event) {
    if (!currentPane()->validate()) {
        event->ignore();
        return;
    }

    auto &prefs = PreferenceManager::instance();
    if (!prefs.saveInstantly()) {
        prefs.discardChanges();
    }

    event->accept();
}

void PreferenceDialog::createGui() {
    const auto gamesImage = IO::loadSVGIcon("GeneralPreferences.svg", ICON_SIZE.width());
    const auto viewImage = IO::loadSVGIcon("ViewPreferences.svg", ICON_SIZE.width());
    const auto colorsImage = IO::loadSVGIcon("ColorPreferences.svg", ICON_SIZE.width());
    const auto mouseImage = IO::loadSVGIcon("MousePreferences.svg", ICON_SIZE.width());
    const auto keyboardImage = IO::loadSVGIcon("KeyboardPreferences.svg", ICON_SIZE.width());
    const auto preferencesImage = IO::loadSVGIcon("AdvancedPreferences.svg", ICON_SIZE.width());

    m_toolBar = new QToolBar();
    m_toolBar->setObjectName("ToolBar_PreferenceDialog");
    m_toolBar->setFloatable(false);
    m_toolBar->setMovable(false);
    m_toolBar->setIconSize(ICON_SIZE);
    m_toolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    makeEmphasized(m_toolBar);

    // store actions
    m_toolButtonActions["Games"] = m_toolBar->addAction(
        gamesImage, "Games", [this]() {
            switchToPane(PrefPane_Games);
            highlightToolButton("Games");
        });

    m_toolButtonActions["View"] = m_toolBar->addAction(
        viewImage, "View", [this]() {
            switchToPane(PrefPane_View);
            highlightToolButton("View");
        });
    m_toolButtonActions["Colors"] = m_toolBar->addAction(
        colorsImage, "Colors", [this]() {
            switchToPane(PrefPane_Colors);
            highlightToolButton("Colors");
        });
    m_toolButtonActions["Mouse"] = m_toolBar->addAction(
        mouseImage, "Mouse", [this]() {
            switchToPane(PrefPane_Mouse);
            highlightToolButton("Mouse");
        });
    m_toolButtonActions["Keyboard"] = m_toolBar->addAction(
        keyboardImage, "Keyboard", [this]() {
            switchToPane(PrefPane_Keyboard);
            highlightToolButton("Keyboard");
        });

    m_toolButtonActions["Advanced"] = m_toolBar->addAction(
        preferencesImage, "Advanced", [this]() {
            switchToPane(PrefPane_Advanced);
            highlightToolButton("Advanced");
        });

    // Don't display tooltips for pane switcher buttons...
    for (auto *button: m_toolBar->findChildren<QToolButton *>()) {
        button->installEventFilter(this);
        button->setCheckable(true);
        button->setMinimumWidth(ICON_WIDTH);
        button->setObjectName("ToolButton_PreferenceDialog");
        makeEmphasized(makeSubTitle(button));
    }

    m_stackedWidget = new QStackedWidget();
    m_stackedWidget->addWidget(new GamesPreferencePane(m_document.get()));
    m_stackedWidget->addWidget(new ViewPreferencePane());
    m_stackedWidget->addWidget(new ColorsPreferencePane());
    m_stackedWidget->addWidget(new MousePreferencePane());
    m_stackedWidget->addWidget(new KeyboardPreferencePane(m_document.get()));
    m_stackedWidget->addWidget(new PreferencesPreferencePane());

    highlightToolButton("Games");

    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::RestoreDefaults
#if !defined __APPLE__
            | QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel
#endif
        ,
        this);

    auto *resetButton = m_buttonBox->button(QDialogButtonBox::RestoreDefaults);
    connect(resetButton, &QPushButton::clicked, this, &PreferenceDialog::resetToDefaults);

#if !defined __APPLE__
    connect(
        m_buttonBox->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, [this]() {
            auto &prefs = PreferenceManager::instance();
            prefs.saveChanges();
            this->close();
        });
    connect(
        m_buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this]() {
            auto &prefs = PreferenceManager::instance();
            prefs.saveChanges();
        });
    connect(
        m_buttonBox->button(QDialogButtonBox::Cancel), &QPushButton::clicked, this, [this]() {
            this->close();
        });
#endif

    auto *layout = new QVBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setAlignment(layout, Qt::AlignVCenter);
    layout->setSpacing(0);

    layout->setMenuBar(m_toolBar);
    layout->addWidget(m_stackedWidget, 1);
    layout->addLayout(wrapDialogButtonBox(m_buttonBox));

    setLayout(layout);
}

void PreferenceDialog::switchToPane(const PrefPane pane) {
    if (!currentPane()->validate()) {
        return;
    }

    m_stackedWidget->setCurrentIndex(pane);
    currentPane()->updateControls();

    auto *resetButton = m_buttonBox->button(QDialogButtonBox::RestoreDefaults);
    resetButton->setEnabled(currentPane()->canResetToDefaults());
}

PreferencePane *PreferenceDialog::currentPane() const {
    return static_cast<PreferencePane *>(m_stackedWidget->currentWidget());
}

void PreferenceDialog::resetToDefaults() {
    currentPane()->resetToDefaults();
}

// Don't display tooltips for pane switcher buttons...
bool PreferenceDialog::eventFilter(QObject *o, QEvent *e) {
    if (e->type() == QEvent::ToolTip) {
        return true;
    }

    return QDialog::eventFilter(o, e);
}

void PreferenceDialog::highlightToolButton(QString buttonName, bool highlighted) {
    auto palette = QPalette{};

    // reset
    for (std::pair<const QString, QAction *> item: m_toolButtonActions) {
        //  palette.setColor(QPalette::Active, QPalette::ButtonText,
        //  toQColor(pref(Preferences::UITextColor)));
        QToolButton *toolButton = dynamic_cast<QToolButton *>(m_toolBar->widgetForAction(item.second));

        if (toolButton) {
            toolButton->setChecked(false);
        }
    }

    QToolButton *toolButton = dynamic_cast<QToolButton *>(
        m_toolBar->widgetForAction(m_toolButtonActions[buttonName]));
    if (toolButton) {
        if (highlighted) {
            toolButton->setChecked(true);
            setWindowTitle(WINDOW_TITLE + " - " + buttonName);
        }
    }
}
}// namespace View
}// namespace TrenchBroom
