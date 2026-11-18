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

#include "View/Theme/QtUtils.h"
#include "View/Preferences/FontUtils.h"
#include "Preferences.h"
#include "PreferenceManager.h"
#include <QAction>
#include <QMenu>
#include <QStyle>

#include <QApplication>
#include <QBoxLayout>
#include <QButtonGroup>
#include <QColor>
#include <QDialog>
#include <QDir>
#include <QFont>
#include <QHeaderView>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QResizeEvent>
#include <QScreen>
#include <QSettings>
#include <QString>
#include <QStringBuilder>
#include <QTableView>
#include <QTextCodec>
#include <QToolBar>
#include <QToolButton>
#include <QWindow>
#include <QtGlobal>

#include "Color.h"
#include "Ensure.h"
#include "IO/ResourceUtils.h"
#include "Macros.h"
#include "View/Widgets/BorderLine.h"
#include "View/App/MapFrame.h"
#include "View/Document/MapTextEncoding.h"
#include "View/Theme/ViewConstants.h"

#include <PreferenceManager.h>
#include <Preferences.h>
#include <TrenchBroomApp.h>


// QDesktopWidget was deprecated in Qt 5.10 and we should use QGuiApplication::screenAt
// in 5.10 and above Used in centerOnScreen
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)

#include <QGuiApplication>

#else
#include <QApplication>
#include <QDesktopWidget>
#endif

namespace TrenchBroom {namespace View {
    void prepareContextMenu(QMenu& menu) {
        if (menu.property("tbMainMenu").toBool()) return;
        const auto icons = pref(Preferences::ContextMenuIcons), shortcuts = pref(Preferences::ContextMenuShortcuts);
        for (auto* action : menu.actions()) {
            if (action->property("tbRegisteredAction").toBool() && action->parent() != &menu) {
                auto* copy = new QAction(&menu);
                const auto source = QPointer<QAction>(action);
                const auto sync = [source,copy] {
                    if (!source) { copy->setEnabled(false); return; }
                    auto label = source->text();
                    if (!source->shortcut().isEmpty()) label += '\t' + source->shortcut().toString(QKeySequence::NativeText);
                    copy->setProperty("tbContextFullLabel",label);
                    const auto displayed = pref(Preferences::ContextMenuShortcuts) ? label : label.section('\t',0,0);
                    copy->setProperty("tbContextDisplayedLabel",displayed);
                    copy->setText(displayed); copy->setIcon(source->icon());
                    copy->setEnabled(source->isEnabled()); copy->setVisible(source->isVisible());
                    copy->setCheckable(source->isCheckable()); copy->setChecked(source->isChecked());
                    copy->setStatusTip(source->statusTip()); copy->setToolTip(source->toolTip());
                    // The source remains the sole owner of the key binding.
                    // This menu item only presents its current native key text.
                };
                sync();
                QObject::connect(action,&QAction::changed,copy,sync);
                QObject::connect(action,&QObject::destroyed,copy,[copy] { copy->setEnabled(false); });
                QObject::connect(copy,&QAction::triggered,copy,[source] { if (source) source->trigger(); });
                menu.insertAction(action,copy); menu.removeAction(action); action = copy;
            }
            action->setIconVisibleInMenu(icons);
            action->setShortcutVisibleInContextMenu(shortcuts);
            // Qt's standard text-edit menus sometimes encode the shortcut in
            // the label instead of QAction::shortcut(). Preserve it for reuse.
            auto label = action->text();
            if (label == action->property("tbContextDisplayedLabel").toString()) label = action->property("tbContextFullLabel").toString();
            action->setProperty("tbContextFullLabel",label);
            const auto displayed = shortcuts ? label : label.section('\t',0,0);
            action->setProperty("tbContextDisplayedLabel",displayed);
            action->setText(displayed);
        }
    }

    namespace {
        QWidget* styledWidget(QWidget* widget) {
            if (!widget->property("uiStyled").toBool()) {
                widget->setProperty("uiStyled", true);
                widget->setProperty("uiTextRole", "body");
            }
            return widget;
        }
    }

    SyncHeightEventFilter::SyncHeightEventFilter(QWidget* primary, QWidget* secondary, QObject* parent)
        : QObject{
              parent
          },
          m_primary{primary},
          m_secondary{secondary} {
        ensure(m_primary != nullptr, "primary is not null");
        ensure(m_secondary != nullptr, "secondary is not null");

        m_primary->installEventFilter(this);
    }

    SyncHeightEventFilter::~SyncHeightEventFilter() {
        if (m_primary) {
            m_primary->removeEventFilter(this);
        }
    }

