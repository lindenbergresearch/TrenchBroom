#include "View/Preferences/IconSelectionDialog.h"

#include "IO/IconCatalog.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include "View/Theme/QtUtils.h"
#include "View/Theme/ViewConstants.h"
#include "View/Widgets/ColorButton.h"

#include <QAbstractListModel>
#include <QDialogButtonBox>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QTextLayout>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace TrenchBroom::View {
namespace {

constexpr auto SearchRole = Qt::UserRole + 1;

QString sizeText(const IO::IconCatalog& catalog, const QString& name) {
    const auto size = catalog.sourceSize(name, std::clamp(pref(Preferences::ToolBarIconsSize), 1, 256));
    return size.isEmpty() ? QString{} : QObject::tr("%1 × %2 px").arg(size.width()).arg(size.height());
}

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
                return entry.name + '\n' + sizeText(m_catalog, entry.name) + '\n' + entry.source;
            case Qt::AccessibleTextRole:
                return entry.name + ' ' + sizeText(m_catalog, entry.name);
            case SearchRole:
                return entry.name + '\n' + entry.source;
            default:
                return {};
        }
    }
};

QString elideName(const QFontMetrics& metrics, const QString& name, const QString& query, int width) {
    const auto elided = metrics.elidedText(name, Qt::ElideMiddle, width);
    if (query.isEmpty() || elided.contains(query, Qt::CaseInsensitive)) return elided;
    const auto start = name.indexOf(query, 0, Qt::CaseInsensitive);
    if (start < 0) return elided;

    // Keep the first match visible even when it was in the elided middle.
    const auto end = start + query.size();
    const auto throughMatch = name.left(end) + (end < name.size() ? QStringLiteral("…") : QString{});
    const auto aroundMatch = metrics.elidedText(throughMatch, Qt::ElideLeft, width);
    return aroundMatch.contains(query, Qt::CaseInsensitive)
        ? aroundMatch : metrics.elidedText(name.mid(start), Qt::ElideRight, width);
}

class IconTileDelegate : public QStyledItemDelegate {
    const QListView& m_view;
    const QLineEdit& m_filter;
    const IO::IconCatalog& m_catalog;
    const IO::IconStyle& m_iconStyle;

public:
    IconTileDelegate(QListView& view, const QLineEdit& filter, const IO::IconCatalog& catalog, const IO::IconStyle& iconStyle)
        : QStyledItemDelegate(&view), m_view(view), m_filter(filter), m_catalog(catalog), m_iconStyle(iconStyle) {}

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override {
        return m_view.gridSize();
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        auto opt = option;
        initStyleOption(&opt, index);
        const auto name = opt.text;
        const auto icon = index == m_view.currentIndex() ? m_catalog.icon(name, m_iconStyle) : opt.icon;
        const auto selected = opt.state.testFlag(QStyle::State_Selected);
        const auto foreground = opt.palette.color(selected ? QPalette::HighlightedText : QPalette::Text);
        opt.text.clear();
        opt.icon = {};

        painter->save();
        painter->setClipRect(opt.rect, Qt::IntersectClip);
        m_view.style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter, &m_view);
        const auto inset = opt.rect.adjusted(4, 3, -4, -3);
        const auto lineHeight = opt.fontMetrics.height();
        const auto iconArea = inset.adjusted(0, 0, 0, -2 * lineHeight - 3);
        const auto iconSize = m_view.iconSize().boundedTo(iconArea.size());
        if (!iconSize.isEmpty()) {
            auto iconRect = QRect(QPoint{}, iconSize);
            iconRect.moveCenter(iconArea.center());
            icon.paint(painter, iconRect);
        }

