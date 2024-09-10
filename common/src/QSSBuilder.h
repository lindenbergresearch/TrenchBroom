#pragma once

#include <QFile>

#include <functional>
#include <map>

namespace TrenchBroom {

static const char *const REPLACEMENT_TOKEN = "#";

/**
 * Simple dynamic Qt Style-Sheet Builder.
 * Replaces template-style placeholder with given replacement functions for
 * creating dynamic qss scripts.
 */
struct QSSBuilder {

  public:
    /**
     * Creates a build from a given qss file.
     * @param path The path to the qss file.
     * @return A new instance of a QSS Builder or null if not successfully.
     */
    static QSSBuilder *fromFile(const std::filesystem::path &path);

    void update();

    /**
     * Returns the raw input text. (template)
     * @return
     */
    const QString &getText() const;

    /**
     * Sets the input text. (template)
     * @param text QString with qss sources.
     */
    void setText(const QString &text);

    /**
     * Adds a replacement function for transforming a placeholder to a concrete value.
     * @param symbol The name of the placeholder without format token.
     * @param repl_f The replacement function which returns a QString.
     */
    void addReplacement(QString symbol, std::function<QString()> repl_f);

    /**
     * Returns the final rendered text.
     *
     * @return Result as QString.
     */
    const QString &getRenderedText() const;

  private:
    QString text;
    QString renderedText;
    std::map<QString, std::function<QString()>> repl_table;
};
} // namespace TrenchBroom
