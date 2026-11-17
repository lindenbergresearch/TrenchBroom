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

#include "ToolBox.h"

#include <QDateTime>
#include <QDebug>

#include "View/DragTracker.h"
#include "View/DropTracker.h"
#include "View/InputState.h"
#include "View/Tool.h"
#include "View/ToolChain.h"
#include "View/ToolController.h"

#include <cassert>
#include <algorithm>
#include <kdl/vector_utils.h>
#include <string>
#include <utility>

namespace TrenchBroom::View
{
ToolBox::ToolBox() = default;
ToolBox::~ToolBox() = default;

void ToolBox::addTool(Tool& tool)
{
  m_notifierConnection += tool.refreshViewsNotifier.connect(refreshViewsNotifier);
  m_notifierConnection +=
    tool.toolHandleSelectionChangedNotifier.connect(toolHandleSelectionChangedNotifier);
}

void ToolBox::pick(
  ToolChain* chain, const InputState& inputState, Model::PickResult& pickResult)
{
  chain->pick(inputState, pickResult);
}

bool ToolBox::dragEnter(
  ToolChain* chain, const InputState& inputState, const std::string& text)
{
  if (!m_enabled || !chain->shouldAcceptDrop(inputState, text))
  {
    return false;
  }

  if (m_dropTracker)
  {
    dragLeave(chain, inputState);
  }

  deactivateAllTools();
  m_dropTracker = chain->dragEnter(inputState, text);
  return m_dropTracker != nullptr;
}

bool ToolBox::dragMove(
  ToolChain* /* chain */, const InputState& inputState, const std::string& /* text */)
{
  if (!m_enabled || !m_dropTracker)
  {
    return false;
  }

  m_dropTracker->move(inputState);
  return true;
}

void ToolBox::dragLeave(ToolChain* /* chain */, const InputState& inputState)
{
  if (!m_enabled || !m_dropTracker)
  {
    return;
  }

  m_dropTracker->leave(inputState);
  m_dropTracker = nullptr;
}

bool ToolBox::dragDrop(
  ToolChain* /* chain */, const InputState& inputState, const std::string& /* text */)
{
  if (!m_enabled || !m_dropTracker)
  {
    return false;
  }

  const auto result = m_dropTracker->drop(inputState);
  m_dropTracker = nullptr;
  return result;
}

void ToolBox::modifierKeyChange(ToolChain* chain, const InputState& inputState)
{
  if (m_enabled)
  {
    chain->modifierKeyChange(inputState);
    if (m_dragTracker)
    {
      m_dragTracker->modifierKeyChange(inputState);
    }
  }
}

void ToolBox::mouseDown(ToolChain* chain, const InputState& inputState) const
{
  if (m_enabled)
  {
    chain->mouseDown(inputState);
  }
}

void ToolBox::mouseUp(ToolChain* chain, const InputState& inputState) const
{
  if (m_enabled)
  {
    chain->mouseUp(inputState);
  }
}

bool ToolBox::mouseClick(ToolChain* chain, const InputState& inputState) const
{
  if (m_enabled)
  {
    return chain->mouseClick(inputState);
  }

  return false;
}

void ToolBox::mouseDoubleClick(ToolChain* chain, const InputState& inputState) const
{
  if (m_enabled)
  {
    chain->mouseDoubleClick(inputState);
  }
}

void ToolBox::mouseMove(ToolChain* chain, const InputState& inputState) const
{
  if (m_enabled)
  {
    chain->mouseMove(inputState);
  }
}

bool ToolBox::dragging() const
{
  return m_dragTracker != nullptr;
}

void ToolBox::startMouseDrag(ToolChain* chain, const InputState& inputState)
{
  if (m_enabled)
  {
    m_dragTracker = chain->startMouseDrag(inputState);
  }
}

bool ToolBox::mouseDrag(const InputState& inputState)
{
  assert(enabled() && dragging());
  return m_dragTracker->drag(inputState);
}

void ToolBox::endMouseDrag(const InputState& inputState)
{
  assert(enabled() && dragging());
  m_dragTracker->end(inputState);
  m_dragTracker = nullptr;
}

void ToolBox::cancelMouseDrag()
{
  assert(dragging());
  m_dragTracker->cancel();
  m_dragTracker = nullptr;
}

void ToolBox::mouseScroll(ToolChain* chain, const InputState& inputState)
{
  if (m_enabled)
  {
    if (m_dragTracker)
    {
      m_dragTracker->mouseScroll(inputState);
    }
    else
    {
      chain->mouseScroll(inputState);
    }
  }
}

bool ToolBox::cancel(ToolChain* chain)
{
  if (dragging())
  {
    cancelMouseDrag();
    return true;
  }

  if (chain->cancel())
  {
    return true;
  }

  if (anyToolActive())
  {
    deactivateTool(*m_modalToolStack.back());
    return true;
  }

  return false;
}

void ToolBox::suppressWhileActive(Tool& suppressedTool, Tool& primaryTool)
{
  assert(&primaryTool != &suppressedTool);
  m_suppressedTools[&primaryTool].push_back(&suppressedTool);
}

void ToolBox::addExclusiveToolGroup(std::initializer_list<Tool*> tools) {m_exclusiveToolGroups.emplace_back(tools);}
bool ToolBox::anyToolActive() const {return !m_modalToolStack.empty();}
Tool* ToolBox::activeTool() {return m_modalToolStack.empty()?nullptr:m_modalToolStack.back();}
void ToolBox::toggleTool(Tool& tool) {
    if(std::find(m_modalToolStack.begin(),m_modalToolStack.end(),&tool)!=m_modalToolStack.end()) deactivateTool(tool);
    else activateTool(tool);
}
void ToolBox::deactivateAllTools() {while(!m_modalToolStack.empty()) deactivateTool(*m_modalToolStack.back());}

bool ToolBox::enabled() const
{
  return m_enabled;
}

void ToolBox::enable()
{
  m_enabled = true;
}

void ToolBox::disable()
{
  assert(!dragging());
  m_enabled = false;
}

void ToolBox::setRenderOptions(
  ToolChain* chain, const InputState& inputState, Renderer::RenderContext& renderContext)
{
  chain->setRenderOptions(inputState, renderContext);
  if (m_dragTracker)
  {
    m_dragTracker->setRenderOptions(inputState, renderContext);
  }
}

void ToolBox::renderTools(
  ToolChain* chain,
  const InputState& inputState,
  Renderer::RenderContext& renderContext,
  Renderer::RenderBatch& renderBatch)
{
  chain->render(inputState, renderContext, renderBatch);
  if (m_dragTracker)
  {
    m_dragTracker->render(inputState, renderContext, renderBatch);
  }
}

std::vector<Tool*> ToolBox::currentlySuppressedTools() const {
    std::vector<Tool*> result;
    for(const auto& [primary,tools]:m_suppressedTools) if(primary->active()) result=kdl::vec_concat(std::move(result),tools);
    return kdl::vec_sort_and_remove_duplicates(std::move(result));
}
void ToolBox::activateTool(Tool& tool) {
    auto excluded=std::vector<Tool*>{};
    if(m_exclusiveToolGroups.empty()) excluded=m_modalToolStack;
    for(const auto& group:m_exclusiveToolGroups) if(std::find(group.begin(),group.end(),&tool)!=group.end())
        for(auto* other:m_modalToolStack) if(std::find(group.begin(),group.end(),other)!=group.end()) excluded.push_back(other);
    excluded=kdl::vec_sort_and_remove_duplicates(std::move(excluded));
    for(auto* other:excluded) if(other->active()) deactivateTool(*other);
    const auto previous=currentlySuppressedTools();
    if(tool.activate()) {
        for(auto* other:currentlySuppressedTools()) if(std::find(previous.begin(),previous.end(),other)==previous.end() && other->active()) {
            other->deactivate();toolDeactivatedNotifier(*other);
        }
        m_modalToolStack.push_back(&tool);toolActivatedNotifier(tool);
    }
}
void ToolBox::deactivateTool(Tool& tool) {
    if(dragging()) cancelMouseDrag();
    const auto previous=currentlySuppressedTools();
    tool.deactivate();m_modalToolStack.erase(std::remove(m_modalToolStack.begin(),m_modalToolStack.end(),&tool),m_modalToolStack.end());
    toolDeactivatedNotifier(tool);
    const auto current=currentlySuppressedTools();
    for(auto* other:previous) if(std::find(current.begin(),current.end(),other)==current.end() && !other->active()) {
        other->activate();toolActivatedNotifier(*other);
    }
}
} // namespace TrenchBroom::View