        const auto nameRect = QRect(inset.left(), iconArea.bottom() + 4, inset.width(), lineHeight);
        const auto query = m_filter.text().trimmed();
        const auto label = elideName(opt.fontMetrics, name, query, nameRect.width());
        painter->setFont(opt.font);
        painter->setPen(foreground);
        if (query.isEmpty()) {
            painter->drawText(nameRect, Qt::AlignCenter, label);
        } else {
            auto match = QTextCharFormat{};
            match.setForeground(opt.palette.color(selected ? QPalette::Text : QPalette::HighlightedText));
            match.setBackground(opt.palette.color(selected ? QPalette::Base : QPalette::Highlight));
            auto formats = QList<QTextLayout::FormatRange>{};
            for (auto pos = label.indexOf(query, 0, Qt::CaseInsensitive); pos >= 0;
                 pos = label.indexOf(query, pos + query.size(), Qt::CaseInsensitive)) {
                formats.append({int(pos), int(query.size()), match});
            }
            if (formats.isEmpty() && name.contains(query, Qt::CaseInsensitive)) {
                // The query itself can be wider than the tile. Mark the
                // part that elideName kept before the ellipsis.
                const auto visibleMatch = label.left(label.indexOf(QChar(0x2026)));
                if (!visibleMatch.isEmpty() && query.startsWith(visibleMatch, Qt::CaseInsensitive))
                    formats.append({0, int(visibleMatch.size()), match});
            }
            auto layout = QTextLayout(label, opt.font, painter->device());
            auto textOption = QTextOption(Qt::AlignHCenter);
            textOption.setWrapMode(QTextOption::NoWrap);
            layout.setTextOption(textOption);
            layout.setFormats(formats);
            layout.beginLayout();
            auto line = layout.createLine();
            if (line.isValid()) line.setLineWidth(nameRect.width());
            layout.endLayout();
            layout.draw(painter, nameRect.topLeft());
        }
        painter->drawText(nameRect.translated(0, lineHeight), Qt::AlignCenter, sizeText(m_catalog, name));
        painter->restore();
    }
};

} // namespace

