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

#include "View/MultiPaneMapView.h"

#include <memory>

namespace TrenchBroom
{
class Logger;
}

namespace TrenchBroom::Renderer
{
class MapRenderer;
}

namespace TrenchBroom::View
{
class CyclingMapView;
class GLContextManager;
class MapDocument;
class MapViewToolBox;

class OnePaneMapView : public MultiPaneMapView
{
private:
  Logger* m_logger;
  std::weak_ptr<MapDocument> m_document;

  CyclingMapView* m_mapView = nullptr;

public:
  explicit OnePaneMapView(
    std::weak_ptr<MapDocument> document,
    MapViewToolBox& toolBox,
    Renderer::MapRenderer& mapRenderer,
    GLContextManager& contextManager,
    Logger* logger,
    QWidget* parent = nullptr);

private:
  void createGui(
    MapViewToolBox& toolBox,
    Renderer::MapRenderer& mapRenderer,
    GLContextManager& contextManager);

private: // implement MultiPaneMapView subclassing interface
  void doMaximizeView(MapView* view) override;
  void doRestoreViews() override;
};
} // namespace TrenchBroom::View
