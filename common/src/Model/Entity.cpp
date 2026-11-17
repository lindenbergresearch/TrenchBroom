/*
 Copyright (C) 2020 Kristian Duske

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

#include "Entity.h"

#include "Assets/EntityDefinition.h"
#include "Assets/EntityModel.h"
#include "Assets/ModelDefinition.h"
#include "Assets/PropertyDefinition.h"
#include "EL/ELExceptions.h"
#include "Model/EntityProperties.h"
#include "Model/EntityPropertiesVariableStore.h"
#include "Model/EntityRotation.h"

#include "kdl/reflection_impl.h"
#include "kdl/string_utils.h"
#include "kdl/vector_utils.h"

#include "vm/bbox.h"
#include "vm/mat.h"
#include "vm/mat_ext.h"
#include "vm/vec.h"
#include "vm/vec_io.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <locale>
#include <sstream>

namespace TrenchBroom::Model {

void setDefaultProperties(const EntityPropertyConfig &propertyConfig, const Assets::EntityDefinition &entityDefinition, Entity &entity, const SetDefaultPropertyMode mode) {
    for (const auto &propertyDefinition : entityDefinition.propertyDefinitions()) {
        if (const auto defaultValue = Assets::PropertyDefinition::defaultValue(*propertyDefinition);
            !defaultValue.empty()) {
            const auto hasProperty = entity.hasProperty(propertyDefinition->key());
            if (mode == SetDefaultPropertyMode::SetAll || (mode == SetDefaultPropertyMode::SetExisting && hasProperty) || (mode == SetDefaultPropertyMode::SetMissing && !hasProperty)) {
                entity.addOrUpdateProperty(propertyConfig, propertyDefinition->key(), defaultValue);
            }
        }
    }
}

kdl_reflect_impl(Entity);

const vm::bbox3 Entity::DefaultBounds = vm::bbox3{8.0};

Entity::Entity() : m_cachedProperties{
    EntityPropertyValues::NoClassname, vm::vec3{}, vm::mat4x4{}, vm::mat4x4{}} {
}

Entity::Entity(const EntityPropertyConfig &propertyConfig, std::vector<EntityProperty> properties) : m_properties{std::move(properties)} {
    updateCachedProperties(propertyConfig);
}

Entity::Entity(const EntityPropertyConfig &propertyConfig, std::initializer_list<EntityProperty> properties) : m_properties{std::move(properties)} {
    updateCachedProperties(propertyConfig);
}

const std::vector<EntityProperty> &Entity::properties() const {
    return m_properties;
}

Entity::Entity(const Entity &other) = default;

Entity::Entity(Entity &&other) = default;

Entity &Entity::operator=(const Entity &other) = default;

Entity &Entity::operator=(Entity &&other) = default;

Entity::~Entity() = default;

void Entity::setProperties(const EntityPropertyConfig &propertyConfig, std::vector<EntityProperty> properties) {
    m_properties = std::move(properties);
    updateCachedProperties(propertyConfig);
}

const std::vector<std::string> &Entity::protectedProperties() const {
    return m_protectedProperties;
}

void Entity::setProtectedProperties(std::vector<std::string> protectedProperties) {
    m_protectedProperties = std::move(protectedProperties);
}

bool Entity::pointEntity() const {
    return m_pointEntity;
}

void Entity::setPointEntity(const EntityPropertyConfig &propertyConfig, const bool pointEntity) {
    if (m_pointEntity == pointEntity) {
        return;
    }

    m_pointEntity = pointEntity;
    updateCachedProperties(propertyConfig);
}

Assets::EntityDefinition *Entity::definition() {
    return m_definition.get();
}

const Assets::EntityDefinition *Entity::definition() const {
    return m_definition.get();
}

const vm::bbox3 &Entity::definitionBounds() const {
    return definition() && definition()->type() == Assets::EntityDefinitionType::PointEntity ? static_cast<const Assets::PointEntityDefinition *>(definition())->bounds() : DefaultBounds;
}

void Entity::setDefinition(const EntityPropertyConfig &propertyConfig, Assets::EntityDefinition *definition) {
    if (m_definition.get() == definition) {
        return;
    }

    m_definition = Assets::AssetReference{definition};
    updateCachedProperties(propertyConfig);
}

const Assets::EntityModelFrame *Entity::model() const {
    return m_model;
}

void Entity::setModel(const EntityPropertyConfig &propertyConfig, const Assets::EntityModelFrame *model) {
    if (m_model == model) {
        return;
    }

    m_model = model;
    updateCachedProperties(propertyConfig);
}

namespace {
size_t modelPropertyIndex(const Entity& entity, const std::string& key) {
    if (const auto* value = entity.property(key)) {
        size_t index = 0;
        const auto result = std::from_chars(value->data(), value->data() + value->size(), index);
        if (result.ec == std::errc{} && result.ptr == value->data() + value->size()) return index;
    }
    return 0;
}

vm::vec3 propertyModelScale(const Entity& entity) {
    for (const auto* key : {"modelscale_vec", "modelscale", "scale"}) {
        const auto* value = entity.property(key);
        if (!value) continue;
        auto stream = std::istringstream(*value);
        stream.imbue(std::locale::classic());
        auto values = std::vector<FloatType>{};
        FloatType component;
        while (stream >> component) values.push_back(component);
        if (!stream.eof() || (values.size() != 1 && values.size() != 3)
            || std::any_of(values.begin(), values.end(), [](const auto v) { return !std::isfinite(v); })) continue;
        return values.size() == 1 ? vm::vec3{values[0], values[0], values[0]} : vm::vec3{values[0], values[1], values[2]};
    }
    return vm::vec3{1, 1, 1};
}
} // namespace

Assets::ModelSpecification Entity::modelSpecification() const {
    if (!pointEntity()) return {};
    if (const auto *pointDefinition = dynamic_cast<const Assets::PointEntityDefinition *>(m_definition.get())) {
        const auto variableStore = EntityPropertiesVariableStore{*this};
        auto specification = pointDefinition->modelDefinition().modelSpecification(variableStore);
        if (!specification.path.empty()) return specification;
    }
    if (const auto* modelPath = property("model")) {
        auto path = kdl::str_trim(*modelPath);
        std::replace(path.begin(), path.end(), '\\', '/');
        // BSP submodels refer to brushes in the compiled map, not external model files.
        if (!path.empty() && path.front() != '*' && path.find('\0') == std::string::npos)
            return Assets::ModelSpecification{std::filesystem::path{path}.lexically_normal(), modelPropertyIndex(*this, "skin"), modelPropertyIndex(*this, "frame")};
    }
    return {};
}

const vm::mat4x4 &Entity::modelTransformation() const {
    return m_cachedProperties.modelTransformation;
}

Assets::DecalSpecification Entity::decalSpecification() const {
    if (const auto *pointDefinition = dynamic_cast<const Assets::PointEntityDefinition *>(m_definition.get())) {
        const auto variableStore = EntityPropertiesVariableStore{*this};
        return pointDefinition->decalDefinition().decalSpecification(variableStore);
    } else {
        return Assets::DecalSpecification{};
    }
}

void Entity::unsetEntityDefinitionAndModel() {
    if (m_definition.get() == nullptr && m_model == nullptr) {
        return;
    }

    m_definition = Assets::AssetReference<Assets::EntityDefinition>{};
    m_model = nullptr;
    m_cachedProperties.rotation = entityRotation(*this);
    m_cachedProperties.modelTransformation = vm::mat4x4::identity();
}

void Entity::addOrUpdateProperty(const EntityPropertyConfig &propertyConfig, std::string key, std::string value, const bool defaultToProtected) {
    auto it = findEntityProperty(m_properties, key);
    if (it != std::end(m_properties)) {
        it->setValue(std::move(value));
    } else {
        m_properties.emplace_back(key, std::move(value));

        if (defaultToProtected && !kdl::vec_contains(m_protectedProperties, key)) {
            m_protectedProperties.push_back(std::move(key));
        }
    }
    updateCachedProperties(propertyConfig);
}

void Entity::renameProperty(const EntityPropertyConfig &propertyConfig, const std::string &oldKey, std::string newKey) {
    if (oldKey == newKey) {
        return;
    }

    const auto oldIt = findEntityProperty(m_properties, oldKey);
    if (oldIt != std::end(m_properties)) {
        if (const auto protIt = std::find(std::begin(m_protectedProperties), std::end(m_protectedProperties), oldKey);
            protIt != std::end(m_protectedProperties)) {
            m_protectedProperties.erase(protIt);
            m_protectedProperties.push_back(newKey);
        }

        const auto newIt = findEntityProperty(m_properties, newKey);
        if (newIt != std::end(m_properties)) {
            m_properties.erase(newIt);
        }

        oldIt->setKey(std::move(newKey));
        updateCachedProperties(propertyConfig);
    }
}

void Entity::removeProperty(const EntityPropertyConfig &propertyConfig, const std::string &key) {
    const auto it = findEntityProperty(m_properties, key);
    if (it != std::end(m_properties)) {
        m_properties.erase(it);
        updateCachedProperties(propertyConfig);
    }
}

void Entity::removeNumberedProperty(const EntityPropertyConfig &propertyConfig, const std::string &prefix) {
    auto it = std::begin(m_properties);
    while (it != std::end(m_properties)) {
        if (it->hasNumberedPrefix(prefix)) {
            it = m_properties.erase(it);
        } else {
            ++it;
        }
    }
    updateCachedProperties(propertyConfig);
}

bool Entity::hasProperty(const std::string &key) const {
    return findEntityProperty(m_properties, key) != std::end(m_properties);
}

bool Entity::hasProperty(const std::string &key, const std::string &value) const {
    const auto it = findEntityProperty(m_properties, key);
    return it != std::end(m_properties) && it->hasValue(value);
}

bool Entity::hasPropertyWithPrefix(const std::string &prefix, const std::string &value) const {
    return std::any_of(std::begin(m_properties), std::end(m_properties), [&](const auto &property) {
        return property.hasPrefixAndValue(prefix, value);
    });
}

bool Entity::hasNumberedProperty(const std::string &prefix, const std::string &value) const {
    return std::any_of(std::begin(m_properties), std::end(m_properties), [&](const auto &property) {
        return property.hasNumberedPrefixAndValue(prefix, value);
    });
}

const std::string *Entity::property(const std::string &key) const {
    const auto it = findEntityProperty(m_properties, key);
    return it != std::end(m_properties) ? &it->value() : nullptr;
}

std::vector<std::string> Entity::propertyKeys() const {
    return kdl::vec_transform(m_properties, [](const auto &property) { return property.key(); });
}

const std::string &Entity::classname() const {
    return m_cachedProperties.classname;
}

void Entity::setClassname(const EntityPropertyConfig &propertyConfig, const std::string &classname) {
    addOrUpdateProperty(propertyConfig, EntityPropertyKeys::Classname, classname);
}

const vm::vec3 &Entity::origin() const {
    return m_cachedProperties.origin;
}

void Entity::setOrigin(const EntityPropertyConfig &propertyConfig, const vm::vec3 &origin) {
    addOrUpdateProperty(propertyConfig, EntityPropertyKeys::Origin, kdl::str_to_string(vm::correct(origin)));
}

const vm::mat4x4 &Entity::rotation() const {
    return m_cachedProperties.rotation;
}

std::vector<EntityProperty> Entity::propertiesWithKey(const std::string &key) const {
    return kdl::vec_filter(m_properties, [&](const auto &property) { return property.hasKey(key); });
}

std::vector<EntityProperty> Entity::propertiesWithPrefix(const std::string &prefix) const {
    return kdl::vec_filter(m_properties, [&](const auto &property) { return property.hasPrefix(prefix); });
}

std::vector<EntityProperty> Entity::numberedProperties(const std::string &prefix) const {
    return kdl::vec_filter(m_properties, [&](const auto &property) {
        return property.hasNumberedPrefix(prefix);
    });
}

void Entity::transform(const EntityPropertyConfig &propertyConfig, const vm::mat4x4 &transformation) {
    if (m_pointEntity) {
        const auto offset = definitionBounds().center();
        const auto center = origin() + offset;
        const auto transformedCenter = transformation * center;
        const auto newOrigin = transformedCenter - offset;
        if (origin() != newOrigin) {
            setOrigin(propertyConfig, transformedCenter - offset);
        }
    }

    // applying rotation has side effects (e.g. normalizing "angles")
    // so only do it if there is actually some rotation.
    const auto rotation = vm::strip_translation(transformation);
    if (rotation != vm::mat4x4::identity()) {
        // applyRotation does not read the origin, so it's ok that it's already updated now
        applyRotation(propertyConfig, rotation);
    }
}

void Entity::applyRotation(const EntityPropertyConfig &propertyConfig, const vm::mat4x4 &rotation) {
    if (propertyConfig.updateAnglePropertyAfterTransform) {
        applyEntityRotation(*this, propertyConfig, rotation);
    }
}

namespace {
auto parseOrigin(const std::string *str) {
    if (!str) {
        return vm::vec3::zero();
    }

    const auto parsed = vm::parse<FloatType, 3>(*str);
    if (!parsed || vm::is_nan(*parsed)) {
        return vm::vec3::zero();
    }

    return *parsed;
}
} // namespace

void Entity::updateCachedProperties(const EntityPropertyConfig &propertyConfig) {
    const auto *classnameValue = property(EntityPropertyKeys::Classname);
    const auto *originValue = property(EntityPropertyKeys::Origin);

    // order is important here because EntityRotation::getRotation accesses classname
    m_cachedProperties.classname = classnameValue ? *classnameValue : EntityPropertyValues::NoClassname;
    m_cachedProperties.origin = parseOrigin(originValue);
    m_cachedProperties.rotation = entityRotation(*this);

    const auto* pointDefinition = dynamic_cast<const Assets::PointEntityDefinition*>(m_definition.get());
    if (pointEntity() && (pointDefinition || hasProperty("model"))) {
        const auto variableStore = EntityPropertiesVariableStore{*this};
        const auto emptyDefinition = Assets::ModelDefinition{};
        const auto& modelDefinition = pointDefinition ? pointDefinition->modelDefinition() : emptyDefinition;
        auto scale = Assets::safeGetModelScale(modelDefinition, variableStore, propertyConfig.defaultModelScaleExpression);
        if (!propertyConfig.defaultModelScaleExpression) {
            try {
                if (modelDefinition.modelSpecification(variableStore).path.empty()) scale = propertyModelScale(*this);
            } catch (const EL::Exception&) { /* An invalid FGD expression is reported by the model loader. */ }
        }
        m_cachedProperties.modelTransformation = vm::translation_matrix(origin()) * rotation() * vm::scaling_matrix(scale);
    } else {
        m_cachedProperties.modelTransformation = vm::mat4x4::identity();
    }
}

} // namespace TrenchBroom::Model
