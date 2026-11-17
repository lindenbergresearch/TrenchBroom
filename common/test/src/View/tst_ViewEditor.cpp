#include "Error.h"
#include "IO/GameConfigParser.h"
#include "IO/VectorIcon.h"
#include "Logger.h"
#include "Model/GameImpl.h"
#include "Preferences.h"
#include "View/Actions.h"
#include "View/Grid.h"
#include "View/MapDocument.h"
#include "View/MapDocumentCommandFacade.h"
#include "View/ViewEditor.h"

#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QTabWidget>
#include <catch2/catch.hpp>

namespace TrenchBroom::View {
namespace {
class ViewEditorTest {
protected:
    NullLogger logger;
    Model::GameConfig config = IO::GameConfigParser(R"({
        "version": 8, "name": "Generic",
        "fileformats": [{"format": "Standard"}],
        "filesystem": {"searchpath": ".", "packageformat": {"extension": ".pak", "format": "idpak"} },
        "textures": {"root": "textures", "format": {"extensions": [".png"], "format": "image"} },
        "entities": {"definitions": ["Generic.fgd"], "defaultcolor": "0.6 0.6 0.6 1.0"}
    })", std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path().parent_path()
        / "app/resources/games/Generic/GameConfig.cfg").parse();
    std::shared_ptr<Model::GameImpl> game = std::make_shared<Model::GameImpl>(config, std::filesystem::path{}, logger);
    std::shared_ptr<MapDocument> document = MapDocumentCommandFacade::newMapDocument();
    ViewEditor editor{document};

    void newMap() {
        REQUIRE(document->newDocument(Model::MapFormat::Standard, vm::bbox3(8192.0), game).is_success());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    template <typename T> T* control(const char* name) {
        auto* result = editor.findChild<T*>(name);
        REQUIRE(result);
        return result;
    }
};
}

TEST_CASE_METHOD(ViewEditorTest, "Grid view controls follow preferences and document state", "[view-editor]") {
    const auto alpha = TemporarilySetPref{Preferences::GridAlpha, 0.53f};
    const auto width = TemporarilySetPref{Preferences::GridLineWidth, 2.25f};
    const auto mix = TemporarilySetPref{Preferences::GridAxisColorMix, 0.42f};
    const auto units = TemporarilySetPref{Preferences::GridUnitSystem, 0};
    const auto shade = TemporarilySetPref{Preferences::ShadeFaces, true};
    const auto shadeGrid = TemporarilySetPref{Preferences::ShadeGrid, false};
    // Notifications can arrive before the first document is ready.
    PreferenceManager::instance().preferenceDidChangeNotifier(Preferences::GridAlpha.path());
    document->grid().toggleVisible();
    newMap();
    auto* opacity = control<SliderWithLabel>("ViewGridOpacity");
    auto* thickness = control<SliderWithLabel>("ViewGridWidth");
    CHECK(opacity->value() == 53); CHECK(thickness->value() == 225);
    CHECK(control<SliderWithLabel>("ViewGridAxisColorMix")->value() == 42);
    opacity->setValue(61); CHECK(pref(Preferences::GridAlpha) == Approx(0.61f));
    thickness->setValue(175); CHECK(pref(Preferences::GridLineWidth) == Approx(1.75f));
    setPref(Preferences::GridAlpha, 0.83f);
    PreferenceManager::instance().preferenceDidChangeNotifier(Preferences::GridAlpha.path());
    CHECK(opacity->value() == 83);
    auto* visible = control<QCheckBox>("ViewGridVisible");
    CHECK(visible->isChecked() == document->grid().visible());
    visible->click(); CHECK(visible->isChecked() == document->grid().visible());
    document->grid().toggleVisible(); CHECK(visible->isChecked() == document->grid().visible());
    auto* snap = control<QCheckBox>("ViewGridSnap");
    snap->click(); CHECK(snap->isChecked() == document->grid().snap());
    document->grid().toggleSnap(); CHECK(snap->isChecked() == document->grid().snap());
    control<QComboBox>("ViewGridUnits")->setCurrentIndex(1); CHECK(pref(Preferences::GridUnitSystem) == 1);
    auto* shading = control<QCheckBox>("ViewGridShading");
    shading->click(); CHECK(pref(Preferences::ShadeGrid));
    setPref(Preferences::ShadeFaces, false);
    PreferenceManager::instance().preferenceDidChangeNotifier(Preferences::ShadeFaces.path());
    CHECK_FALSE(shading->isEnabled());
    setPref(Preferences::ShadeFaces, true);
    PreferenceManager::instance().preferenceDidChangeNotifier(Preferences::ShadeFaces.path());
    CHECK(shading->isEnabled());
}

TEST_CASE_METHOD(ViewEditorTest, "View settings retain their page when rebuilt and restore grid defaults", "[view-editor]") {
    newMap();
    auto* tabs = control<QTabWidget>("ViewSettingsTabs");
    REQUIRE(tabs->count() == 3);
    CHECK(tabs->widget(1)->findChild<QWidget*>("ViewSettingsGeometry"));
    tabs->setCurrentIndex(1);
    newMap();
    CHECK(control<QTabWidget>("ViewSettingsTabs")->currentIndex() == 1);
    const auto alpha = TemporarilySetPref{Preferences::GridAlpha, 0.71f};
    const auto width = TemporarilySetPref{Preferences::GridLineWidth, 1.71f};
    const auto units = TemporarilySetPref{Preferences::GridUnitSystem, 1};
    control<QPushButton>("ViewSettingsReset")->click();
    CHECK(pref(Preferences::GridAlpha) == Preferences::GridAlpha.defaultValue());
    CHECK(pref(Preferences::GridLineWidth) == Preferences::GridLineWidth.defaultValue());
    CHECK(pref(Preferences::GridUnitSystem) == Preferences::GridUnitSystem.defaultValue());
    CHECK(control<SliderWithLabel>("ViewGridOpacity")->value() == qRound(100*pref(Preferences::GridAlpha)));
    // The tool can also be created after a map has already loaded.
    ViewEditor another(document);
    CHECK(another.findChild<QTabWidget*>("ViewSettingsTabs"));
}

TEST_CASE("Main toolbar additions reuse registered actions and available icons", "[view-editor]") {
    class Visitor : public MenuVisitor {
    public:
        std::map<std::filesystem::path, const Action*> actions;
        bool separator = true;
        void visit(const Menu& menu) override { menu.visitEntries(*this); }
        void visit(const MenuSeparatorItem&) override { CHECK_FALSE(separator); separator = true; }
        void visit(const MenuActionItem& item) override {
            CHECK(actions.emplace(item.action().preferencePath(), &item.action()).second);
            separator = false;
        }
    } visitor;
    const auto& manager = ActionManager::instance();
    manager.visitToolBarActions(visitor);
    CHECK_FALSE(visitor.separator);
    for (const auto* path : {"Menu/Edit/Group", "Menu/Edit/Ungroup", "Menu/View/Camera/Focus on Selection",
                             "Menu/View/Isolate", "Menu/View/Hide", "Menu/View/Show All",
                             SaveSelectionAsPrefabActionPath, "Menu/View/Show Prefab Browser"}) {
        CAPTURE(path);
        REQUIRE(visitor.actions.count(path) == 1);
        const auto* action = visitor.actions.at(path);
        CHECK(action == manager.actionsMap().at(path).get());
        REQUIRE(action->hasIcon());
        CHECK_FALSE(IO::loadSVGIcon(action->iconPath(), 24).isNull());
    }
}
}
