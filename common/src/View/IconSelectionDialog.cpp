#include "IconSelectionDialog.h"

#include "IO/PathQt.h"
#include "IO/ResourceUtils.h"
#include "IO/SystemPaths.h"
#include "View/QtUtils.h"
#include "View/ViewConstants.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QSet>
#include <QVBoxLayout>

#include <algorithm>

namespace TrenchBroom::View {

QStringList IconSelectionDialog::availableIcons() {
    QSet<QString> names;
    for (const auto& path : IO::SystemPaths::findResourceDirectories("images")) {
        const auto directory = QDir{IO::pathAsQString(path)};
        auto files = QDirIterator{directory.absolutePath(), QDir::Files | QDir::Readable, QDirIterator::Subdirectories};
        while (files.hasNext()) {
            const auto file = files.next();
            if (QFileInfo(file).suffix().compare("svg", Qt::CaseInsensitive) == 0) names.insert(directory.relativeFilePath(file));
        }
    }
    auto result = names.values();
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        const auto comparison = QString::compare(a, b, Qt::CaseInsensitive);
        return comparison == 0 ? a < b : comparison < 0;
    });
    return result;
}

IconSelectionDialog::IconSelectionDialog(const QString& currentIcon, QWidget* parent) : QDialog{parent} {
    setObjectName("EditorIconSelection");
    setWindowTitle(tr("Choose icon"));
    setWindowIconTB(this);
    resize(680, 520);
    setMinimumSize(400, 320);

    m_filter = new QLineEdit;
    m_filter->setObjectName("EditorIconFilter");
    m_filter->setPlaceholderText(tr("Search icons…"));
    m_filter->setClearButtonEnabled(true);

    m_icons = new QListWidget;
    m_icons->setObjectName("EditorIconGrid");
    m_icons->setProperty("preferencesList", true);
    m_icons->setViewMode(QListView::IconMode);
    m_icons->setMovement(QListView::Static);
    m_icons->setResizeMode(QListView::Adjust);
    m_icons->setSelectionMode(QAbstractItemView::SingleSelection);
    m_icons->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_icons->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_icons->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_icons->setIconSize(QSize{40, 40});
    m_icons->setGridSize(QSize{112, 86});
    m_icons->setUniformItemSizes(true);
    m_icons->setWordWrap(false);
    m_icons->setTextElideMode(Qt::ElideRight);

    m_preview = new QLabel;
    m_preview->setObjectName("EditorIconPreview");
    m_preview->setFixedSize(56, 56);
    m_preview->setAlignment(Qt::AlignCenter);
    m_name = new QLabel;
    m_name->setObjectName("EditorIconName");
    m_name->setTextFormat(Qt::PlainText);
    m_name->setWordWrap(true);
    m_count = new QLabel;
    m_count->setObjectName("EditorIconCount");
    m_count->setProperty("textStyle", "info");
    auto* details = new QVBoxLayout;
    details->addWidget(m_name);
    details->addWidget(m_count);
    auto* preview = new QHBoxLayout;
    preview->addWidget(m_preview);
    preview->addLayout(details, 1);

    m_buttons = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel};
    m_buttons->setObjectName("EditorIconButtons");
    auto* layout = new QVBoxLayout{this};
    layout->setContentsMargins(LayoutConstants::DialogOuterMargin, LayoutConstants::DialogOuterMargin, LayoutConstants::DialogOuterMargin, LayoutConstants::DialogOuterMargin);
    layout->addWidget(m_filter);
    layout->addWidget(m_icons, 1);
    layout->addLayout(preview);
    layout->addWidget(m_buttons);

    auto selectedName = QDir::cleanPath(currentIcon);
    if (QFileInfo(currentIcon).isAbsolute()) {
        for (const auto& path : IO::SystemPaths::findResourceDirectories("images")) {
            const auto relative = QDir(IO::pathAsQString(path)).relativeFilePath(currentIcon);
            if (!relative.startsWith("../") && !QDir::isAbsolutePath(relative)) {
                selectedName = relative;
                break;
            }
        }
    }
    QListWidgetItem* current = nullptr;
    for (const auto& name : availableIcons()) {
        auto* item = new QListWidgetItem{IO::loadSVGIcon(IO::pathFromQString(name), 40), QFileInfo(name).completeBaseName(), m_icons};
        item->setData(Qt::UserRole, name);
        item->setToolTip(name);
        if (name == selectedName) current = item;
    }
    connect(m_filter, &QLineEdit::textChanged, this, &IconSelectionDialog::filterIcons);
    connect(m_icons, &QListWidget::currentItemChanged, this, [this] { updateSelection(); });
    connect(m_icons, &QListWidget::itemActivated, this, [this] { accept(); });
    connect(m_buttons, &QDialogButtonBox::accepted, this, &IconSelectionDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &IconSelectionDialog::reject);
    m_icons->setCurrentItem(current ? current : m_icons->item(0));
    if (current) m_icons->scrollToItem(current, QAbstractItemView::PositionAtCenter);
    filterIcons({});
    m_filter->setFocus();
}

QString IconSelectionDialog::selectedIcon() const {
    const auto* item = m_icons->currentItem();
    return item && !item->isHidden() ? item->data(Qt::UserRole).toString() : QString{};
}

void IconSelectionDialog::accept() {
    if (!selectedIcon().isEmpty()) QDialog::accept();
}

void IconSelectionDialog::filterIcons(const QString& filter) {
    auto visible = 0;
    QListWidgetItem* first = nullptr;
    for (auto i = 0; i < m_icons->count(); ++i) {
        auto* item = m_icons->item(i);
        const auto matches = item->data(Qt::UserRole).toString().contains(filter.trimmed(), Qt::CaseInsensitive);
        item->setHidden(!matches);
        if (matches) {
            ++visible;
            if (!first) first = item;
        }
    }
    if (!m_icons->currentItem() || m_icons->currentItem()->isHidden()) m_icons->setCurrentItem(first);
    m_count->setText(tr("%1 of %2 icons").arg(visible).arg(m_icons->count()));
    updateSelection();
}

void IconSelectionDialog::updateSelection() {
    const auto name = selectedIcon();
    m_name->setText(name.isEmpty() ? tr("No matching icons") : name);
    m_preview->clear();
    if (!name.isEmpty()) {
        const auto ratio = m_preview->devicePixelRatioF();
        QPixmap pixmap{QSize{qRound(48 * ratio), qRound(48 * ratio)}};
        pixmap.setDevicePixelRatio(ratio);
        pixmap.fill(Qt::transparent);
        {
            QPainter painter{&pixmap};
            IO::loadSVGIcon(IO::pathFromQString(name), 48).paint(&painter, QRect{0, 0, 48, 48});
        }
        m_preview->setPixmap(pixmap);
    }
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(!name.isEmpty());
}

} // namespace TrenchBroom::View
