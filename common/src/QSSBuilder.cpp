#include "QSSBuilder.h"
#include "Logger.h"

#include "IO/DiskIO.h"
#include "IO/PathInfo.h"
#include "IO/PathQt.h"
#include "IO/SystemPaths.h"

namespace TrenchBroom {

QSSBuilder *QSSBuilder::fromFile(const std::filesystem::path &path) {
    if (auto file = QFile{IO::pathAsQString(path)}; file.exists()) {
        QSSBuilder *builder = new QSSBuilder;

        // closed automatically by destructor
        file.open(QFile::ReadOnly | QFile::Text);
        builder->setText(QTextStream{&file}.readAll());

        if (!builder->text.isNull() || !builder->text.isEmpty()) {
            defaultQtLogger.info() << "Created builder from file: " << path;
            return builder;
        }
    }

    defaultQtLogger.warn() << "Unable to create builder from file: " << path;
    return nullptr;
}

const QString &QSSBuilder::getText() const {
    return text;
}

void QSSBuilder::setText(const QString &text) {
    QSSBuilder::text = text;
}

const QString &QSSBuilder::getRenderedText() const {
    return renderedText;
}

void QSSBuilder::update() {
    // keep original text untouched
    renderedText = text;

    for (const auto &item : repl_table) {
        const auto source = REPLACEMENT_TOKEN + item.first;
        std::function<QString()> repl = item.second;
        const auto replacement = repl();

        if (!replacement.isEmpty()) {
            renderedText.replace(source, replacement);
            defaultQtLogger.trace() << "replaced: '" << source << "' with: '" << replacement << "'";
        }
    }
}

void QSSBuilder::addReplacement(QString symbol, std::function<QString()> repl_f) {
    repl_table[symbol] = repl_f;
}
} // namespace TrenchBroom
