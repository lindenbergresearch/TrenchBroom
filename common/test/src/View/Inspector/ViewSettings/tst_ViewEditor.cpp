#include "Error.h"
#include "PreferenceManager.h"
#include <kdl/result.h>
#include "TestSourcePath.h"
#include "Error.h"
#include "IO/GameConfigParser.h"
#include "IO/VectorIcon.h"
#include "IO/ResourceUtils.h"
#include "Logger.h"
#include "Model/GameImpl.h"
#include "Preferences.h"
#include "View/Actions/Actions.h"
#include "View/Document/Grid.h"
#include "View/Widgets/ColorButton.h"
#include "View/Document/MapDocument.h"
#include "View/Document/MapDocumentCommandFacade.h"
#include "View/Inspector/ViewSettings/ViewEditor.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
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
    })", testSourceRoot()
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

TEST_CASE_METHOD(ViewEditorTest, "Entity link controls persist animation and visibility settings", "[view-editor][annotations]") {
    const auto width = TemporarilySetPref{Preferences::EntityLinkWidth, 2.f};
    const auto speed = TemporarilySetPref{Preferences::EntityLinkSpeed, 48.f};
    const auto distance = TemporarilySetPref{Preferences::EntityLinkDistance, 2048.f};
    const auto opacity = TemporarilySetPref{Preferences::EntityLinkOccludedAlpha, 0.15f};
    const auto mode = TemporarilySetPref{Preferences::EntityLinkMode, Preferences::entityLinkModeDirect()};
    newMap();
    CHECK(control<QDoubleSpinBox>("ViewEntityLinkWidth")->value() == Approx(2));
    control<QDoubleSpinBox>("ViewEntityLinkWidth")->setValue(3.5);
    CHECK(pref(Preferences::EntityLinkWidth) == Approx(3.5));
    control<QDoubleSpinBox>("ViewEntityLinkSpeed")->setValue(0);
    CHECK(pref(Preferences::EntityLinkSpeed) == 0);
    control<QDoubleSpinBox>("ViewEntityLinkDistance")->setValue(0);
    CHECK(pref(Preferences::EntityLinkDistance) == 0);
    control<QDoubleSpinBox>("ViewEntityLinkOccludedAlpha")->setValue(25);
    CHECK(pref(Preferences::EntityLinkOccludedAlpha) == Approx(0.25));
    setPref(Preferences::EntityLinkMode, Preferences::entityLinkModeNone());
    PreferenceManager::instance().preferenceDidChangeNotifier(Preferences::EntityLinkMode.path());
    CHECK_FALSE(control<QDoubleSpinBox>("ViewEntityLinkSpeed")->isEnabled());
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

TEST_CASE_METHOD(ViewEditorTest, "Model animation controls persist independent triggers and update availability", "[view-editor][model-animation]") {
    const auto selectedPref = TemporarilySetPref{Preferences::AnimateSelectedEntityModels, true};
    const auto nearbyPref = TemporarilySetPref{Preferences::AnimateNearbyEntityModels, false};
    const auto distancePref = TemporarilySetPref{Preferences::EntityModelAnimationDistance, 256.f};
    const auto speedPref = TemporarilySetPref{Preferences::EntityModelAnimationSpeed, 1.5f};
    const auto visiblePref = TemporarilySetPref{Preferences::ShowPointEntities, true};
    const auto modelsPref = TemporarilySetPref{Preferences::ShowPointEntityModels, true};
    newMap();
    auto* selected = control<QCheckBox>("animateSelectedEntityModels");
    auto* nearby = control<QCheckBox>("animateNearbyEntityModels");
    auto* distance = control<SliderWithLabel>("entityModelAnimationDistance");
    auto* speed = control<SliderWithLabel>("entityModelAnimationSpeed");
    CHECK(selected->isChecked()); CHECK_FALSE(nearby->isChecked());
    CHECK(distance->value() == 256); CHECK_FALSE(distance->isEnabled());
    CHECK(speed->value() == 150); CHECK(speed->isEnabled());
    nearby->click(); CHECK(pref(Preferences::AnimateNearbyEntityModels));
    PreferenceManager::instance().preferenceDidChangeNotifier(Preferences::AnimateNearbyEntityModels.path());
    CHECK(distance->isEnabled());
    distance->setValue(768); CHECK(pref(Preferences::EntityModelAnimationDistance) == 768.f);
    speed->setValue(75); CHECK(pref(Preferences::EntityModelAnimationSpeed) == Approx(.75f));
    selected->click(); CHECK_FALSE(pref(Preferences::AnimateSelectedEntityModels));
    nearby->click(); CHECK_FALSE(pref(Preferences::AnimateNearbyEntityModels));
    PreferenceManager::instance().preferenceDidChangeNotifier(Preferences::AnimateNearbyEntityModels.path());
    CHECK_FALSE(speed->isEnabled()); CHECK_FALSE(distance->isEnabled());
    setPref(Preferences::ShowPointEntityModels, false);
    PreferenceManager::instance().preferenceDidChangeNotifier(Preferences::ShowPointEntityModels.path());
    CHECK_FALSE(selected->isEnabled()); CHECK_FALSE(nearby->isEnabled());
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

TEST_CASE_METHOD(ViewEditorTest, "Bounds controls persist independent markers range hover and distance labels", "[view-editor][annotations]") {
    const auto corners = TemporarilySetPref{Preferences::SelectionBoundsCornerMarkers, true};
    const auto animation = TemporarilySetPref{Preferences::SelectionBoundsAnimationSpeed, 1.f};
    const auto length = TemporarilySetPref{Preferences::SelectionBoundsGuideLength, 1024.f};
    const auto range = TemporarilySetPref{Preferences::SelectionBoundsGuideDistance, 0.f};
    const auto distances = TemporarilySetPref{Preferences::SelectionBoundsShowDistances, false};
    const auto font = TemporarilySetPref{Preferences::SelectionBoundsDistanceFontSize, 12};
    const auto offset = TemporarilySetPref{Preferences::SelectionBoundsDistanceOffset, 10};
    const auto position = TemporarilySetPref{Preferences::SelectionBoundsDistancePosition, 1};
    const auto hover = TemporarilySetPref{Preferences::ObjectBoundsOnHover, false};
    const auto color = TemporarilySetPref{Preferences::SelectionBoundsDistanceColor, Color{1, 1, 0, 1}};
    newMap();
    auto* speed = control<SliderWithLabel>("BoundsAnimationSpeed");
    CHECK(speed->value() == 100);
    speed->setValue(10);
    CHECK(pref(Preferences::SelectionBoundsAnimationSpeed) == Approx(0.1f));
    speed->setValue(0);
    CHECK(pref(Preferences::SelectionBoundsAnimationSpeed) == 0.f);
    setPref(Preferences::SelectionBoundsAnimationSpeed, 0.5f);
    PreferenceManager::instance().preferenceDidChangeNotifier(Preferences::SelectionBoundsAnimationSpeed.path());
    CHECK(speed->value() == 50);
    control<QCheckBox>("BoundsCornerMarkers")->click();
    CHECK_FALSE(pref(Preferences::SelectionBoundsCornerMarkers));
    control<QCheckBox>("ObjectBoundsOnHover")->click();
    CHECK(pref(Preferences::ObjectBoundsOnHover));
    control<QCheckBox>("BoundsShowDistances")->click();
    CHECK(pref(Preferences::SelectionBoundsShowDistances));
    control<QDoubleSpinBox>("BoundsGuideLength")->setValue(512);
    CHECK(pref(Preferences::SelectionBoundsGuideLength) == 512.f);
    control<QDoubleSpinBox>("BoundsGuideDistance")->setValue(2048);
    CHECK(pref(Preferences::SelectionBoundsGuideDistance) == 2048.f);
    control<QDoubleSpinBox>("BoundsDistanceFontSize")->setValue(18);
    CHECK(pref(Preferences::SelectionBoundsDistanceFontSize) == 18);
    control<QDoubleSpinBox>("BoundsDistanceOffset")->setValue(-12);
    CHECK(pref(Preferences::SelectionBoundsDistanceOffset) == -12);
    control<QComboBox>("BoundsDistancePosition")->setCurrentIndex(2);
    CHECK(pref(Preferences::SelectionBoundsDistancePosition) == 2);
    control<ColorButton>("BoundsDistanceColor")->setColor(QColor("#88ccff"));
    CHECK(pref(Preferences::SelectionBoundsDistanceColor) == Color(QColor("#88ccff")));
    control<QPushButton>("ViewSettingsReset")->click();
    CHECK(pref(Preferences::SelectionBoundsCornerMarkers) == Preferences::SelectionBoundsCornerMarkers.defaultValue());
    CHECK(pref(Preferences::SelectionBoundsAnimationSpeed) == Preferences::SelectionBoundsAnimationSpeed.defaultValue());
    CHECK(speed->value() == qRound(100 * Preferences::SelectionBoundsAnimationSpeed.defaultValue()));
    CHECK(pref(Preferences::SelectionBoundsGuideLength) == Preferences::SelectionBoundsGuideLength.defaultValue());
    CHECK(pref(Preferences::SelectionBoundsGuideDistance) == Preferences::SelectionBoundsGuideDistance.defaultValue());
    CHECK(pref(Preferences::SelectionBoundsShowDistances) == Preferences::SelectionBoundsShowDistances.defaultValue());
    CHECK(pref(Preferences::SelectionBoundsDistanceFontSize) == Preferences::SelectionBoundsDistanceFontSize.defaultValue());
    CHECK(pref(Preferences::SelectionBoundsDistanceOffset) == Preferences::SelectionBoundsDistanceOffset.defaultValue());
    CHECK(pref(Preferences::SelectionBoundsDistancePosition) == Preferences::SelectionBoundsDistancePosition.defaultValue());
    CHECK(pref(Preferences::SelectionBoundsDistanceColor) == Preferences::SelectionBoundsDistanceColor.defaultValue());
    CHECK_FALSE(control<QCheckBox>("ObjectBoundsOnHover")->isChecked());
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
