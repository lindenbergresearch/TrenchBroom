#include "View/Actions/Actions.h"

#include "PreferenceManager.h"
#include "Preferences.h"
#include "View/App/MapFrame.h"
#include "View/Document/MapDocument.h"
#include "View/MapView/MapViewBase.h"

namespace TrenchBroom::View {
namespace {

constexpr auto filterPrefix = "Controls/Map view/View Filter > ";
constexpr auto view3D = ActionContext::View3D | ActionContext::AnyOrNoTool | ActionContext::AnyOrNoSelection;
constexpr auto objectSelection = ActionContext::AnyView | ActionContext::AnyOrNoTool | ActionContext::NodeSelection;
constexpr auto anySelection = ActionContext::AnyView | ActionContext::AnyOrNoTool | ActionContext::AnySelection;

constexpr auto hasMap = [](ActionExecutionContext& context) {
    return context.hasDocument();
};

} // namespace

void ActionManager::createDisplayActions() {
    const auto toggle = [this](const char* suffix, const QString& label, Preference<bool>& setting,
                            const std::filesystem::path& icon = {}, ActionContext::Type scope = ActionContext::Any,
                            const QKeySequence& shortcut = {}, bool (*available)() = nullptr) {
        m_actionStatePreferences.insert(setting.path());
        createAction(std::string(filterPrefix) + suffix, label, scope, shortcut,
            [&setting](ActionExecutionContext&) { togglePref(setting); },
            [available](ActionExecutionContext& context) { return hasMap(context) && (!available || available()); },
            [&setting](ActionExecutionContext& context) { return hasMap(context) && pref(setting); }, icon);
    };
    toggle("Toggle show entity classnames", QObject::tr("Toggle Show Entity Classnames"), Preferences::ShowEntityClassnames);
    toggle("Toggle show group bounds", QObject::tr("Toggle Show Group Bounds"), Preferences::ShowGroupBounds);
    toggle("Toggle show brush entity bounds", QObject::tr("Toggle Show Brush Entity Bounds"), Preferences::ShowBrushEntityBounds);
    toggle("Toggle show point entity bounds", QObject::tr("Toggle Show Point Entity Bounds"), Preferences::ShowPointEntityBounds);
    toggle("Toggle show point entities", QObject::tr("Toggle Show Point Entities"), Preferences::ShowPointEntities);
    toggle("Toggle show point entity models", QObject::tr("Toggle Show Point Entity Models"), Preferences::ShowPointEntityModels);
    toggle("Toggle show brushes", QObject::tr("Toggle Show Brushes"), Preferences::ShowBrushes);
    toggle("Shade faces", QObject::tr("Toggle Shade Faces"), Preferences::ShadeFaces);
    toggle("Use fog", QObject::tr("Toggle Show Fog"), Preferences::ShowFog);
    toggle("Show edges", QObject::tr("Toggle Show Edges"), Preferences::ShowEdges);
    toggle("Use Lightning", QObject::tr("Use lightning (experimental)"), Preferences::EnableLightning, {},
        ActionContext::Any, QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_L));

    toggle("Toggle sky", QObject::tr("Toggle Sky"), Preferences::ShowSky, "landscape.svg", view3D);
    toggle("Toggle soft map bounds", QObject::tr("Toggle Soft Map Bounds"), Preferences::ShowSoftMapBounds, "border_outer.svg");
    toggle("Toggle axes", QObject::tr("Toggle Axes"), Preferences::ShowAxes, "3d_2.svg");
    toggle("Toggle viewport information", QObject::tr("Toggle Viewport Information"), Preferences::ShowContextOverlay, "TextMessage.svg");
    toggle("Toggle FPS", QObject::tr("Toggle FPS Counter"), Preferences::ShowFPS, "show_chart.svg");
    toggle("Toggle object bounds", QObject::tr("Toggle Object Bounds"), Preferences::ShowObjectBoundsSelectionBounds, "view_in_ar.svg");
    toggle("Toggle selection guides", QObject::tr("Toggle Selection Guides"), Preferences::SelectionBoundsIntersectionMode, "open_with.svg");
    toggle("Toggle guide distances", QObject::tr("Toggle Selection Guide Distances"), Preferences::SelectionBoundsShowDistances, "straighten.svg",
        ActionContext::Any, {}, [] { return pref(Preferences::SelectionBoundsIntersectionMode); });
    toggle("Toggle grid shading", QObject::tr("Toggle Grid Shading"), Preferences::ShadeGrid, "grid_4x4.svg",
        ActionContext::Any, {}, [] { return pref(Preferences::ShadeFaces); });

