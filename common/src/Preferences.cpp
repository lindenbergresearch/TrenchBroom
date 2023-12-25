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

#include "Preferences.h"

#include <QKeySequence>

#include "View/MapViewLayout.h"

#include <vecmath/util.h>

namespace TrenchBroom {
namespace Preferences {

QString systemTheme() {
    return QStringLiteral("System");
}

QString darkTheme() {
    return QStringLiteral("Dark");
}

/* --- VIEW ------------------------------------------ */
Preference<QString> Theme("Theme", darkTheme());
Preference<int> MapViewLayout("Views/Map view layout", static_cast<int>(View::MapViewLayout::OnePane));
Preference<bool> ShowFocusIndicator("Renderer/Show focus indicator", true);
Preference<float> ViewFrameWidth("Renderer/View frame width", 2.0f);
Preference<Color> SoftMapBoundsColor("Renderer/Colors/Soft map bounds color", Color(241, 125, 37));
Preference<Color> BackgroundColor("Renderer/Colors/Background", Color(38, 38, 38));
Preference<Color> PointFileColor("Renderer/Colors/Point file", Color(0.0f, 1.0f, 0.0f, 1.0f));
Preference<Color> PortalFileBorderColor("Renderer/Colors/Portal file border", Color(1.0f, 1.0f, 1.0f, 0.5f));
Preference<Color> PortalFileFillColor("Renderer/Colors/Portal file fill", Color(1.0f, 0.4f, 0.4f, 0.2f));
Preference<bool> ShowFPS("Renderer/Show FPS", true);

Preference<float> TextRendererMaxDistance("Renderer/Maximum text visibility", 400.f);
Preference<float> TextRendererFadeOutFactor("Renderer/Text fadeout factor", 0.9f);

/* --- AXIS ------------------------------------------ */
Preference<bool> ShowAxes("Renderer/Show axes", true);
Preference<float> AxisLength("Renderer/Axis length", 999999999.0f);
Preference<Color> XAxisColor("Renderer/Colors/X axis", Color(0.8f, 0.1f, 0.2f, 1.0f));
Preference<Color> YAxisColor("Renderer/Colors/Y axis", Color(0.2f, 0.8f, 0.1f, 1.0f));
Preference<Color> ZAxisColor("Renderer/Colors/Z axis", Color(0.0f, 0.3f, 0.8f, 1.0f));

Preference<Color> &axisColor(vm::axis::type axis) {
    switch (axis) {
        case vm::axis::x:
            return Preferences::XAxisColor;
        case vm::axis::y:
            return Preferences::YAxisColor;
        case vm::axis::z:
        default:
            return Preferences::ZAxisColor;
    }
}

/* --- UNITS CONVERSATION ---------------------------- */
Preference<int> LengthUnitSystem("Renderer/Length unit system", LengthUnitDisplay::Units);
Preference<float> MetricConversationFactor("Renderer/Metric Conversation Factor", 32.0f);
Preference<int> UnitsMaxDigits("Renderer/Units Maximum Digits", 2);

/* --- COMPASS---------------------------------------- */
Preference<Color> CompassBackgroundColor("Renderer/Colors/Compass background", Color(0.5f, 0.5f, 0.5f, 0.2f));
Preference<Color> CompassBackgroundOutlineColor("Renderer/Colors/Compass background outline",
    Color(1.0f, 1.0f, 1.0f, 0.4f));
Preference<float> CompassTransparency("Renderer/Colors/Compass transparency", 0.75f);
Preference<float> CompassScale("Renderer/Colors/Compass scale", 1.5f);

Preference<Color> CameraFrustumColor("Renderer/Colors/Camera frustum", Color(0.0f, 1.0f, 1.0f, 1.0f));

Preference<Color> DefaultGroupColor("Renderer/Colors/Groups", Color(0.7f, 0.4f, 1.0f, 1.0f));
Preference<Color> LinkedGroupColor("Renderer/Colors/Linked Groups", Color(1.0f, 0.35f, 0.87f, 1.0f));

Preference<Color> TutorialOverlayTextColor("Renderer/Colors/Tutorial overlay text", Color(1.0f, 1.0f, 1.0f, 1.0f));
Preference<Color> TutorialOverlayBackgroundColor("Renderer/Colors/Tutorial overlay background",
    Color(1.0f, 0.5f, 0.0f, 0.6f));

/* --- FACES ----------------------------------------- */
Preference<Color> FaceColor("Renderer/Colors/Faces", Color(0.1f, 0.11f, 0.12f, 1.0f));
Preference<Color> SelectedFaceColor("Renderer/Colors/Selected faces", Color(1.0f, 0.1f, 0.11f, 1.0f));
Preference<Color> LockedFaceColor("Renderer/Colors/Locked faces", Color(0.5f, 0.6f, 0.1f, 1.0f));
Preference<float> TransparentFaceAlpha("Renderer/Colors/Transparent faces", 0.75f);

/* --- EDGES ----------------------------------------- */
Preference<Color> EdgeColor("Renderer/Colors/Edges", Color(1.0f, 1.0f, 1.0f, 0.35f));
Preference<Color> OccludedSelectedEdgeColor("Renderer/Colors/Occluded Selected Edges", Color(1.0f, 0.4f, 0.2f, 1.0f));
Preference<Color> SelectedEdgeColor("Renderer/Colors/Selected edges", Color(1.0f, 0.0f, 0.0f, 0.6f));
Preference<float> EdgeLineWidth("Renderer/Edge line width", 1.15f);
Preference<float> EdgeSelectedLineWidth("Renderer/Selected edge line width", 1.3f);
Preference<float> OccludedSelectedEdgeAlpha("Renderer/Colors/Occluded selected edge alpha", 0.3f);
Preference<Color> LockedEdgeColor("Renderer/Colors/Locked edges", Color(0.7f, 0.7f, 0.7f, 0.7f));
Preference<Color> UndefinedEntityColor("Renderer/Colors/Undefined entity", Color(0.9f, 0.0f, 0.0f, 1.0f));

/* --- SELECTION BOUNDS ------------------------------ */
Preference<Color> SelectionBoundsColor("Renderer/Colors/Selection bounds", Color(0.0f, 0.8f, 0.9f, 0.75f));
Preference<float> SelectionBoundsAlpha("Renderer/Colors/Selection bounds alpha", 0.6f);
Preference<float> SelectionBoundsLineWidth("Renderer/Selection bounds line width", 1.3f);
Preference<float> SelectionBoundsPointSize("Renderer/Selection bounds point size", 26.0f);
Preference<Color> SelectionBoundsPointColor("Renderer/Selection bounds point color", Color(1.0f, 0.0f, 0.0f, 1.0f));
Preference<int> SelectionBoundsPattern("Renderer/Selection bounds pattern", 0x6666);
Preference<bool> SelectionBoundsDashedLines("Renderer/Selection bounds dashed lines", true);
Preference<bool> SelectionBoundsIntersectionMode("Renderer/Selection bounds intersection mode", true);
Preference<bool> ShowHiddenSelectionBounds("Renderer/Selection bounds show hidden bounds", false);
Preference<bool> AlwaysShowSelectionBounds("Renderer/Selection bounds always show bounds", true);
Preference<bool> ShowObjectBoundsSelectionBounds("Renderer/Selection bounds show object bounds", true);
Preference<bool> SelectionBoundsShowMinMax("Renderer/Selection bounds show min max values", false);
Preference<int> SelectionBoundsDashedSize("Renderer/Selection bounds line dashes size", 2);

Preference<Color> InfoOverlayTextColor("Renderer/Colors/Info overlay text", Color(1.0f, 1.0f, 1.0f, 1.0f));
Preference<Color> GroupInfoOverlayTextColor("Renderer/Colors/Group info overlay text", Color(0.7f, 0.4f, 1.0f, 1.0f));
Preference<Color> InfoOverlayBackgroundColor("Renderer/Colors/Info overlay background", Color(0.0f, 0.0f, 0.0f, 0.6f));
Preference<float> WeakInfoOverlayBackgroundAlpha("Renderer/Colors/Weak info overlay background alpha", 0.6f);
Preference<Color> SelectedInfoOverlayTextColor("Renderer/Colors/Selected info overlay text",
    Color(1.0f, 1.0f, 1.0f, 1.0f));
Preference<Color> SelectedInfoOverlayBackgroundColor("Renderer/Colors/Selected info overlay background",
    Color(1.0f, 0.0f, 0.0f, 0.6f));
Preference<Color> LockedInfoOverlayTextColor("Renderer/Colors/Locked info overlay text",
    Color(0.35f, 0.35f, 0.6f, 1.0f));
Preference<Color> LockedInfoOverlayBackgroundColor("Renderer/Colors/Locked info overlay background",
    Color(0.0f, 0.0f, 0.0f, 0.6f));

Preference<float> HandleRadius("Controls/Handle radius", 6.0f);
Preference<float> MaximumHandleDistance("Controls/Maximum handle distance", 1000.0f);
Preference<Color> HandleColor("Renderer/Colors/Handle", Color(248, 230, 60, 1.0f));
Preference<Color> OccludedHandleColor("Renderer/Colors/Occluded handle", Color(248, 230, 60, 0.4f));
Preference<Color> SelectedHandleColor("Renderer/Colors/Selected handle", Color(1.0f, 0.0f, 0.0f, 1.0f));
Preference<Color> OccludedSelectedHandleColor("Renderer/Colors/Occluded selected handle",
    Color(1.0f, 0.0f, 0.0f, 0.4f));

Preference<Color> ClipHandleColor("Renderer/Colors/Clip handle", Color(1.0f, 0.5f, 0.0f, 1.0f));
Preference<Color> ClipFaceColor("Renderer/Colors/Clip face", Color(0.6f, 0.4f, 0.0f, 0.35f));

Preference<Color> ExtrudeHandleColor("Renderer/Colors/Resize handle", Color(248, 230, 60, 1.0f));
Preference<float> RotateHandleRadius("Controls/Rotate handle radius", 64.0f);
Preference<Color> RotateHandleColor("Renderer/Colors/Rotate handle", Color(248, 230, 60, 1.0f));

Preference<Color> ScaleHandleColor("Renderer/Colors/Scale handle", Color(77, 255, 80, 1.0f));
Preference<Color> ScaleFillColor("Renderer/Colors/Scale fill", Color(77, 255, 80, 0.125f));
Preference<Color> ScaleOutlineColor("Renderer/Colors/Scale outline", Color(77, 255, 80, 1.0f));
Preference<float> ScaleOutlineDimAlpha("Renderer/Colors/Scale outline dim alpha", 0.3f);
Preference<Color> ShearFillColor("Renderer/Colors/Shear fill", Color(45, 133, 255, 0.125f));
Preference<Color> ShearOutlineColor("Renderer/Colors/Shear outline", Color(45, 133, 255, 1.0f));

Preference<Color> MoveTraceColor("Renderer/Colors/Move trace", Color(0.0f, 1.0f, 1.0f, 1.0f));
Preference<Color> OccludedMoveTraceColor("Renderer/Colors/Move trace", Color(0.0f, 1.0f, 1.0f, 0.4f));

Preference<Color> MoveIndicatorOutlineColor("Renderer/Colors/Move indicator outline", Color(1.0f, 1.0f, 1.0f, 1.0f));
Preference<Color> MoveIndicatorFillColor("Renderer/Colors/Move indicator fill", Color(0.0f, 0.0f, 0.0f, 0.5f));

Preference<Color> AngleIndicatorColor("Renderer/Colors/Angle indicator", Color(1.0f, 1.0f, 1.0f, 1.0f));

Preference<Color> TextureSeamColor("Renderer/Colors/Texture seam", Color(1.0f, 1.0f, 0.0f, 1.0f));

Preference<float> Brightness("Renderer/Brightness", 2.0f);
Preference<int> FaceAutoBrightness("Renderer/Brightness mode", 2);

/* --- GRID ------------------------------------------ */
Preference<float> GridLineWidth("Renderer/Grid/With", 0.5);
Preference<float> GridAlpha("Renderer/Grid/Alpha", 0.6f);
Preference<float> GridMajorDivisionSize("Renderer/Grid/Major division size", 128.0f);
Preference<int> GridUnitSystem("Renderer/Grid/Units system", LengthUnitDisplay::Units);
Preference<Color> GridColor2D("Renderer/Grid/Color2D", Color(0.7f, 0.8f, 0.9f, 1.0f));

Preference<int> TextureMinFilter("Renderer/Texture mode min filter", 0x2700);
Preference<int> TextureMagFilter("Renderer/Texture mode mag filter", 0x2600);
Preference<bool> EnableMSAA("Renderer/Enable multisampling", true);

Preference<bool> TextureLock("Editor/Texture lock", true);
Preference<bool> UVLock("Editor/UV lock", false);

/* --- FONTS ----------------------------------------- */
Preference<std::filesystem::path> RendererFontPath("Renderer/Font name", "fonts/Inter-SemiBold.otf");
Preference<std::filesystem::path> UIFontPath("Editor/UI Font name", "fonts/Inter-Regular.otf");
Preference<std::filesystem::path> ConsoleFontPath("Editor/Console Font name", "fonts/JetBrainsMono-Regular.ttf");
Preference<int> RendererFontSize("Renderer/Font size", 12);
Preference<int> BrowserFontSize("Browser/Font size", 12);
Preference<int> UIFontSize("Editor/UI Font size", 13);
Preference<int> ConsoleFontSize("Editor/Console Font size", 12);

Preference<int> ToolBarIconsSize("Editor/Toolbar Icon Size", 22);

Preference<Color> BrowserTextColor("Browser/Text color", Color(1.0f, 1.0f, 1.0f, 1.0f));
Preference<Color> BrowserSubTextColor("Browser/Sub text color", Color(0.65f, 0.65f, 0.65f, 1.0f));
Preference<Color> BrowserGroupBackgroundColor("Browser/Group background color", Color(0.1f, 0.1f, 0.1f, 0.8f));
Preference<Color> BrowserBackgroundColor("Browser/Background color", Color(0.14f, 0.14f, 0.14f, 1.0f));
Preference<float> TextureBrowserIconSize("Texture Browser/Icon size", 1.0f);
Preference<Color> TextureBrowserDefaultColor("Texture Browser/Default color", Color(0.0f, 0.0f, 0.0f, 0.0f));
Preference<Color> TextureBrowserSelectedColor("Texture Browser/Selected color", Color(1.0f, 1.0f, 0.0f, 1.0f));
Preference<Color> TextureBrowserUsedColor("Texture Browser/Used color", Color(1.0f, 0.7f, 0.7f, 0.7f));

/* --- UI COLORS  ------------------------------------ */
Preference<Color> UIHighlightColor("Editor/Colors/UI Highlight Color", Color(0.221f, 0.4359f, 0.7875f));
Preference<Color> UIWindowTintColor("Editor/Colors/UI Window Color Tint", Color(0.107f, 0.117f, 0.125f));
Preference<Color> UITextColor("Editor/Colors/UI Text Color", Color(0.7f, 0.7f, 0.7f));
Preference<float> UIBrightness("Editor/Colors/UI Brightness", 1.0f);

/* --- LOGGING COLORS -------------------------------- */
Preference<Color> LogDebugColor("Editor/Colors/LogLevel Debug", Color(0.0f, 0.6f, 0.68f));
Preference<Color> LogInfoColor("Editor/Colors/LogLevel Info", Color(0.8f, 0.8f, 0.8f));
Preference<Color> LogWarningColor("Editor/Colors/LogLevel Warning", Color(0.78f, 0.72f, 0.1f));
Preference<Color> LogErrorColor("Editor/Colors/LogLevel Error", Color(0.98f, 0.12f, 0.23f));

/* --- CAMERA ---------------------------------------- */
Preference<float> CameraLookSpeed("Controls/Camera/Look speed", 0.65f);
Preference<int> CameraLookSmoothing("Controls/Camera/Look smoothing", 4);
Preference<bool> CameraLookInvertH("Controls/Camera/Invert horizontal look", false);
Preference<bool> CameraLookInvertV("Controls/Camera/Invert vertical look", false);
Preference<float> CameraPanSpeed("Controls/Camera/Pan speed", 0.5f);
Preference<bool> CameraPanInvertH("Controls/Camera/Invert horizontal pan", false);
Preference<bool> CameraPanInvertV("Controls/Camera/Invert vertical pan", false);
Preference<bool> CameraMouseWheelInvert("Controls/Camera/Invert mouse wheel", false);
Preference<float> CameraMoveSpeed("Controls/Camera/Move speed", 0.3f);
Preference<bool> CameraEnableAltMove("Controls/Camera/Use alt to move", false);
Preference<bool> CameraAltMoveInvert("Controls/Camera/Invert zoom direction when using alt to move", false);
Preference<bool> CameraMoveInCursorDir("Controls/Camera/Move camera in cursor dir", false);
Preference<float> CameraFov("Controls/Camera/Field of vision", 90.0f);
Preference<float> CameraFlyMoveSpeed("Controls/Camera/Fly move speed", 0.2f);
Preference<bool> Link2DCameras("Controls/Camera/Link 2D cameras", true);

Preference<QKeySequence> &CameraFlyForward() {
    static Preference<QKeySequence> pref("Controls/Camera/Move forward", QKeySequence('W'));
    return pref;
}

Preference<QKeySequence> &CameraFlyBackward() {
    static Preference<QKeySequence> pref("Controls/Camera/Move backward", QKeySequence('S'));
    return pref;
}

Preference<QKeySequence> &CameraFlyLeft() {
    static Preference<QKeySequence> pref("Controls/Camera/Move left", QKeySequence('A'));
    return pref;
}

Preference<QKeySequence> &CameraFlyRight() {
    static Preference<QKeySequence> pref("Controls/Camera/Move right", QKeySequence('D'));
    return pref;
}

Preference<QKeySequence> &CameraFlyUp() {
    static Preference<QKeySequence> pref("Controls/Camera/Move up", QKeySequence('Q'));
    return pref;
}

Preference<QKeySequence> &CameraFlyDown() {
    static Preference<QKeySequence> pref("Controls/Camera/Move down", QKeySequence('X'));
    return pref;
}

/* --- MAP VIEW--------------------------------------- */
Preference<bool> ShowEntityClassnames("Map view/Show entity classnames", true);
Preference<bool> ShowGroupBounds("Map view/Show group bounds", true);
Preference<bool> ShowBrushEntityBounds("Map view/Show brush entity bounds", true);
Preference<bool> ShowPointEntityBounds("Map view/Show point entity bounds", true);
Preference<bool> ShowPointEntityModels("Map view/Show point entity models", true);

QString faceRenderModeTextured() {
    return "textured";
}

QString faceRenderModeFlat() {
    return "flat";
}

QString faceRenderModeSkip() {
    return "skip";
}

// shade
Preference<bool> ShadeFaces("Map view/Shade faces", true);
Preference<float> ShadeLevel("Map view/Shade Level", 0.3f);

// fog settings
Preference<bool> ShowFog("Map view/Show Fog", true);
Preference<Color> FogColor("Map view/Fog Color", Color{155, 155, 155, 255});
Preference<float> FogMaxAmount("Map view/Fog Max Amount", 0.10f);
Preference<float> FogMinDistance("Map view/Fog Min Distance", 512.0f);
Preference<float> FogScale("Map view/Fog Scale", 0.00075f);
Preference<float> FogBias("Map view/Fog Bias", 0.0f);
Preference<int> FogType("Map view/Fog Type", 0);

Preference<QString> FaceRenderMode("Map view/Face render mode", "textured");
Preference<bool> ShowEdges("Map view/Show edges", true);
Preference<bool> ShowSoftMapBounds("Map view/Show soft map bounds", true);
Preference<bool> ShowPointEntities("Map view/Show point entities", true);
Preference<bool> ShowBrushes("Map view/Show brushes", true);

QString entityLinkModeAll() {
    return "all";
}

QString entityLinkModeTransitive() {
    return "transitive";
}

QString entityLinkModeDirect() {
    return "direct";
}

QString entityLinkModeNone() {
    return "none";
}

Preference<QString> EntityLinkMode("Map view/Entity link mode", "direct");

const std::vector<PreferenceBase *> &staticPreferences() {
    static const std::vector<PreferenceBase *> list{
        &MapViewLayout, &Theme, &ShowAxes, &BackgroundColor, &AxisLength, &XAxisColor, &YAxisColor, &ZAxisColor,
        &UnitsMaxDigits,
        &PointFileColor, &PortalFileBorderColor, &ShowObjectBoundsSelectionBounds,
        &PortalFileFillColor, &ShowFPS, &TextRendererMaxDistance, &TextRendererFadeOutFactor, &LengthUnitSystem,
        &MetricConversationFactor, &SoftMapBoundsColor, &CompassBackgroundColor,
        &CompassBackgroundOutlineColor, &CompassTransparency, &CompassScale, &CameraFrustumColor, &DefaultGroupColor,
        &TutorialOverlayTextColor,
        &TutorialOverlayBackgroundColor, &FaceColor, &SelectedFaceColor, &LockedFaceColor, &TransparentFaceAlpha,
        &EdgeColor,
        &OccludedSelectedEdgeColor,
        &FogColor,
        &FogBias,
        &FogMaxAmount,
        &FogMinDistance,
        &FogType,
        &FogScale,
        &SelectedEdgeColor,
        &ShadeLevel,
        &EdgeLineWidth, &EdgeSelectedLineWidth, &OccludedSelectedEdgeAlpha, &LockedEdgeColor, &UndefinedEntityColor,
        &SelectionBoundsColor, &AlwaysShowSelectionBounds,&SelectionBoundsDashedSize,
        &SelectionBoundsPointColor, &SelectionBoundsPointSize, &SelectionBoundsDashedLines, &ShowHiddenSelectionBounds,
        &InfoOverlayTextColor,
        &SelectionBoundsIntersectionMode, &SelectionBoundsLineWidth,&SelectionBoundsShowMinMax, &GroupInfoOverlayTextColor,
        &InfoOverlayBackgroundColor, &WeakInfoOverlayBackgroundAlpha,
        &SelectedInfoOverlayTextColor, &SelectedInfoOverlayBackgroundColor, &LockedInfoOverlayTextColor,
        &LockedInfoOverlayBackgroundColor, &HandleRadius,
        &MaximumHandleDistance, &HandleColor, &OccludedHandleColor, &SelectedHandleColor, &OccludedSelectedHandleColor,
        &ClipHandleColor, &ClipFaceColor,
        &ExtrudeHandleColor, &RotateHandleRadius, &RotateHandleColor, &ScaleHandleColor, &ScaleFillColor,
        &ScaleOutlineColor, &ScaleOutlineDimAlpha,
        &ShearFillColor, &ShearOutlineColor, &MoveTraceColor, &OccludedMoveTraceColor, &MoveIndicatorOutlineColor,
        &MoveIndicatorFillColor,
        &AngleIndicatorColor, &TextureSeamColor, &Brightness, &FaceAutoBrightness, &GridLineWidth, &GridAlpha,
        &GridMajorDivisionSize, &GridColor2D, &GridUnitSystem,
        &TextureMinFilter, &TextureMagFilter, &TextureLock, &UVLock, &RendererFontPath, &UIFontPath, &ConsoleFontPath,
        &RendererFontSize, &BrowserFontSize,
        &UIFontSize, &ConsoleFontSize, &ToolBarIconsSize, &BrowserTextColor, &BrowserSubTextColor,
        &BrowserBackgroundColor, &BrowserGroupBackgroundColor,
        &TextureBrowserIconSize, &TextureBrowserDefaultColor, &TextureBrowserSelectedColor, &TextureBrowserUsedColor,
        &UIHighlightColor, &UITextColor,
        &UIWindowTintColor, &UIBrightness, &LogInfoColor, &LogDebugColor, &LogWarningColor, &LogErrorColor,
        &CameraLookSpeed,&CameraLookSmoothing, &CameraLookInvertH,
        &CameraLookInvertV, &CameraPanSpeed, &CameraPanInvertH, &CameraPanInvertV, &CameraMouseWheelInvert,
        &CameraMoveSpeed, &CameraEnableAltMove,
        &CameraAltMoveInvert, &CameraMoveInCursorDir, &CameraFov, &CameraFlyMoveSpeed, &Link2DCameras,
        &CameraFlyForward(), &CameraFlyBackward(),
        &CameraFlyLeft(), &CameraFlyRight(), &CameraFlyUp(), &CameraFlyDown(), &ShowEntityClassnames, &ShowGroupBounds,
        &ShowBrushEntityBounds,
        &ShowPointEntityBounds, &ShowPointEntityModels, &FaceRenderMode, &ShadeFaces, &ShowFog, &ShowEdges,
        &ShowSoftMapBounds, &ShowPointEntities,
        &ShowBrushes, &EntityLinkMode
    };

    return list;
}

const std::map<std::filesystem::path, PreferenceBase *> &staticPreferencesMap() {
    static std::map<std::filesystem::path, PreferenceBase *> map;

    if (map.empty()) {
        for (PreferenceBase *pref: staticPreferences()) {
            map[pref->path()] = pref;
        }
    }

    return map;
}

std::vector<Preference<QKeySequence> *> keyPreferences() {
    std::vector<Preference<QKeySequence> *> result;

    for (PreferenceBase *pref: staticPreferences()) {
        auto *keyPref = dynamic_cast<Preference<QKeySequence> *>(pref);
        if (keyPref != nullptr) {
            result.push_back(keyPref);
        }
    }

    return result;
}

DynamicPreferencePattern<QString> GamesPath("Games/*/Path");
DynamicPreferencePattern<QString> GamesToolPath("Games/*/Tool Path/*");
DynamicPreferencePattern<QString> GamesDefaultEngine("Games/*/Default Engine");
DynamicPreferencePattern<QKeySequence> FiltersTagsToggle("Filters/Tags/*/Toggle Visible");
DynamicPreferencePattern<QKeySequence> TagsEnable("Tags/*/Enable");
DynamicPreferencePattern<QKeySequence> TagsDisable("Tags/*/Disable");
DynamicPreferencePattern<QKeySequence> FiltersEntitiesToggleVisible("Filters/Entities/*/Toggle Visible");
DynamicPreferencePattern<QKeySequence> EntitiesCreate("Entities/*/Create");

const std::vector<DynamicPreferencePatternBase *> &dynaimcPreferencePatterns() {
    static const std::vector<DynamicPreferencePatternBase *> list{
        &GamesPath, &GamesToolPath, &GamesDefaultEngine, &FiltersTagsToggle, &TagsEnable, &TagsDisable,
        &FiltersEntitiesToggleVisible, &EntitiesCreate
    };
    return list;
}
} // namespace Preferences
} // namespace TrenchBroom
