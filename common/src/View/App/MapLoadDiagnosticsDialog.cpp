#include "View/App/MapLoadDiagnosticsDialog.h"
#include "IO/PathQt.h"
#include "Preferences.h"
#include "PreferenceManager.h"
#include "View/Widgets/Lists/StyledItemDelegate.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPointer>
#include <QPlainTextEdit>
#include <QScreen>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <map>

namespace TrenchBroom::View {
MapLoadDiagnosticsDialog::MapLoadDiagnosticsDialog(const std::filesystem::path& path, const std::vector<LogMessage>& messages, QWidget* parent)
    : QDialog(parent) {
    setObjectName("MapLoadDiagnosticsDialog");
    setWindowTitle(tr("Map load diagnostics"));
    setWindowModality(Qt::NonModal);
    auto* layout = new QVBoxLayout(this);
    auto* summary = new QLabel(this);
    summary->setObjectName("MapLoadDiagnosticsSummary");
    summary->setTextFormat(Qt::PlainText);
    summary->setWordWrap(true);
    summary->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(summary);
    auto* list = new QTreeWidget(this);
    list->setObjectName("MapLoadDiagnosticsList");
    list->setItemDelegate(new StyledItemDelegate("loadDiagnostics", list));
    list->setHeaderLabels({tr("Level"), tr("Message"), tr("Count")});
    list->setRootIsDecorated(false);
    list->setAlternatingRowColors(false);
    list->setSelectionBehavior(QAbstractItemView::SelectRows);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    list->header()->setStretchLastSection(false);
    list->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    list->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    list->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    int warnings = 0, errors = 0;
    std::map<std::pair<LogLevel, QString>, QTreeWidgetItem*> rows;
    for (const auto& message : messages) {
        if (message.level != LogLevel::Warn && message.level != LogLevel::Error) continue;
        (message.level == LogLevel::Warn ? warnings : errors)++;
        auto& row = rows[{message.level, message.message}];
        if (!row) {
            row = new QTreeWidgetItem(list, {message.level == LogLevel::Warn ? tr("Warning") : tr("Error"), message.message, "0"});
            row->setToolTip(1, message.message);
            row->setData(0, StyledItemDelegate::TextStyleRole, message.level == LogLevel::Warn ? "warning" : "error");
            row->setData(1, StyledItemDelegate::TextStyleRole, "message");
            row->setData(2, StyledItemDelegate::TextStyleRole, "count");
            row->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
        }
        row->setText(2, QString::number(row->text(2).toInt() + 1));
    }
    summary->setText(tr("%1\nWarnings: %2 · Errors: %3").arg(IO::pathAsQString(path)).arg(warnings).arg(errors));
    layout->addWidget(list, 1);
    // Keep the overview compact while making long diagnostics readable and
    // selectable/copyable without relying on a tooltip or a wide window.
    auto* details = new QPlainTextEdit(this);
    details->setObjectName("MapLoadDiagnosticsMessage");
    details->setReadOnly(true);
    connect(list, &QTreeWidget::currentItemChanged, details, [details](QTreeWidgetItem* item) {
        details->setPlainText(item ? item->text(1) : QString{});
    });
    layout->addWidget(details);
    list->setCurrentItem(list->topLevelItem(0));
    auto* enabled = new QCheckBox(tr("Show this summary after loading maps"), this);
    enabled->setObjectName("ShowMapLoadDiagnostics");
    enabled->setChecked(pref(Preferences::ShowMapLoadDiagnostics));
    connect(enabled, &QCheckBox::toggled, this, [](bool value) { setPref(Preferences::ShowMapLoadDiagnostics, value); });
    layout->addWidget(enabled);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    layout->addWidget(buttons);
    ensurePolished();
    const auto hint = sizeHint();
    resize(QSize(m_initialWidth > 0 ? m_initialWidth : hint.width(), m_initialHeight > 0 ? m_initialHeight : hint.height())
        .boundedTo(screen()->availableGeometry().size() - QSize(40, 40)));
}

void showMapLoadDiagnostics(const std::filesystem::path& path, const std::vector<LogMessage>& messages, QWidget* parent) {
    if (!parent || !pref(Preferences::ShowMapLoadDiagnostics)
        || std::none_of(messages.begin(), messages.end(), [](const auto& message) {
            return message.level == LogLevel::Warn || message.level == LogLevel::Error;
        })) return;
    const QPointer<QWidget> guard(parent);
    QTimer::singleShot(0, parent, [guard, path, messages] {
        if (!guard || !pref(Preferences::ShowMapLoadDiagnostics)) return;
        auto* dialog = new MapLoadDiagnosticsDialog(path, messages, guard);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    });
}
}
