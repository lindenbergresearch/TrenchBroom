#include "ContextOverlayTemplate.h"

#include <stdexcept>

namespace TrenchBroom::View {
namespace {
QVariant lookup(const QString& key, const std::vector<QVariant>& scopes) {
    if (key == ".") return scopes.back();
    const auto path = key.split('.');
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        const auto map = it->toMap();
        if (!map.contains(path.front())) continue;
        auto value = map.value(path.front());
        for (int i = 1; i < path.size(); ++i) value = value.toMap().value(path[i]);
        return value;
    }
    return {};
}

bool truthy(const QVariant& value) {
    if (!value.isValid() || value.isNull()) return false;
    if (value.type() == QVariant::List) return !value.toList().empty();
    if (value.type() == QVariant::Map) return !value.toMap().empty();
    if (value.type() == QVariant::String) return !value.toString().isEmpty();
    return value.toBool();
}
} // namespace

ContextOverlayTemplate::ContextOverlayTemplate(const QString& source) {
    int offset = 0;
    m_parts = parse(source, offset, {}, 0);
}

std::vector<ContextOverlayTemplate::Part> ContextOverlayTemplate::parse(
    const QString& source, int& offset, const QString& section, const int depth) {
    if (depth > 32) throw std::runtime_error("Overlay template nesting exceeds 32 levels");
    std::vector<Part> parts;
    while (offset < source.size()) {
        const auto start = source.indexOf("{{", offset);
        if (start == -1) {
            parts.push_back({'t', source.mid(offset), {}});
            offset = source.size();
            break;
        }
        if (start > offset) parts.push_back({'t', source.mid(offset, start - offset), {}});
        const auto end = source.indexOf("}}", start + 2);
        if (end == -1) throw std::runtime_error("Unclosed overlay template tag");
        const auto tag = source.mid(start + 2, end - start - 2).trimmed();
        offset = end + 2;
        if (tag.isEmpty()) throw std::runtime_error("Empty overlay template tag");
        const auto kind = tag.front();
        if (kind == '!') continue;
        if (kind == '/') {
            if (section.isEmpty() || tag.mid(1).trimmed() != section)
                throw std::runtime_error("Mismatched overlay template section");
            return parts;
        }
        if (kind == '#' || kind == '^') {
            const auto name = tag.mid(1).trimmed();
            if (name.isEmpty()) throw std::runtime_error("Empty overlay template section");
            parts.push_back({kind, name, parse(source, offset, name, depth + 1)});
        } else {
            if (kind == '{' || kind == '&' || kind == '>' || kind == '=')
                throw std::runtime_error("Unsupported overlay template tag (use escaped values and sections)");
            parts.push_back({'v', tag, {}});
        }
    }
    if (!section.isEmpty()) throw std::runtime_error("Unclosed overlay template section");
    return parts;
}

QString ContextOverlayTemplate::render(const std::vector<Part>& parts, std::vector<QVariant>& scopes) {
    QString result;
    for (const auto& part : parts) {
        if (part.kind == 't') result += part.value;
        else {
            const auto value = lookup(part.value, scopes);
            if (part.kind == 'v') result += value.toString().toHtmlEscaped();
            else if (part.kind == '^') {
                if (!truthy(value)) result += render(part.children, scopes);
            } else if (value.type() == QVariant::List) {
                for (const auto& item : value.toList()) {
                    scopes.push_back(item);
                    result += render(part.children, scopes);
                    scopes.pop_back();
                }
            } else if (truthy(value)) {
                scopes.push_back(value);
                result += render(part.children, scopes);
                scopes.pop_back();
            }
        }
    }
    return result;
}

QString ContextOverlayTemplate::render(const QVariantMap& data) const {
    std::vector<QVariant> scopes{data};
    return render(m_parts, scopes);
}
} // namespace TrenchBroom::View
