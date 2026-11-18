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

#include <QWidget>
#include <QColor>

#include <vector>

class QColor;

namespace TrenchBroom {
namespace View {
class ColorTable : public QWidget {
  Q_OBJECT
  Q_PROPERTY(QColor selectionColor READ selectionColor WRITE setSelectionColor)
  Q_PROPERTY(int selectionWidth READ selectionWidth WRITE setSelectionWidth)
  Q_PROPERTY(int cellSize READ cellSize WRITE setCellSize)
  Q_PROPERTY(int cellSpacing READ cellSpacing WRITE setCellSpacing)
private:
  int m_cellSize;
  int m_cellSpacing;
  QColor m_selectionColor;
  int m_selectionWidth = 1;
  std::vector<QColor> m_colors;
  std::vector<QColor> m_selectedColors;

public:
  explicit ColorTable(int cellSize, QWidget* parent = nullptr);

  void setColors(const std::vector<QColor>& colors);
  QColor selectionColor() const { return m_selectionColor.isValid() ? m_selectionColor : palette().color(QPalette::Highlight); }
  int selectionWidth() const { return m_selectionWidth; }
  int cellSize() const { return m_cellSize; }
  int cellSpacing() const { return m_cellSpacing; }
  void setSelectionColor(QColor color) {
      m_selectionColor = color;
      update();
  }
  void setSelectionWidth(int width);
  void setCellSize(int size);
  void setCellSpacing(int spacing);

  void setSelection(const std::vector<QColor>& colors);

protected: // QWidget overrides
  void paintEvent(QPaintEvent* event) override;

  void mouseReleaseEvent(QMouseEvent* event) override;

public: // QWidget overrides
  bool hasHeightForWidth() const override;

  int heightForWidth(int w) const override;

private:
  int computeCols(int width) const;

  int computeRows(int cols) const;

  int computeHeight(int rows) const;

signals:

  void colorTableSelected(QColor color);
};
} // namespace View
} // namespace TrenchBroom
