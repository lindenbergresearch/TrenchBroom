#include "View/Theme/QSSBuilder.h"

#include "IO/PathQt.h"

#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

namespace TrenchBroom {
bool QSSBuilder::loadFile(const std::filesystem::path& path) {
    m_changed = false;
    QFile file{IO::pathAsQString(path)};
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_errors = QStringList{QStringLiteral("%1: %2").arg(file.fileName(), file.errorString())};
        return false;
    }
    // Preserve QTextStream's BOM detection (UTF-8/UTF-16), including existing
    // personal stylesheets written by editors on other platforms.
    QTextStream stream{&file};
    const auto text = stream.readAll();
    if (file.error() != QFileDevice::NoError) {
        m_errors = QStringList{file.errorString()};
        return false;
    }
    setText(text, file.fileName());
    return true;
}

void QSSBuilder::setText(const QString& text, const QString& source) {
    m_text = text;
    m_source = source;
}

bool QSSBuilder::update(const ThemeValues& values) {
    m_errors.clear();
    m_changed = false;
    if (m_hasCompiled && m_text == m_compiledSource) {
        auto same = true;
        for (auto it = m_dependencies.cbegin(); it != m_dependencies.cend(); ++it) {
            const auto value = values.constFind(it.key());
            if (value == values.cend() || *value != it.value()) {
                same = false;
                break;
            }
        }
        if (same) return true;
    }
    ++m_compilationCount;
    auto output = QString{};
    output.reserve(m_text.size());
    auto dependencies = ThemeValues{};
    auto inComment = false;
    QChar quote;
    auto inRule = false;
    auto inValue = false;
    auto inUrl = false;
    const auto report = [&](const int position, const QString& error) {
        const auto prefix = m_text.left(position);
        const auto line = prefix.count('\n') + 1;
        const auto column = position - prefix.lastIndexOf('\n');
        m_errors.push_back(QStringLiteral("%1:%2:%3: %4").arg(m_source).arg(line).arg(column).arg(error));
    };
    const auto expand = [&](const QString& name, const int position) {
        const auto value = values.constFind(name);
        if (value == values.cend()) {
            report(position, QStringLiteral("Unknown theme value '%1'").arg(name));
            return;
        }
        if (!value->isValid()) {
            report(position, QStringLiteral("Invalid theme value '%1'").arg(name));
            return;
        }
        dependencies.insert(name, *value);
        output += value->toStyleSheet(!quote.isNull());
    };
    static const auto namePattern = QRegularExpression{QStringLiteral("^[a-z][A-Za-z0-9_]*(?:\\.[A-Za-z][A-Za-z0-9_]*)+$")};
    static const auto legacyPattern = QRegularExpression{QStringLiteral("^[A-Z][A-Z0-9_]*$")};
    static const auto hexPattern = QRegularExpression{QStringLiteral("^(?:[0-9A-Fa-f]{3}|[0-9A-Fa-f]{4}|[0-9A-Fa-f]{6}|[0-9A-Fa-f]{8})$")};
    for (auto i = 0; i < m_text.size();) {
        const auto c = m_text[i];
        if (inComment) {
            if (m_text.midRef(i, 2) == "*/") {
                output += "*/";
                i += 2;
                inComment = false;
            } else
                output += m_text[i++];
            continue;
        }
        if (quote.isNull() && m_text.midRef(i, 2) == "/*") {
            output += "/*";
            i += 2;
            inComment = true;
            continue;
        }
        if (quote.isNull() && m_text.midRef(i, 4).compare(QStringLiteral("url("), Qt::CaseInsensitive) == 0) {
            output += m_text.mid(i, 4);
            i += 4;
            inUrl = true;
            continue;
        }
        if (c == '\\' && !quote.isNull() && i + 1 < m_text.size()) {
            output += m_text.mid(i, 2);
            i += 2;
            continue;
        }
        if (m_text.midRef(i, 2) == "${") {
            const auto end = m_text.indexOf('}', i + 2);
            if (end < 0) {
                report(i, "Unclosed theme placeholder");
                break;
            }
            const auto name = m_text.mid(i + 2, end - i - 2);
            if (!namePattern.match(name).hasMatch())
                report(i, QStringLiteral("Invalid theme name '%1'").arg(name));
            else
                expand(name, i);
            i = end + 1;
            continue;
        }
        if (c == '#') {
            auto end = i + 1;
            while (end < m_text.size() && (m_text[end].isLetterOrNumber() || m_text[end] == '_' || m_text[end] == '-'))
                ++end;
            const auto legacy = m_text.mid(i + 1, end - i - 1);
            const auto alias = ThemeManager::legacyNames().constFind(legacy);
            if (alias != ThemeManager::legacyNames().cend() && inValue && !inUrl) {
                expand(*alias, i);
                i = end;
                continue;
            }
            if (inValue && !inUrl && legacyPattern.match(legacy).hasMatch() && !hexPattern.match(legacy).hasMatch())
                report(i, QStringLiteral("Unknown legacy theme placeholder '#%1'").arg(legacy));
            output += m_text.mid(i, end - i);
            i = end;
            continue;
        }
        if (c == '"' || c == '\'') {
            if (quote.isNull())
                quote = c;
            else if (quote == c)
                quote = {};
        } else if (quote.isNull()) {
            if (c == ')') inUrl = false;
            if (c == '{') {
                inRule = true;
                inValue = false;
            } else if (c == '}') {
                inRule = false;
                inValue = false;
            } else if (c == ':' && inRule)
                inValue = true;
            else if (c == ';')
                inValue = false;
        }
        output += c;
        ++i;
    }
    if (inComment) report(m_text.size(), "Unclosed stylesheet comment");
    if (!quote.isNull()) report(m_text.size(), "Unclosed stylesheet string");
    if (!m_errors.isEmpty()) return false;
    m_changed = output != m_renderedText;
    m_renderedText = std::move(output);
    m_compiledSource = m_text;
    m_dependencies = std::move(dependencies);
    m_hasCompiled = true;
    return true;
}
} // namespace TrenchBroom