    bool SyncHeightEventFilter::eventFilter(QObject* target, QEvent* event) {
        if (target == m_primary && event->type() == QEvent::Resize) {
            const auto* sizeEvent = static_cast<QResizeEvent *>(event);
            const auto height = sizeEvent->size().height();

            if (m_secondary->height() != height) {
                m_secondary->setFixedHeight(height);
            }

            return false;
        }
        else {
            return QObject::eventFilter(target, event);
        }
    }

    static QString fileDialogDirToString(const FileDialogDir dir) {
        switch (dir) {
            case FileDialogDir::Map:
                return "Map";
            case FileDialogDir::TextureCollection:
                return "TextureCollection";
            case FileDialogDir::CompileTool:
                return "CompileTool";
            case FileDialogDir::Engine:
                return "Engine";
            case FileDialogDir::EntityDefinition:
                return "EntityDefinition";
            case FileDialogDir::GamePath:
                return "GamePath";
            case FileDialogDir::Resources:
                return "Resources";
                switchDefault();
        }
    }

    static QString fileDialogDefaultDirectorySettingsPath(const FileDialogDir dir) {
        return QString::fromLatin1("FileDialog/%1/DefaultDirectory").arg(fileDialogDirToString(dir));
    }

    QString fileDialogDefaultDirectory(const FileDialogDir dir) {
        const auto key = fileDialogDefaultDirectorySettingsPath(dir);

        const auto settings = QSettings{};
        const auto defaultDir = settings.value(key).toString();
        return defaultDir;
    }

    void updateFileDialogDefaultDirectoryWithFilename(FileDialogDir type, const QString& filename) {
        const auto dirQDir = QFileInfo(filename).absoluteDir();
        const auto dirString = dirQDir.absolutePath();
        updateFileDialogDefaultDirectoryWithDirectory(type, dirString);
    }

    void updateFileDialogDefaultDirectoryWithDirectory(FileDialogDir type, const QString& newDefaultDirectory) {
        const auto key = fileDialogDefaultDirectorySettingsPath(type);

        auto settings = QSettings{};
        settings.setValue(key, newDefaultDirectory);
    }

    QString windowSettingsPath(const QWidget* window, const QString& suffix) {
        ensure(window != nullptr, "window must not be null");
        ensure(!window->objectName().isEmpty(), "window name must not be empty");

        return "Windows/" + window->objectName() + "/" + suffix;
    }

    void saveWindowGeometry(QWidget* window) {
        ensure(window != nullptr, "window must not be null");

        const auto path = windowSettingsPath(window, "Geometry");
        auto settings = QSettings{};
        settings.setValue(path, window->saveGeometry());
    }

    void restoreWindowGeometry(QWidget* window) {
        ensure(window != nullptr, "window must not be null");

        const auto path = windowSettingsPath(window, "Geometry");
        auto settings = QSettings{};
        window->restoreGeometry(settings.value(path).toByteArray());
    }

    bool widgetOrChildHasFocus(const QWidget* widget) {
        ensure(widget != nullptr, "widget must not be null");

        const auto* currentWidget = static_cast<QObject *>(QApplication::focusWidget());
        while (currentWidget != nullptr) {
            if (currentWidget == widget) {
                return true;
            }
            currentWidget = currentWidget->parent();
        }
        return false;
    }

    MapFrame* findMapFrame(QWidget* widget) {
        for (auto* ancestor = widget; ancestor; ancestor = ancestor->parentWidget()) {
            if (auto* frame = qobject_cast<MapFrame*>(ancestor)) return frame;
        }
        return nullptr;
    }

    void setHint(QLineEdit* ctrl, const char* hint) {
        ctrl->setPlaceholderText(hint);
    }