IconSelectionDialog::IconSelectionDialog(const IO::IconCatalog& catalog, const QString& currentIcon,
                                         const IO::IconStyle& iconStyle, QWidget* parent)
    : QDialog{parent}, m_catalog(catalog), m_iconStyle(iconStyle) {
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
    m_icons->setIconSize({40, 40});
    m_icons->setGridSize({112, 86});
    m_icons->setItemDelegate(new IconTileDelegate(*m_icons, *m_filter, m_catalog, m_iconStyle));

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

    m_size = new QSpinBox;
    m_size->setObjectName("EditorIconSize");
    m_size->setRange(8, 256);
    m_size->setSuffix(tr(" px"));
    m_size->setToolTip(tr("Size of the icons in this picker."));
    auto* sizeLabel = new QLabel(tr("Preview size"));
    sizeLabel->setBuddy(m_size);

    m_color = new ColorButton;
    m_color->setObjectName("EditorIconColor");
    m_color->setAccessibleName(tr("Icon color"));
    m_color->setColor(m_iconStyle.color.isValid() ? m_iconStyle.color : palette().color(QPalette::WindowText));
    m_defaultColor = new QPushButton(tr("Default"));
    m_defaultColor->setObjectName("EditorIconDefaultColor");
    m_defaultColor->setAutoDefault(false);
    m_defaultColor->setToolTip(tr("Use the SVG colors or the theme text color for a font icon."));
    auto* colorLabel = new QLabel(tr("Color"));
    colorLabel->setBuddy(m_color);
    auto* colorLayout = new QHBoxLayout;
    colorLayout->addWidget(m_color);
    colorLayout->addWidget(m_defaultColor);
    colorLayout->addStretch(1);

    auto* appearance = new QGridLayout;
    appearance->addWidget(sizeLabel, 0, 0);
    appearance->addWidget(m_size, 0, 1, Qt::AlignLeft);
    appearance->addWidget(colorLabel, 0, 2);
    appearance->addLayout(colorLayout, 0, 3);
    const auto addPercent = [&](const QString& label, const char* name, const QString& automatic, const std::optional<qreal>& value, int column) {
        auto* input = new QSpinBox;
        input->setObjectName(QString::fromLatin1(name));
        input->setRange(-1, 100);
        input->setSpecialValueText(automatic);
        input->setSuffix(tr(" %"));
        input->setValue(value ? qRound(100 * *value) : -1);
        auto* caption = new QLabel(label);
        caption->setBuddy(input);
        appearance->addWidget(caption, 1, column);
        appearance->addWidget(input, 1, column + 1, Qt::AlignLeft);
        return input;
    };
    m_brightness = addPercent(tr("Brightness"), "EditorIconBrightness", tr("Theme"), m_iconStyle.brightness, 0);
    m_opacity = addPercent(tr("Opacity"), "EditorIconOpacity", tr("Default"), m_iconStyle.opacity, 2);
    appearance->setColumnStretch(3, 1);

    m_buttons = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel};
    m_buttons->setObjectName("EditorIconButtons");
    auto* layout = new QVBoxLayout{this};
    layout->setContentsMargins(LayoutConstants::DialogOuterMargin, LayoutConstants::DialogOuterMargin, LayoutConstants::DialogOuterMargin, LayoutConstants::DialogOuterMargin);
    layout->addWidget(m_filter);
    layout->addWidget(m_icons, 1);
    layout->addLayout(preview);
    layout->addLayout(appearance);
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
    connect(m_icons, &QListView::iconSizeChanged, this, [this](const QSize& size) {
        const auto blocker = QSignalBlocker(m_size);
        m_size->setValue(std::max(size.width(), size.height()));
    });
    connect(m_size, &QSpinBox::valueChanged, this, [this](int size) {
        const auto iconSize = QSize(size, size);
        const auto minimumSpace = QSize(12, 2 * m_icons->fontMetrics().height() + 9);
        const auto space = (m_icons->gridSize() - m_icons->iconSize()).expandedTo(minimumSpace);
        m_icons->setGridSize(iconSize + space);
        m_icons->setIconSize(iconSize);
        if (m_icons->currentIndex().isValid()) m_icons->scrollTo(m_icons->currentIndex());
    });
    connect(m_color, &ColorButton::colorChangedByUser, this, [this](const QColor& color) {
        m_iconStyle.color = color;
        updateSelection();
    });
    connect(m_defaultColor, &QPushButton::clicked, this, [this] {
        m_iconStyle.color = {};
        m_color->setColor(palette().color(QPalette::WindowText));
        updateSelection();
    });
    connect(m_brightness, &QSpinBox::valueChanged, this, [this](int value) {
        m_iconStyle.brightness = value >= 0 ? std::optional{value / 100.0} : std::nullopt;
        updateSelection();
    });
    connect(m_opacity, &QSpinBox::valueChanged, this, [this](int value) {
        m_iconStyle.opacity = value >= 0 ? std::optional{value / 100.0} : std::nullopt;
        updateSelection();
    });
    connect(m_icons->selectionModel(), &QItemSelectionModel::currentChanged, this, [this] { updateSelection(); });
    connect(m_icons, &QListView::activated, this, [this] { accept(); });
    connect(m_buttons, &QDialogButtonBox::accepted, this, &IconSelectionDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &IconSelectionDialog::reject);
    m_icons->ensurePolished();
    m_size->setValue(std::max(m_icons->iconSize().width(), m_icons->iconSize().height()));
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
    m_icons->viewport()->update();
    updateSelection();
}

void IconSelectionDialog::updateSelection() {
    const auto name = selectedIcon();
    const auto dimensions = sizeText(m_catalog, name);
    m_name->setText(name.isEmpty() ? tr("No matching icons") : dimensions.isEmpty() ? name : tr("%1 · %2").arg(name, dimensions));
    m_preview->clear();
    if (!name.isEmpty()) {
        const auto ratio = m_preview->devicePixelRatioF();
        const auto logicalSize = m_preview->contentsRect().size().expandedTo(QSize{1, 1});
        QPixmap pixmap{logicalSize * ratio};
        pixmap.setDevicePixelRatio(ratio);
        pixmap.fill(Qt::transparent);
        {
            QPainter painter{&pixmap};
            m_catalog.icon(name, m_iconStyle).paint(&painter, QRect{QPoint{}, logicalSize});
        }
        m_preview->setPixmap(pixmap);
    }
    const auto hasIcon = !name.isEmpty();
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(hasIcon);
    m_color->setEnabled(hasIcon);
    m_defaultColor->setEnabled(hasIcon && m_iconStyle.color.isValid());
    m_brightness->setEnabled(hasIcon);
    m_opacity->setEnabled(hasIcon);
    m_icons->viewport()->update(m_icons->visualRect(m_icons->currentIndex()));
}

} // namespace TrenchBroom::View
