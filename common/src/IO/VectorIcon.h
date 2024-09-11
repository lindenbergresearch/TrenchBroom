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

#include <map>

namespace TrenchBroom::IO {

/* ------------------------------------------------------------------------------------------- */

class SVGIcon;

static const char *const SVG_ON_POSTFIX = "_on.svg";
static const char *const SVG_OFF_POSTFIX = "_off.svg";


/**
 *
 */
class IconCache {
  private:
    /** icon cache */
    static std::map<QString, SVGIcon *> iconCache;

  public:
    /**
     * Test if the given icon is already cached.
     * @param path Path to the source filename.
     * @return True if cached, false otherwise.
     */
    static bool isCached(const QString &path);


    /**
     * Pushed a new icon to cache.
     * @param path The associated source file path.
     * @param icon The icon to cache.
     */
    static void push(const QString &path, SVGIcon *icon);


    /**
     * Pulls an icon from cache.
     * @param path The associated source file path.
     * @return An instance of SVGIcon if found in cache.
     */
    static SVGIcon *pop(const QString &path);

    /**
     * Resets the comlete cache.
     */
    static void resetCache();

};

/* ------------------------------------------------------------------------------------------- */

/**
 * Type of the specific icon resource.
 */
enum IconVariant {
  Single,
  IconOn,
  IconOff
};

/**
 * Scalable Vector Graphics based icon class.
 */
class SVGIcon {
  public:
    SVGIcon(const std::filesystem::path &path);

    static SVGIcon *fromPath(const std::filesystem::path &path);

    const QIcon &icon() const;

    const QList<QSize> &sizes() const;

    void setSizes(const QList<QSize> &Sizes);

    double targetBrightness() const;

    void setTargetBrightness(double TargetBrightness);

    double adjustmentFactor() const;

    void setAdjustmentFactor(double AdjustmentFactor);

    QString iconPath(const IconVariant variant) const;

    bool grayscale();

    bool normalized();

    bool isVariant() const;

    bool isSingle() const;

    bool isValid() const;

    void setGrayscale(bool m_grayscale);

    void setNormalized(bool m_normalized);

  private:
    void renderVariant(const IconVariant variant);

    double calculateAverageBrightness(const QImage &image);

    QImage normalizeBrightness(const QImage &image);

    QImage convertToGrayscale(const QImage &image);

    static QString filePathToString(const std::filesystem::path &imagePath);

    const std::filesystem::path &m_path;

    QIcon m_icon{};
    QList<QSize> m_sizes{};

    bool m_grayscale = false;
    bool m_normalized = false;

    double m_targetBrightness = 128.0;
    double m_adjustmentFactor = 1.0;
};

}
