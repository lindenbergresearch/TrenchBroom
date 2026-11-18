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
#include "FloatType.h"
#include <string>
#include <vector>
#include <vm/bbox.h>
#include <vm/mat.h>
#include <vm/vec.h>

namespace TrenchBroom::View {
// The active component tool owns its handles. The document only borrows this
// interface during activation, so ordinary object commands can target them too.
class BrushComponentSelection {
  public:
    virtual ~BrushComponentSelection() = default;
    virtual std::vector<vm::vec3> selectedVertexPositions() const = 0;
    virtual bool transformSelection(const std::string& commandName, const vm::mat4x4& transformation) = 0;
};
} // namespace TrenchBroom::View
