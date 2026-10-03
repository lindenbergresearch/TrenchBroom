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

#include "Macros.h"
#include "Renderer/GL.h"
#include "Result.h"

#include <vm/forward.h>
#include <vm/vec.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace TrenchBroom::Renderer {
class ShaderManager;

class Shader;

class ShaderProgram {
    using UniformVariableCache = std::unordered_map<std::string, GLint>;
    using AttributeLocationCache = std::unordered_map<std::string, GLint>;

    std::string m_name;
    GLuint m_programId;

    mutable UniformVariableCache m_variableCache;
    mutable AttributeLocationCache m_attributeCache;
    std::unordered_map<std::string, std::vector<vm::vec4f>> m_vec4ArrayCache;
    std::unordered_map<GLint, GLint> m_intCache;

public:
    ShaderProgram(std::string name, GLuint programId);
    ShaderProgram(ShaderProgram&& other) noexcept;
    ~ShaderProgram();

    ShaderProgram& operator=(ShaderProgram&& other) noexcept;
    deleteCopy(ShaderProgram);

    void attach(const Shader& shader) const;
    Result<void> link();
    void activate(ShaderManager& shaderManager);
    static void deactivate(ShaderManager& shaderManager);

    void set(const std::string& name, bool value);
    void set(const std::string& name, int value);
    void set(const std::string& name, size_t value);
    void set(const std::string& name, float value);
    void set(const std::string& name, double value);
    void set(const std::string& name, const vm::vec2f& value) const;
    void set(const std::string& name, const vm::vec3f& value) const;
    void set(const std::string& name, const vm::vec4f& value) const;
    void setVec4Array(const std::string& name, const std::vector<vm::vec4f>& values);
    void set(const std::string& name, const vm::mat2x2f& value) const;
    void set(const std::string& name, const vm::mat3x3f& value) const;
    void set(const std::string& name, const vm::mat4x4f& value) const;

    GLint findAttributeLocation(const std::string& name) const;

private:
    GLint findUniformLocation(const std::string& name) const;
    bool checkActive() const;
};

Result<ShaderProgram> createShaderProgram(std::string name);
} // namespace TrenchBroom::Renderer
