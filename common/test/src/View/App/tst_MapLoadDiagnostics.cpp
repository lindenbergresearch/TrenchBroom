#include "View/Tools/Brush/BrushUpstreamTest.h"
#include "View/App/MapLoadDiagnosticsDialog.h"
#include "View/Preferences/ViewPreferencePane.h"
#include "PreferenceManager.h"
#include "Preferences.h"

#include <QApplication>
#include <QCheckBox>
#include <QLabel>
#include <QHeaderView>
#include <QPointer>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace TrenchBroom::View {
namespace {
class ForwardLogger : public Logger {
    Logger& m_next;
    void doLog(LogLevel level, const LogMessage* message) override { m_next.log(level, message); }
public:
    explicit ForwardLogger(Logger& next) : m_next(next) {}
};
void flushDialogs() {
    QApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}
}
TEST_CASE("Load diagnostics capture warnings once across log forwarding and console filtering", "[load-diagnostics]") {
    NullLogger target;
    target.setLogLevel(LogLevel::Error);
    ForwardLogger proxy(target);
    proxy.setLogLevel(LogLevel::Debug);
    ScopedDiagnosticLog capture;
    proxy.info("Not a diagnostic");
    proxy.warn("Missing texture");
    proxy.error("Missing definition");
    REQUIRE(capture.messages().size() == 2);
    CHECK(capture.messages()[0].level == LogLevel::Warn);
    CHECK(capture.messages()[0].message == "Missing texture");
    CHECK(capture.messages()[1].level == LogLevel::Error);
    target.warn("Hidden by console filter");
    REQUIRE(capture.messages().size() == 3);
    CHECK(capture.messages().back().message == "Hidden by console filter");
    const LogMessage message{LogLevel::Error, "Pointer overload"};
    proxy.log(LogLevel::Error, &message);
    REQUIRE(capture.messages().size() == 4);
    CHECK(capture.messages().back().message == "Pointer overload");
}
TEST_CASE("Diagnostic scopes restore capture after nested operations and exceptions", "[load-diagnostics]") {
    NullLogger logger;
    ScopedDiagnosticLog outer;
    {
        ScopedDiagnosticLog inner;
        logger.warn("Both scopes");
        CHECK(inner.messages().size() == 1);
        CHECK(outer.messages().size() == 1);
    }
    {
        ScopedDiagnosticLog disabled(false);
        logger.error("Outer only");
        CHECK(disabled.messages().empty());
    }
    CHECK_THROWS_AS(([&] {
        ScopedDiagnosticLog exceptional;
        logger.warn("Before exception");
        throw std::runtime_error("Stop operation");
    }()), std::runtime_error);
    logger.error("After exception");
    REQUIRE(outer.messages().size() == 4);
    CHECK(outer.messages().back().message == "After exception");
}
TEST_CASE("Map load summary groups identical diagnostics and keeps messages read-only", "[load-diagnostics]") {
    const auto enabled = TemporarilySetPref{Preferences::ShowMapLoadDiagnostics, true};
    MapLoadDiagnosticsDialog dialog("maps/<test>.map", {
        {LogLevel::Info, "Ignore"},
        {LogLevel::Warn, "Missing texture <a>"},
        {LogLevel::Error, "Missing texture <a>"},
        {LogLevel::Warn, "Missing texture <a>"}});
    auto* list = dialog.findChild<QTreeWidget*>("MapLoadDiagnosticsList");
    REQUIRE(list);
    REQUIRE(list->topLevelItemCount() == 2);
    CHECK(list->editTriggers() == QAbstractItemView::NoEditTriggers);
    CHECK_FALSE(list->alternatingRowColors());
    CHECK_FALSE(list->header()->stretchLastSection());
    CHECK(list->topLevelItem(0)->text(1) == "Missing texture <a>");
    CHECK(list->topLevelItem(0)->text(2) == "2");
    CHECK(list->topLevelItem(1)->text(2) == "1");
    auto* details = dialog.findChild<QPlainTextEdit*>("MapLoadDiagnosticsMessage");
    REQUIRE(details);
    CHECK(details->isReadOnly());
    CHECK(details->toPlainText() == "Missing texture <a>");
    list->topLevelItem(1)->setText(1, "Another diagnostic\nwith full detail");
    list->setCurrentItem(list->topLevelItem(1));
    CHECK(details->toPlainText() == "Another diagnostic\nwith full detail");
    auto* summary = dialog.findChild<QLabel*>("MapLoadDiagnosticsSummary");
    REQUIRE(summary);
    CHECK(summary->textFormat() == Qt::PlainText);
    CHECK(summary->text().contains("Warnings: 2"));
    CHECK(summary->text().contains("Errors: 1"));
    CHECK(dialog.windowModality() == Qt::NonModal);
    auto* toggle = dialog.findChild<QCheckBox*>("ShowMapLoadDiagnostics");
    REQUIRE(toggle);
    toggle->setChecked(false);
    CHECK_FALSE(pref(Preferences::ShowMapLoadDiagnostics));
}
TEST_CASE("Load summaries respect preferences parent lifetime and clean loads", "[load-diagnostics]") {
    const auto enabled = TemporarilySetPref{Preferences::ShowMapLoadDiagnostics, true};
    QWidget parent;
    const std::vector<LogMessage> warning{{LogLevel::Warn, "Missing texture"}};
    showMapLoadDiagnostics("clean.map", {}, &parent);
    showMapLoadDiagnostics("info.map", {{LogLevel::Info, "Loaded"}}, &parent);
    flushDialogs();
    CHECK(parent.findChildren<MapLoadDiagnosticsDialog*>().empty());
    setPref(Preferences::ShowMapLoadDiagnostics, false);
    showMapLoadDiagnostics("disabled.map", warning, &parent);
    flushDialogs();
    CHECK(parent.findChildren<MapLoadDiagnosticsDialog*>().empty());
    setPref(Preferences::ShowMapLoadDiagnostics, true);
    showMapLoadDiagnostics("suppressed.map", warning, &parent);
    setPref(Preferences::ShowMapLoadDiagnostics, false);
    flushDialogs();
    CHECK(parent.findChildren<MapLoadDiagnosticsDialog*>().empty());
    setPref(Preferences::ShowMapLoadDiagnostics, true);
    {
        QWidget temporary;
        showMapLoadDiagnostics("closed.map", warning, &temporary);
    }
    flushDialogs();
    showMapLoadDiagnostics("warning.map", warning, &parent);
    CHECK(parent.findChildren<MapLoadDiagnosticsDialog*>().empty()); // Only after loading returns.
    flushDialogs();
    auto* dialog = parent.findChild<MapLoadDiagnosticsDialog*>();
    REQUIRE(dialog);
    CHECK(dialog->isVisible());
    QPointer<MapLoadDiagnosticsDialog> guard(dialog);
    dialog->close();
    flushDialogs();
    CHECK(guard.isNull());
}
TEST_CASE_METHOD(BrushUpstreamTest, "Actual map loads report asset errors without aborting", "[load-diagnostics]") {
    const auto enabled = TemporarilySetPref{Preferences::ShowMapLoadDiagnostics, true};
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    const auto path = std::filesystem::path(folder.path().toStdString()) / "warning.map";
    {
        std::ofstream map(path);
        map << "{\n\"classname\" \"worldspawn\"\n\"_tb_def\" \"external:missing.fgd\"\n}\n";
    }
    ScopedDiagnosticLog capture;
    const auto result = document->loadDocument(Model::MapFormat::Standard, vm::bbox3{32768.0}, game, path);
    REQUIRE(result.is_success());
    REQUIRE_FALSE(capture.messages().empty());
    CHECK(std::any_of(capture.messages().begin(), capture.messages().end(), [](const auto& message) {
        return message.level == LogLevel::Error && message.message.contains("missing.fgd");
    }));
    QWidget parent;
    showMapLoadDiagnostics(path, capture.messages(), &parent);
    flushDialogs();
    auto* dialog = parent.findChild<MapLoadDiagnosticsDialog*>();
    REQUIRE(dialog);
    CHECK(dialog->findChild<QTreeWidget*>("MapLoadDiagnosticsList")->topLevelItemCount() > 0);
}
TEST_CASE("Map load summary can be configured in View preferences", "[load-diagnostics]") {
    const auto enabled = TemporarilySetPref{Preferences::ShowMapLoadDiagnostics, true};
    ViewPreferencePane pane(nullptr);
    pane.updateControls();
    auto* toggle = pane.findChild<QCheckBox*>("ShowMapLoadDiagnostics");
    REQUIRE(toggle);
    CHECK(toggle->isChecked());
    toggle->setChecked(false);
    CHECK_FALSE(pref(Preferences::ShowMapLoadDiagnostics));
    pane.updateControls();
    CHECK_FALSE(toggle->isChecked());
}
} // namespace TrenchBroom::View