    m_actionStatePreferences.insert(Preferences::EnableAnisotropicFilter.path());
    createAction(std::string(filterPrefix) + "Use anisotropy filter", QObject::tr("Enable Anisotropy Filtering"),
        ActionContext::Any, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A),
        [](ActionExecutionContext& context) { context.view()->toggleAnisotropy(); }, hasMap,
        [](ActionExecutionContext& context) { return hasMap(context) && pref(Preferences::EnableAnisotropicFilter); });

    const auto mode = [this](const char* suffix, const QString& label, Preference<QString>& setting, const QString& value) {
        m_actionStatePreferences.insert(setting.path());
        createAction(std::string(filterPrefix) + suffix, label, ActionContext::Any, {},
            [&setting, value](ActionExecutionContext&) { setPref(setting, value); }, hasMap,
            [&setting, value](ActionExecutionContext& context) { return hasMap(context) && pref(setting) == value; });
    };
    mode("Show textures", QObject::tr("Show Textures"), Preferences::FaceRenderMode, Preferences::faceRenderModeTextured());
    mode("Hide textures", QObject::tr("Hide Textures"), Preferences::FaceRenderMode, Preferences::faceRenderModeFlat());
    mode("Hide faces", QObject::tr("Hide Faces"), Preferences::FaceRenderMode, Preferences::faceRenderModeSkip());
    createAction(ToggleTexturesCommandPath, QObject::tr("Toggle Textures"), ActionContext::Any, {},
        [](ActionExecutionContext&) {
            setPref(Preferences::FaceRenderMode, pref(Preferences::FaceRenderMode) == Preferences::faceRenderModeTextured()
                ? Preferences::faceRenderModeFlat() : Preferences::faceRenderModeTextured());
        }, hasMap, [](ActionExecutionContext& context) {
            return hasMap(context) && pref(Preferences::FaceRenderMode) == Preferences::faceRenderModeTextured();
        }, std::filesystem::path("view_module.svg"));

    mode("Show all entity links", QObject::tr("Show All Entity Links"), Preferences::EntityLinkMode, Preferences::entityLinkModeAll());
    mode("Show transitively selected entity links", QObject::tr("Show Transitively Selected Entity Links"),
        Preferences::EntityLinkMode, Preferences::entityLinkModeTransitive());
    mode("Show directly selected entity links", QObject::tr("Show Directly Selected Entity Links"),
        Preferences::EntityLinkMode, Preferences::entityLinkModeDirect());
    mode("Hide entity links", QObject::tr("Hide All Entity Links"), Preferences::EntityLinkMode, Preferences::entityLinkModeNone());

    for (const auto* setting : {&Preferences::TextureLock, &Preferences::UVLock, &Preferences::Link2DCameras})
        m_actionStatePreferences.insert(setting->path());
    m_actionStatePreferences.insert(Preferences::MapViewLayout.path());
}