    void centerOnScreen(QWidget* window) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
        const auto* screen = QGuiApplication::screenAt(window->mapToGlobal({window->width() / 2, 0}));
        if (screen == nullptr) {
            return;
        }
        const auto screenGeometry = screen->availableGeometry();
#else
        const auto screenGeometry = QApplication::desktop()->availableGeometry(window);
#endif
        window->setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, window->size(), screenGeometry));
    }

    int getCommonFieldHeight() {
        auto fontsize = uiFontPixelSize();
        return fontsize + 2 * LayoutConstants::MediumVMargin;
    }

    void repolishWidget(QWidget* widget) {
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
        widget->updateGeometry();
        widget->update();
    }

    QWidget* setTextStyleRole(QWidget* widget, const QString& role) {
        styledWidget(widget)->setProperty("uiTextRole", role);
        repolishWidget(widget);
        return widget;
    }

    QWidget* makeDefault(QWidget* widget, bool resetFont) {
        styledWidget(widget)->setProperty("uiTone", "normal");
        if (resetFont) {
            widget->setProperty("uiTextRole", "body");
            widget->setProperty("uiFontFamily", "ui");
            widget->setProperty("uiEmphasized", QVariant{});
            widget->setProperty("uiItalic", QVariant{});
        }
        repolishWidget(widget);
        return widget;
    }

    QWidget* makeEmphasized(QWidget* widget) {
        styledWidget(widget)->setProperty("uiEmphasized", true);
        repolishWidget(widget);
        return widget;
    }

    QWidget* makeItalic(QWidget* widget) {
        styledWidget(widget)->setProperty("uiItalic", true);
        repolishWidget(widget);
        return widget;
    }

    QWidget* makeUnemphasized(QWidget* widget) {
        styledWidget(widget)->setProperty("uiEmphasized", false);
        repolishWidget(widget);
        return widget;
    }

    QWidget* makeInfo(QWidget* widget) { return setTextStyleRole(widget, "info"); }
    QWidget* makeSmall(QWidget* widget) { return setTextStyleRole(widget, "small"); }
    QWidget* makeTitle(QWidget* widget) {
        makeEmphasized(widget);
        return setTextStyleRole(widget, "title");
    }
    QWidget* makeSubTitle(QWidget* widget) { return setTextStyleRole(widget, "subtitle"); }
    QWidget* makeHeader(QWidget* widget) {
        makeEmphasized(widget);
        return setTextStyleRole(widget, "header");
    }

    QWidget* makeError(QWidget* widget) {
        styledWidget(widget)->setProperty("uiTone", "error");
        repolishWidget(widget);
        return widget;
    }

    QWidget* makeMono(QWidget* widget) {
        styledWidget(widget)->setProperty("uiFontFamily", "console");
        repolishWidget(widget);
        return widget;
    }

    QWidget* makeSelected(QWidget* widget) {
        styledWidget(widget)->setProperty("uiTone", "highlighted");
        repolishWidget(widget);
        return widget;
    }

    QWidget* makeBright(QWidget* widget) {
        styledWidget(widget)->setProperty("uiTone", "highlighted");
        repolishWidget(widget);
        return widget;
    }

    QWidget* makeUnselected(QWidget* widget) { return makeDefault(widget, false); }

    Color fromQColor(const QColor& color) {
        return Color(
            static_cast<float>(color.redF()), static_cast<float>(color.greenF()), static_cast<float>(color.blueF()),
            static_cast<float>(color.alphaF())
        );
    }

    QColor toQColor(const Color& color, const float multiplier) {
        // return QColor::fromRgb(int(color.r() * 255.0f * multiplier), int(color.g() * 255.0f *
        // multiplier), int(color.b() * 255.0f * multiplier), int(color.a() * 255.0f));
        return QColor::fromRgbF(color.r() * multiplier, color.g() * multiplier, color.b() * multiplier, color.a());
    }

    float getQColorBrightnessFactor(const QColor& color) {
        return float(std::max(std::max(color.redF(), color.greenF()), color.blueF()) * color.alphaF());
    }

    QString toStyleSheetColor(const char* prefix, const QColor& color) {
        auto sheet = QString::asprintf("%s: rgba(%d, %d, %d, %d); ", prefix, color.red(), color.green(), color.blue(), color.alpha());
        return sheet;
    }

    QString toStyleSheetRGBA(const QColor& color, int adjustment) {
        auto tmp_color = adjustment < 0 ? color.darker(-adjustment) : color.lighter(adjustment);
        return QString::asprintf("rgba(%d, %d, %d, %d)", tmp_color.red(), tmp_color.green(), tmp_color.blue(), tmp_color.alpha());
    }

    QString toStyleSheetRGBA(const QPalette& palette, QPalette::ColorRole role, QPalette::ColorGroup group, int adjustment) {
        auto color = palette.color(group, role);
        return toStyleSheetRGBA(color, adjustment);
    }

    QString toStyleSheetRGBA(const QPalette& palette, QPalette::ColorRole role, int adjustment) {
        auto color = palette.color(QPalette::ColorGroup::Active, role);
        return toStyleSheetRGBA(color, adjustment);
    }

    QToolBar* createInspectorToolBar(QWidget* parent) {
        auto* toolbar = new QToolBar(parent);
        toolbar->setObjectName("InspectorToolToolbar");
        toolbar->setMovable(false);
        toolbar->setFloatable(false);
        return toolbar;
    }

    QToolButton* createBitmapButton(const std::string& image, const QString& tooltip, QWidget* parent) {
        return createBitmapButton(IO::loadSVGIcon(image, 16), tooltip, parent);
    }

    QToolButton* createBitmapButton(const QIcon& icon, const QString& tooltip, QWidget* parent) {
        auto* button = new QToolButton{parent};
        // Vector icons have no finite list of raster sizes. Widget dimensions are
        // logical pixels; the icon engine handles the output device's density.
        button->setIconSize(QSize(16, 16));
        button->setMinimumSize(button->iconSize());
        button->setToolTip(tooltip);
        button->setIcon(icon);
        button->setObjectName("toolButton_borderless");

        return button;
    }

    QToolButton* createBitmapToggleButton(const std::string& image, const QString& tooltip, QWidget* parent) {
        auto* button = createBitmapButton(image, tooltip, parent);
        button->setCheckable(true);
        return button;
    }

    QWidget* createDefaultPage(const QString& message, QWidget* parent) {
        auto* container = new QWidget{parent};
        auto* layout = new QVBoxLayout{};

        auto* messageLabel = new QLabel{message};
        makeEmphasized(messageLabel);
        layout->addWidget(messageLabel, 0, Qt::AlignHCenter | Qt::AlignTop);
        container->setLayout(layout);

        return container;
    }

    QSlider* createSlider(const int min, const int max) {
        auto* slider = new QSlider{};
        slider->setMinimum(min);
        slider->setMaximum(max);
        slider->setTickPosition(QSlider::NoTicks);
        slider->setTracking(true);
        slider->setOrientation(Qt::Horizontal);
        makeBright(slider);

        return slider;
    }

    float getSliderRatio(const QSlider* slider) {
        return float(slider->value() - slider->minimum()) / float(slider->maximum() - slider->minimum());
    }

    void setSliderRatio(QSlider* slider, float ratio) {
        const auto value = ratio * float(slider->maximum() - slider->minimum()) + float(slider->minimum());
        slider->setValue(int(value));
    }

    float getSliderRange(QSlider* slider, float length, float offset) {
        auto ratio = getSliderRatio(slider);
        return ratio * length + offset;
    }

    void setSliderRange(QSlider* slider, float length, float value, float offset) {
        auto ratio = (value - offset) / length;
        setSliderRatio(slider, ratio);
    }

    QLayout* wrapDialogButtonBox(QWidget* buttonBox) {
        auto* innerLayout = new QHBoxLayout{};
        innerLayout->setContentsMargins(
            LayoutConstants::DialogButtonLeftMargin, LayoutConstants::DialogButtonTopMargin, LayoutConstants::DialogButtonRightMargin,
            LayoutConstants::DialogButtonBottomMargin
        );
        innerLayout->setSpacing(0);
        innerLayout->addWidget(buttonBox);

        auto* outerLayout = new QVBoxLayout{};
        outerLayout->setContentsMargins(QMargins{});
        outerLayout->setSpacing(0);
        outerLayout->addWidget(new BorderLine{BorderLine::Direction::Horizontal});
        outerLayout->addLayout(innerLayout);

        return outerLayout;
    }

    QLayout* wrapDialogButtonBox(QLayout* buttonBox) {
        auto* innerLayout = new QHBoxLayout{};
        innerLayout->setContentsMargins(
            LayoutConstants::DialogButtonLeftMargin, LayoutConstants::DialogButtonTopMargin, LayoutConstants::DialogButtonRightMargin,
            LayoutConstants::DialogButtonBottomMargin
        );
        innerLayout->setSpacing(0);
        innerLayout->addLayout(buttonBox);

        auto* outerLayout = new QVBoxLayout{};
        outerLayout->setContentsMargins(QMargins{});
        outerLayout->setSpacing(0);
        outerLayout->addWidget(new BorderLine{BorderLine::Direction::Horizontal});
        outerLayout->addLayout(innerLayout);

        return outerLayout;
    }

    void addToMiniToolBarLayout(QBoxLayout*) {}

    void setWindowIconTB(QWidget* window) {
        ensure(window != nullptr, "window is null");
        window->setWindowIcon(QIcon{IO::loadPixmapResource("AppIcon.png")});
    }

    void setDebugBackgroundColor(QWidget* widget, const QColor& color) {
        auto p = widget->palette();
        p.setColor(QPalette::Window, color);

        widget->setAutoFillBackground(true);
        widget->setPalette(p);
    }

    void setDefaultWindowColor(QWidget* widget) {
        widget->setAttribute(Qt::WA_StyledBackground);
        widget->setProperty("uiSurface", "window");
        repolishWidget(widget);
    }

    void setBaseWindowColor(QWidget* widget) {
        widget->setAttribute(Qt::WA_StyledBackground);
        widget->setProperty("uiSurface", "base");
        repolishWidget(widget);
    }

    void setHighlightWindowColor(QWidget* widget) {
        widget->setAttribute(Qt::WA_StyledBackground);
        widget->setProperty("uiSurface", "highlight");
        repolishWidget(widget);
    }

    QLineEdit* createSearchBox() {
        auto* widget = new QLineEdit{};
        widget->setClearButtonEnabled(true);
        widget->setPlaceholderText(QLineEdit::tr("Search…"));

        const auto icon = IO::loadSVGIcon("Search.svg", 16);
        widget->addAction(icon, QLineEdit::LeadingPosition);
        return widget;
    }

    void checkButtonInGroup(QButtonGroup* group, const int id, const bool checked) {
        if (auto* button = group->button(id)) {
            button->setChecked(checked);
        }
    }

    void checkButtonInGroup(QButtonGroup* group, const QString& objectName, bool checked) {
        for (auto* button: group->buttons()) {
            if (button->objectName() == objectName) {
                button->setChecked(checked);
                return;
            }
        }
    }

    void insertTitleBarSeparator(QVBoxLayout* layout) {
#ifdef _WIN32
        layout->insertWidget(0, new BorderLine{}, 1);
#endif
        unused(layout);
    }

    AutoResizeRowsEventFilter::AutoResizeRowsEventFilter(QTableView* tableView)
        : QObject{tableView},
          m_tableView{
              tableView
          } {
        m_tableView->installEventFilter(this);
    }

    bool AutoResizeRowsEventFilter::eventFilter(QObject* watched, QEvent* event) {
        if (watched == m_tableView && event->type() == QEvent::Show) {
            m_tableView->resizeRowsToContents();
            m_tableView->removeEventFilter(this);
        }
        return QObject::eventFilter(watched, event);
    }

    void autoResizeRows(QTableView* tableView) {
        tableView->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        tableView->installEventFilter(new AutoResizeRowsEventFilter{tableView});
        tableView->resizeRowsToContents();
    }

    void deleteChildWidgetsLaterAndDeleteLayout(QWidget* widget) {
        const auto children = widget->findChildren<QWidget *>("", Qt::FindDirectChildrenOnly);
        for (auto* childWidget: children) {
            childWidget->deleteLater();
        }

        delete widget->layout();
    }

    void showModelessDialog(QDialog* dialog) {
        // https://doc.qt.io/qt-5/qdialog.html#code-examples
        dialog->show();
        dialog->raise();
        dialog->activateWindow();
    }

    static QTextCodec* codecForEncoding(const MapTextEncoding encoding) {
        switch (encoding) {
            case MapTextEncoding::Quake:
                // Quake uses the full 1-255 range for its bitmap font.
                // So using a "just assume UTF-8" approach would not work here.
                // See: https://github.com/TrenchBroom/TrenchBroom/issues/3122
                return QTextCodec::codecForLocale();
            case MapTextEncoding::Iso88591:
                return QTextCodec::codecForName("ISO 8859-1");
            case MapTextEncoding::Utf8:
                return QTextCodec::codecForName("UTF-8");
                switchDefault();
        }
    }

    QString mapStringToUnicode(const MapTextEncoding encoding, const std::string& string) {
        auto* codec = codecForEncoding(encoding);
        ensure(codec != nullptr, "null codec");

        return codec->toUnicode(QByteArray::fromStdString(string));
    }

    std::string mapStringFromUnicode(const MapTextEncoding encoding, const QString& string) {
        auto* codec = codecForEncoding(encoding);
        ensure(codec != nullptr, "null codec");

        return codec->fromUnicode(string).toStdString();
    }

    QString nativeModifierLabel(const int modifier) {
        assert(modifier == Qt::META || modifier == Qt::SHIFT || modifier == Qt::CTRL || modifier == Qt::ALT);

        const auto keySequence = QKeySequence(modifier);

        // QKeySequence doesn't totally support being given just a modifier
        // but it does seem to handle the key codes like Qt::SHIFT, which
        // it turns into native text as "Shift+" or the Shift symbol on macOS,
        // and portable text as "Shift+".

        auto nativeLabel = keySequence.toString(QKeySequence::NativeText);
        if (nativeLabel.endsWith("+")) {
            // On Linux we get nativeLabel as something like "Ctrl+"
            // On macOS it's just the special Command character, with no +
            nativeLabel.chop(1); // Remove last character
        }

        return nativeLabel;
    }
    } // namespace View
} // namespace TrenchBroom
