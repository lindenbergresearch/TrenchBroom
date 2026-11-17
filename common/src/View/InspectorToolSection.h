#pragma once

#include <QString>

class QWidget;

namespace TrenchBroom::View {
struct InspectorToolSection {
    QString id;
    QString title;
    QWidget* widget;
    QWidget* dragHandle;
    int stretch = 1;
};
}
