#include "View/Preferences/IconSelectionDialog.h"

#include "IO/IconCatalog.h"
#include "View/Theme/QtUtils.h"
#include "View/Theme/ViewConstants.h"

#include <QAbstractListModel>
#include <QDialogButtonBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPainter>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <QVBoxLayout>

namespace TrenchBroom::View {
namespace {

constexpr auto SearchRole = Qt::UserRole + 1;

class IconListModel : public QAbstractListModel {
    const IO::IconCatalog& m_catalog;

public:
    IconListModel(const IO::IconCatalog& catalog, QObject* parent) : QAbstractListModel(parent), m_catalog(catalog) {}

    int rowCount(const QModelIndex& parent = {}) const override {
        return parent.isValid() ? 0 : int(m_catalog.entries().size());
    }

    QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) return {};
        const auto& entry = m_catalog.entries()[index.row()];
        switch (role) {
            case Qt::DisplayRole:
            case Qt::UserRole:
                return entry.name;
            case Qt::DecorationRole:
                return m_catalog.icon(entry.name);
            case Qt::ToolTipRole:
            case SearchRole:
                return entry.name + '\n' + entry.source;
            default:
                return {};
        }
    }
};

} // namespace

IconSelectionDialog::IconSelectionDialog(const IO::IconCatalog& catalog, const QString& currentIcon, QWidget* parent)
    : QDialog{parent}, m_catalog(catalog) {
    setObjectName("EditorIconSelection");
    setWindowTitle(tr("Choose icon"));
    setWindowIconTB(this);
    resize(680, 520);
    setMinimumSize(400, 320);

    m_filter = new QLineEdit;
    m_filter->setObjectName("EditorIconFilter");
    m_filter->setPlaceholderText(tr("Search icons…"));
    m_filter->setClearButtonEnabled(true);

    m_icons = new QListView;
    m_icons->setObjectName("EditorIconGrid");
    m_icons->setProperty("preferencesList", true);
    m_icons->setViewMode(QListView::IconMode);
    m_icons->setMovement(QListView::Static);
    m_icons->setResizeMode(QListView::Adjust);
    m_icons->setSelectionMode(QAbstractItemView::SingleSelection);
    m_icons->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_icons->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_icons->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_icons->setUniformItemSizes(true);
    m_icons->setLayoutMode(QListView::Batched);
    m_icons->setWordWrap(false);
    m_icons->setTextElideMode(Qt::ElideRight);

    m_preview = new QLabel;
    m_preview->setObjectName("EditorIconPreview");
    m_preview->installEventFilter(this);
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

    // The model asks for a QIcon only when Qt needs it for a tile. Filtering
    // thousands of glyph names doesn't rasterize the fonts or load the SVGs.
    auto* model = new IconListModel(m_catalog, this);
    m_results = new QSortFilterProxyModel(this);
    m_results->setSourceModel(model);
    m_results->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_results->setFilterRole(SearchRole);
    m_icons->setModel(m_results);
    const auto matches = model->match(model->index(0), Qt::UserRole, m_catalog.canonicalName(currentIcon),
        1, Qt::MatchExactly);
    const auto current = matches.isEmpty() ? m_results->index(0, 0) : m_results->mapFromSource(matches.front());
    connect(m_filter, &QLineEdit::textChanged, this, &IconSelectionDialog::filterIcons);
    connect(m_icons->selectionModel(), &QItemSelectionModel::currentChanged, this, [this] { updateSelection(); });
    connect(m_icons, &QListView::activated, this, [this] { accept(); });
    connect(m_buttons, &QDialogButtonBox::accepted, this, &IconSelectionDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &IconSelectionDialog::reject);
    m_icons->setCurrentIndex(current);
    if (current.isValid()) m_icons->scrollTo(current, QAbstractItemView::PositionAtCenter);
    filterIcons({});
    m_filter->setFocus();
}

QString IconSelectionDialog::selectedIcon() const {
    return m_icons->currentIndex().data(Qt::UserRole).toString();
}

bool IconSelectionDialog::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_preview && (event->type() == QEvent::Resize || event->type() == QEvent::StyleChange)) QTimer::singleShot(0, this, [this] { updateSelection(); });
    return QDialog::eventFilter(watched, event);
}

void IconSelectionDialog::accept() {
    if (!selectedIcon().isEmpty()) QDialog::accept();
}

void IconSelectionDialog::filterIcons(const QString& filter) {
    m_results->setFilterFixedString(filter.trimmed());
    if (!m_icons->currentIndex().isValid()) m_icons->setCurrentIndex(m_results->index(0, 0));
    m_count->setText(tr("%1 of %2 icons").arg(m_results->rowCount()).arg(m_catalog.entries().size()));
    updateSelection();
}

void IconSelectionDialog::updateSelection() {
    const auto name = selectedIcon();
    m_name->setText(name.isEmpty() ? tr("No matching icons") : name);
    m_preview->clear();
    if (!name.isEmpty()) {
        const auto ratio = m_preview->devicePixelRatioF();
        const auto logicalSize = m_preview->contentsRect().size().expandedTo(QSize{1, 1});
        QPixmap pixmap{logicalSize * ratio};
        pixmap.setDevicePixelRatio(ratio);
        pixmap.fill(Qt::transparent);
        {
            QPainter painter{&pixmap};
            m_catalog.icon(name).paint(&painter, QRect{QPoint{}, logicalSize});
        }
        m_preview->setPixmap(pixmap);
    }
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(!name.isEmpty());
}

} // namespace TrenchBroom::View