void ActionManager::createSelectionActions() {
    createAction(RenameGroupsCommandPath, QObject::tr("Rename Groups"), objectSelection, {},
        [](ActionExecutionContext& context) { context.frame()->renameSelectedGroups(); },
        [](ActionExecutionContext& context) { return hasMap(context) && context.frame()->canRenameSelectedGroups(); });

    createAction(HideSelectedLayersCommandPath, QObject::tr("Hide Layers of Selection"), objectSelection, {},
        [](ActionExecutionContext& context) { context.document()->hideLayers(context.selectedLayers()); },
        [](ActionExecutionContext& context) {
            return hasMap(context) && context.document()->canHideLayers(context.selectedLayers());
        }, std::filesystem::path("visibility_off.svg"));
    createAction(IsolateSelectedLayersCommandPath, QObject::tr("Isolate Layers of Selection"), objectSelection, {},
        [](ActionExecutionContext& context) { context.document()->isolateLayers(context.selectedLayers()); },
        [](ActionExecutionContext& context) {
            return hasMap(context) && !context.selectedLayers().empty() && context.document()->canIsolateLayers(context.selectedLayers());
        }, std::filesystem::path("Layer.svg"));
    createAction(SelectAllInSelectedLayersCommandPath, QObject::tr("Select All in Layers of Selection"), objectSelection, {},
        [](ActionExecutionContext& context) { context.document()->selectAllInLayers(context.selectedLayers()); },
        [](ActionExecutionContext& context) {
            return hasMap(context) && !context.selectedLayers().empty() && context.document()->canSelectAllInLayers(context.selectedLayers());
        });
    createAction("Menu/View/Layers/Make Layer of Selection Active", QObject::tr("Make Layer of Selection Active"), objectSelection, {},
        [](ActionExecutionContext& context) { context.document()->setCurrentLayer(context.selectedLayers().front()); },
        [](ActionExecutionContext& context) {
            return hasMap(context) && context.selectedLayers().size() == 1
                && context.document()->canSetCurrentLayer(context.selectedLayers().front());
        });
    createAction("Menu/Edit/Move Selection to Active Layer", QObject::tr("Move Selection to Active Layer"), objectSelection, {},
        [](ActionExecutionContext& context) { context.document()->moveSelectionToLayer(context.document()->currentLayer()); },
        [](ActionExecutionContext& context) {
            return hasMap(context) && context.document()->canMoveSelectionToLayer(context.document()->currentLayer());
        });
    createAction("Menu/View/Reveal Selected Texture", QObject::tr("Reveal Selected Texture in Browser"), anySelection, {},
        [](ActionExecutionContext& context) { context.frame()->revealTexture(); },
        [](ActionExecutionContext& context) { return hasMap(context) && context.frame()->canRevealTexture(); },
        std::filesystem::path("Search.svg"));
    createAction("Menu/View/Show Selection Source", QObject::tr("Show Selection Source…"), ActionContext::Any, {},
        [](ActionExecutionContext& context) { context.frame()->showSelectionSource(); },
        [](ActionExecutionContext& context) { return hasMap(context) && context.frame()->canShowSelectionSource(); },
        std::filesystem::path("Source.svg"));
}

void ActionManager::createInspectorActions() {
    const auto showTool = [this](const char* tool, const char* command, const QString& label, const std::filesystem::path& icon) {
        const auto id = QString::fromLatin1(tool);
        createAction(command, label, ActionContext::Any, {},
            [id](ActionExecutionContext& context) { context.frame()->revealInspectorTool(id); }, hasMap, icon);
    };
    showTool("history", "Menu/View/Show History", QObject::tr("Show History"), "history_restore.svg");
    showTool("search", "Menu/View/Show Map Search", QObject::tr("Show Map Search"), "Search.svg");
    showTool("texture-browser", "Menu/View/Show Texture Browser", QObject::tr("Show Texture Browser"), "view_module.svg");
    showTool("entity-definitions", "Menu/View/Show Entity Definitions", QObject::tr("Show Entity Definitions"), "Entity.svg");
    showTool("mods", "Menu/View/Show Mods", QObject::tr("Show Mods"), "Folder.svg");
    createAction(ShowViewSettingsCommandPath, QObject::tr("Show View Settings"), ActionContext::Any, {},
        [](ActionExecutionContext& context) { context.frame()->showViewSettings(); }, hasMap,
        std::filesystem::path("ViewPreferences.svg"));
}

} // namespace TrenchBroom::View
