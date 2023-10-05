#include "QSSBuilder.h"
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
        builder->text = QTextStream{&file}.readAll();

        return builder;
    }
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

    for (const auto &item: repl_table) {
        auto source = REPLACEMENT_TOKEN + item.first + REPLACEMENT_TOKEN;
        std::function<QString()> repl = item.second;
        auto replacement = repl();

        if (!replacement.isEmpty()) {
            printf("%s -> %s\n", source.toStdString().c_str(), replacement.toStdString().c_str());
            renderedText.replace(source, replacement);
        }
    }
    printf("%s\n", renderedText.toStdString().c_str());
}

void QSSBuilder::addReplacement(QString symbol, std::function<QString()> repl_f) {
    repl_table[symbol] = repl_f;
}
}
