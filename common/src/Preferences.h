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

#pragma once

#include "Color.h"
#include "Preference.h"

#include <vecmath/util.h>

#include <filesystem>
#include <vector>

namespace TrenchBroom {
namespace Preferences {
// NOTE: any QKeySequence preferences must be functions like CameraFly*
// because QKeySequence docs specify that you can't create an instance before QApplication

// NOTE: When adding a new preference here, always update the staticPreferences()
// implementation

enum LengthUnitDisplay {
  Units,    /* common units used by Quake (standard) */
  Metric,   /* metric system used for interfacing 3rd party systems and engines */
  Combined  /* combined */
};


extern Preference<int> MapViewLayout;

QString systemTheme();

QString darkTheme();

extern Preference<QString> Theme;

/* --- VIEW ------------------------------------------ */
extern Preference<bool> ShowFocusIndicator;
extern Preference<float> ViewFrameWidth;
extern Preference<Color> SoftMapBoundsColor;
extern Preference<Color> BackgroundColor;
extern Preference<Color> PointFileColor;
extern Preference<Color> PortalFileBorderColor;
extern Preference<Color> PortalFileFillColor;
extern Preference<bool> ShowFPS;

extern Preference<float> TextRendererMaxDistance;
extern Preference<float> TextRendererFadeOutFactor;

/* --- AXIS ------------------------------------------ */
extern Preference<bool> ShowAxes;
extern Preference<float> AxisLength;
extern Preference<Color> XAxisColor;
extern Preference<Color> YAxisColor;
extern Preference<Color> ZAxisColor;
Preference<Color> &axisColor(vm::axis::type axis);

/* --- UNITS CONVERSATION ---------------------------- */
extern Preference<int> LengthUnitSystem;
extern Preference<float> MetricConversationFactor;
extern Preference<int> UnitsMaxDigits;

/* --- COMPASS---------------------------------------- */
extern Preference<Color> CompassBackgroundColor;
extern Preference<Color> CompassBackgroundOutlineColor;
extern Preference<float> CompassTransparency;
extern Preference<float> CompassScale;

extern Preference<Color> CameraFrustumColor;
extern Preference<Color> DefaultGroupColor;
extern Preference<Color> LinkedGroupColor;
extern Preference<Color> TutorialOverlayTextColor;
extern Preference<Color> TutorialOverlayBackgroundColor;
extern Preference<Color> UndefinedEntityColor;

/* --- FACES ----------------------------------------- */
extern Preference<Color> FaceColor;
extern Preference<Color> SelectedFaceColor;
extern Preference<Color> LockedFaceColor;
extern Preference<float> TransparentFaceAlpha;

/* --- EDGES ----------------------------------------- */
extern Preference<Color> EdgeColor;
extern Preference<Color> SelectedEdgeColor;
extern Preference<float> EdgeLineWidth;
extern Preference<float> EdgeSelectedLineWidth;
extern Preference<Color> LockedEdgeColor;
extern Preference<float> OccludedSelectedEdgeAlpha;
extern Preference<Color> LockedEdgeColor;

/* --- SELECTION BOUNDS ------------------------------ */
extern Preference<Color> SelectionBoundsColor;
extern Preference<Color> SelectionBoundsPointColor;
extern Preference<float> SelectionBoundsAlpha;
extern Preference<float> SelectionBoundsLineWidth;
extern Preference<float> SelectionBoundsPointSize;
extern Preference<int> SelectionBoundsPattern;
extern Preference<bool> SelectionBoundsIntersectionMode;
extern Preference<bool> SelectionBoundsDashedLines;
extern Preference<bool> ShowHiddenSelectionBounds;
extern Preference<bool> AlwaysShowSelectionBounds;
extern Preference<bool> ShowObjectBoundsSelectionBounds;
extern Preference<bool> SelectionBoundsShowMinMax;
extern Preference<int> SelectionBoundsDashedSize;


extern Preference<Color> InfoOverlayTextColor;
extern Preference<Color> GroupInfoOverlayTextColor;
extern Preference<Color> InfoOverlayBackgroundColor;
extern Preference<float> WeakInfoOverlayBackgroundAlpha;
extern Preference<Color> SelectedInfoOverlayTextColor;
extern Preference<Color> SelectedInfoOverlayBackgroundColor;
extern Preference<Color> LockedInfoOverlayTextColor;
extern Preference<Color> LockedInfoOverlayBackgroundColor;

extern Preference<float> HandleRadius;
extern Preference<float> MaximumHandleDistance;
extern Preference<Color> HandleColor;
extern Preference<Color> OccludedHandleColor;
extern Preference<Color> SelectedHandleColor;
extern Preference<Color> OccludedSelectedHandleColor;

extern Preference<Color> ClipHandleColor;
extern Preference<Color> ClipFaceColor;

extern Preference<Color> ExtrudeHandleColor;
extern Preference<float> RotateHandleRadius;
extern Preference<Color> RotateHandleColor;

extern Preference<Color> ScaleHandleColor;
extern Preference<Color> ScaleFillColor;
extern Preference<Color> ScaleOutlineColor;
extern Preference<float> ScaleOutlineDimAlpha;
extern Preference<Color> ShearFillColor;
extern Preference<Color> ShearOutlineColor;

extern Preference<Color> MoveTraceColor;
extern Preference<Color> OccludedMoveTraceColor;

extern Preference<Color> MoveIndicatorOutlineColor;
extern Preference<Color> MoveIndicatorFillColor;

extern Preference<Color> AngleIndicatorColor;

extern Preference<Color> TextureSeamColor;

extern Preference<float> Brightness;
extern Preference<int> FaceAutoBrightness;

/* --- GRID ------------------------------------------ */
extern Preference<float> GridLineWidth;
extern Preference<float> GridAlpha;
extern Preference<float> GridMajorDivisionSize;
extern Preference<int> GridUnitSystem;
extern Preference<Color> GridColor2D;

extern Preference<int> TextureMinFilter;
extern Preference<int> TextureMagFilter;
extern Preference<bool> EnableMSAA;
extern Preference<bool> TextureLock;
extern Preference<bool> UVLock;

/* --- FONTS ----------------------------------------- */
extern Preference<std::filesystem::path> RendererFontPath;
extern Preference<std::filesystem::path> UIFontPath;
extern Preference<std::filesystem::path> ConsoleFontPath;
extern Preference<int> RendererFontSize;
extern Preference<int> BrowserFontSize;
extern Preference<int> UIFontSize;
extern Preference<int> ConsoleFontSize;

extern Preference<int> ToolBarIconsSize;

extern Preference<Color> BrowserTextColor;
extern Preference<Color> BrowserSubTextColor;
extern Preference<Color> BrowserBackgroundColor;
extern Preference<Color> BrowserGroupBackgroundColor;
extern Preference<float> TextureBrowserIconSize;
extern Preference<Color> TextureBrowserDefaultColor;
extern Preference<Color> TextureBrowserSelectedColor;
extern Preference<Color> TextureBrowserUsedColor;

/* --- UI COLORS  ------------------------------------ */
extern Preference<Color> UIHighlightColor;
extern Preference<Color> UIWindowTintColor;
extern Preference<Color> UITextColor;
extern Preference<float> UIBrightness;

/* --- LOGGING COLORS -------------------------------- */
extern Preference<Color> LogDebugColor;
extern Preference<Color> LogInfoColor;
extern Preference<Color> LogWarningColor;
extern Preference<Color> LogErrorColor;

/* --- CAMERA ---------------------------------------- */
extern Preference<float> CameraLookSpeed;
extern Preference<bool> CameraLookInvertH;
extern Preference<bool> CameraLookInvertV;
extern Preference<float> CameraPanSpeed;
extern Preference<bool> CameraPanInvertH;
extern Preference<bool> CameraPanInvertV;
extern Preference<float> CameraMoveSpeed;
extern Preference<bool> CameraMouseWheelInvert;
extern Preference<bool> CameraEnableAltMove;
extern Preference<bool> CameraAltMoveInvert;
extern Preference<bool> CameraMoveInCursorDir;
extern Preference<float> CameraFov;
extern Preference<float> CameraFlyMoveSpeed;
extern Preference<bool> Link2DCameras;

static constexpr auto MinCameraFlyMoveSpeed = 0.1f;
static constexpr auto MaxCameraFlyMoveSpeed = 10.0f;

extern Preference<QKeySequence> &CameraFlyForward();

extern Preference<QKeySequence> &CameraFlyBackward();

extern Preference<QKeySequence> &CameraFlyLeft();

extern Preference<QKeySequence> &CameraFlyRight();

extern Preference<QKeySequence> &CameraFlyUp();

extern Preference<QKeySequence> &CameraFlyDown();

/* --- MAP VIEW--------------------------------------- */
extern Preference<bool> ShowEntityClassnames;
extern Preference<bool> ShowGroupBounds;
extern Preference<bool> ShowBrushEntityBounds;
extern Preference<bool> ShowPointEntityBounds;
extern Preference<bool> ShowPointEntityModels;

QString faceRenderModeTextured();

QString faceRenderModeFlat();

QString faceRenderModeSkip();

extern Preference<QString> FaceRenderMode;
extern Preference<bool> ShadeFaces;
extern Preference<bool> ShowFog;
extern Preference<bool> ShowEdges;
extern Preference<bool> ShowSoftMapBounds;

/* --- EDITOR CONTEXT -------------------------------- */
extern Preference<bool> ShowPointEntities;
extern Preference<bool> ShowBrushes;

QString entityLinkModeAll();

QString entityLinkModeTransitive();

QString entityLinkModeDirect();

QString entityLinkModeNone();

extern Preference<QString> EntityLinkMode;

/**
 * Returns all Preferences declared in this file. Needed for migrating preference formats
 * or if we wanted to do a Path to Preference lookup.
 */
const std::vector<PreferenceBase *> &staticPreferences();

const std::map<std::filesystem::path, PreferenceBase *> &staticPreferencesMap();

/**
 * Returns the subset of staticPreferences() that are key sequences, used by
 * dump-shortcuts.
 */
std::vector<Preference<QKeySequence> *> keyPreferences();

extern DynamicPreferencePattern<QString> GamesPath;
extern DynamicPreferencePattern<QString> GamesToolPath;
extern DynamicPreferencePattern<QString> GamesDefaultEngine;
extern DynamicPreferencePattern<QKeySequence> FiltersTagsToggle;
extern DynamicPreferencePattern<QKeySequence> TagsEnable;
extern DynamicPreferencePattern<QKeySequence> TagsDisable;
extern DynamicPreferencePattern<QKeySequence> FiltersEntitiesToggleVisible;
extern DynamicPreferencePattern<QKeySequence> EntitiesCreate;

const std::vector<DynamicPreferencePatternBase *> &dynaimcPreferencePatterns();
} // namespace Preferences
} // namespace TrenchBroom
