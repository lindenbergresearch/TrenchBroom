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

#include "ShaderProgram.h"

#include "Ensure.h"
#include "Error.h"
#include "Renderer/Shader.h"
#include "Renderer/ShaderManager.h"

#include <kdl/result.h>

#include <vm/forward.h>
#include <vm/mat.h>
#include <vm/vec.h>

#include <memory>
#include <string>

namespace TrenchBroom::Renderer {
ShaderProgram::ShaderProgram(std::string name, const GLuint programId) : m_name{std::move(name)}, m_programId{programId} { assert(m_programId != 0); }

ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept : m_name{std::move(other.m_name)}, m_programId{std::exchange(other.m_programId, 0)} {}

ShaderProgram::~ShaderProgram() {
    if (m_programId != 0) {
        glAssert(glDeleteProgram(m_programId));
        m_programId = 0;
    }
}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept {
    if (this == &other) return *this;
    if (m_programId != 0)
        glAssert(glDeleteProgram(m_programId));

    m_name = std::move(other.m_name);
    m_programId = std::exchange(other.m_programId, 0);
    m_variableCache.clear();
    m_attributeCache.clear();
    m_vec4ArrayCache.clear();
    m_intCache.clear();

    return *this;
}

void ShaderProgram::attach(const Shader& shader) const {
    assert(m_programId != 0);
    shader.attach(m_programId);
}

namespace {
std::string getInfoLog(const GLuint programId) {
    auto infoLogLength = GLint{};
    glAssert(glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &infoLogLength));
    if (infoLogLength > 0) {
        auto infoLog = std::string{};
        infoLog.resize(size_t(infoLogLength));

        glAssert(glGetProgramInfoLog(programId, infoLogLength, &infoLogLength, infoLog.data()));
        return infoLog;
    }

    return "Unknown error";
}
} // namespace

Result<void> ShaderProgram::link() {
    m_variableCache.clear();
    m_attributeCache.clear();
    m_vec4ArrayCache.clear();
    m_intCache.clear();
    glAssert(glLinkProgram(m_programId));

    auto linkStatus = GLint(0);
    glAssert(glGetProgramiv(m_programId, GL_LINK_STATUS, &linkStatus));

    if (linkStatus == 0) {
        return Error{
            "Could not link shader program '" + m_name + "': " + getInfoLog(m_programId)
        };
    }

    return kdl::void_success;
}

void ShaderProgram::activate(ShaderManager& shaderManager) {
    assert(m_programId != 0);

    glAssert(glUseProgram(m_programId));
    assert(checkActive());

    shaderManager.setCurrentProgram(this);
}

void ShaderProgram::deactivate(ShaderManager& shaderManager) {
    glAssert(glUseProgram(0));

    shaderManager.setCurrentProgram(nullptr);
}

void ShaderProgram::set(const std::string& name, const bool value) { return set(name, int(value)); }

void ShaderProgram::set(const std::string& name, const int value) {
    assert(checkActive());
    const auto location = findUniformLocation(name);
    const auto [it, inserted] = m_intCache.try_emplace(location, value);
    if (inserted || it->second != value) {
        glAssert(glUniform1i(location, value));
        it->second = value;
    }
}

void ShaderProgram::set(const std::string& name, const size_t value) { set(name, int(value)); }

void ShaderProgram::set(const std::string& name, const float value)  {
    assert(checkActive());
    glAssert(glUniform1f(findUniformLocation(name), value));
}

void ShaderProgram::set(const std::string& name, const double value) {
    // All shaders target GLSL 1.20 and use float uniforms. glUniform1d both
    // mismatches those uniforms and requires an unavailable GL 4.x entry point
    // on legacy contexts (including macOS compatibility contexts).
    set(name, static_cast<float>(value));
}

void ShaderProgram::set(const std::string& name, const vm::vec2f& value) const {
    assert(checkActive());
    glAssert(glUniform2f(findUniformLocation(name), value.x(), value.y()));
}

void ShaderProgram::set(const std::string& name, const vm::vec3f& value) const {
    assert(checkActive());
    glAssert(glUniform3f(findUniformLocation(name), value.x(), value.y(), value.z()));
}

void ShaderProgram::set(const std::string& name, const vm::vec4f& value) const {
    assert(checkActive());
    glAssert(glUniform4f(findUniformLocation(name), value.x(), value.y(), value.z(), value.w()));
}

void ShaderProgram::setVec4Array(const std::string& name, const std::vector<vm::vec4f>& values) {
    assert(checkActive());
    if (values.empty()) return;
    auto& cached = m_vec4ArrayCache[name];
    if (cached == values) return;
    glAssert(glUniform4fv(findUniformLocation(name), static_cast<GLsizei>(values.size()), values.front().v));
    cached = values;
}

void ShaderProgram::set(const std::string& name, const vm::mat2x2f& value) const {
    assert(checkActive());
    glAssert(glUniformMatrix2fv(findUniformLocation(name), 1, false, reinterpret_cast<const float *>(value.v)));
}

void ShaderProgram::set(const std::string& name, const vm::mat3x3f& value) const {
    assert(checkActive());
    glAssert(glUniformMatrix3fv(findUniformLocation(name), 1, false, reinterpret_cast<const float *>(value.v)));
}

void ShaderProgram::set(const std::string& name, const vm::mat4x4f& value) const {
    assert(checkActive());
    glAssert(glUniformMatrix4fv(findUniformLocation(name), 1, false, reinterpret_cast<const float *>(value.v)));
}

GLint ShaderProgram::findAttributeLocation(const std::string& name) const {
    auto it = m_attributeCache.find(name);
    if (it == std::end(m_attributeCache)) {
        auto index = GLint(0);
        glAssert(index = glGetAttribLocation(m_programId, name.c_str()));
        ensure(index != -1, "Attribute location found in shader program");

        auto inserted = false;
        std::tie(it, inserted) = m_attributeCache.emplace(name, index);
        assert(inserted);
    }

    return it->second;
}

GLint ShaderProgram::findUniformLocation(const std::string& name) const {
    auto it = m_variableCache.find(name);
    if (it == std::end(m_variableCache)) {
        auto index = GLint(0);
        glAssert(index = glGetUniformLocation(m_programId, name.c_str()));
        ensure(index != -1, "Attribute location found in shader program");

        auto inserted = false;
        std::tie(it, inserted) = m_variableCache.emplace(name, index);
        assert(inserted);
    }

    return it->second;
}

bool ShaderProgram::checkActive() const {
    auto currentProgramId = GLint(-1);
    glAssert(glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgramId));
    return GLuint(currentProgramId) == m_programId;
}

Result<ShaderProgram> createShaderProgram(std::string name) {
    auto programId = GLuint(0);
    glAssert(programId = glCreateProgram());

    if (programId == 0) { return Error{"Could not create shader '" + name + "'"}; }

    return ShaderProgram{std::move(name), programId};
}
} // namespace TrenchBroom::Renderer
