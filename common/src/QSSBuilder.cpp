#include "QSSBuilder.h"
#include "Logger.h"
#include "TrenchBroomApp.h"

#include "IO/PathQt.h"
#include "IO/SystemPaths.h"

namespace TrenchBroom {

QSSBuilder *QSSBuilder::fromFile(const std::filesystem::path &path) {
    if (auto file = QFile{IO::pathAsQString(path)}; file.exists()) {
        auto *builder = new QSSBuilder;

        // closed automatically by destructor
        file.open(QFile::ReadOnly | QFile::Text);
        builder->setText(QTextStream{&file}.readAll());

        if (!builder->m_text.isNull() && !builder->m_text.isEmpty()) {
            defaultQtLogger.info() << "Created dynamic Qt stylesheet builder from template: " << path.stem();
            LOG.debug() << "";
            return builder;
        } else {
            defaultQtLogger.error() << "Unable to load Qt dynamic stylesheets from: " << path;
        }
    }

    defaultQtLogger.warn() << "Unable to create Qt stylesheet builder from the given QSS template file: " << path.filename();
    return nullptr;
}

const QString &QSSBuilder::getText() const {
    return m_text;
}

void QSSBuilder::setText(const QString &text) {
    QSSBuilder::m_text = text;
}

const QString &QSSBuilder::getRenderedText() const {
    return m_renderedText;
}

void QSSBuilder::update() {
    // keep original text untouched
    m_renderedText = m_text;

    for (const auto &item : repl_table) {
        const auto source = REPLACEMENT_TOKEN + item.first;
        std::function<QString()> repl = item.second;
        const auto replacement = repl();

        if (!replacement.isEmpty()) {
            m_renderedText.replace(source, replacement);
            defaultQtLogger.trace() << "replaced placeholder »" << std::setw(40) << source << "« -> " << replacement << "";
        }
    }
}

void QSSBuilder::addReplacement(QString symbol, std::function<QString()> repl_f) {
    repl_table[symbol] = repl_f;
}
} // namespace TrenchBroom
