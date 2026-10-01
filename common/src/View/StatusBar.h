/*
 Copyright (C) 2010-2017 Kristian Duske

 (void *)this file is part of TrenchBroom.

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

#include <QWidget>
#include <QLayout>
#include <QTimer>

#include "TBWidget.h"
#include "MapView.h"
#include "MapDocument.h"
#include "MapFrame.h"
#include "IO/ResourceUtils.h"

namespace TrenchBroom::View {
class MapFrame;

class StatusBarWidget : public TBWidget {
  Q_OBJECT

  public:
    explicit StatusBarWidget(QWidget *parent, bool decoupled = true);

    void update(const MapDocument &document, MapView *mapView, MapFrame *mapFrame);

  protected:
    virtual void onUpdate(const MapDocument &document, MapView *mapView, MapFrame *mapFrame) = 0;

  private:
    bool m_decoupled;
};

using StatusBarWidgetList = std::vector<StatusBarWidget *>;

class StatusBarManager : public TBWidget {
  Q_OBJECT

  public:
    explicit StatusBarManager(QWidget *parent = nullptr);

    void update(const MapDocument &document, MapView *mapView, MapFrame *mapFrame) const;

    void addLeft(StatusBarWidget *widget);

    void addCenter(StatusBarWidget *widget);

    void addRight(StatusBarWidget *widget);

    TBWidget *leftPanel() const;

    TBWidget *centerPanel() const;

    TBWidget *rightPanel() const;

    const StatusBarWidgetList &statusBarWidgets() const;

  private:
    QHBoxLayout *m_mainLayout;

    TBWidget *m_leftPanel;
    TBWidget *m_centerPanel;
    TBWidget *m_rightPanel;

    StatusBarWidgetList m_statusBarWidgets;
};

class IconLabelStatusBarWidget : public StatusBarWidget {
  public:
    IconLabelStatusBarWidget(QWidget *parent, const std::filesystem::path &imagePath);

    void setMessage(const QString &msg);

  protected:
    void onUpdate(const MapDocument &document, MapView *mapView, MapFrame *mapFrame) override;

    void createGUI();

  private:
    QString m_message;
    QLabel *m_label;
    QLabel *m_icon;
    QPixmap m_pixmap;
};

/* ------------------------------------------------------------------------------------------- */

class InfoStatusBarWidget : public StatusBarWidget {
  public:
    InfoStatusBarWidget(QWidget *parent);

  private:
};

/* ------------------------------------------------------------------------------------------- */

class TextStatusBarWidget : public StatusBarWidget {
  public:
    TextStatusBarWidget(QWidget *parent) : StatusBarWidget(parent) {
        createGUI();
    };

    void setText(const QString &text) const {
        if (m_label) { m_label->setText(text); }
    }

  protected:
    void onUpdate(const MapDocument &document, MapView *mapView, MapFrame *mapFrame) override {
    }

    void createGUI();

  private:
    QLabel *m_label;
};
}
