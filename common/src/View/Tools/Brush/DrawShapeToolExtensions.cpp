/*
 Copyright (C) 2023 Kristian Duske

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

#include "View/Tools/Brush/DrawShapeToolExtensions.h"
#include "View/Theme/QtUtils.h"
#include <QBoxLayout>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>
#include <QRandomGenerator>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QToolButton>

namespace TrenchBroom::View {
DrawShapeToolExtensionPage::DrawShapeToolExtensionPage(DrawShapeKind kind, DrawShapeToolParameters& p, QWidget* parent) : QWidget(parent) {
    static const std::array<const char*, 8> names{"Cuboid", "Stairs", "Arch", "Cylinder", "Cone", "UVSphere", "IcoSphere", "Rock"};
    setObjectName(QString("ShapeParameters") + names[size_t(kind)]);
    auto* layout = new QGridLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto updates = std::vector<std::function<void()>>{};
    int field = 0;
    const auto add = [&](const QString& text, QWidget* control) -> QLabel* {
        const auto row = field / 3, col = (field % 3) * 2;
        ++field;
        if (text.isEmpty()) {
            layout->addWidget(control, row, col, 1, 2);
            return nullptr;
        }
        auto* label = new QLabel(text);
        makeSmall(label);
        label->setBuddy(control);
        layout->addWidget(label, row, col);
        layout->addWidget(control, row, col + 1);
        return label;
    };
    const auto integer = [&](const QString& text, const char* name, int minimum, int maximum, auto get, auto set) {
        auto* box = new QSpinBox;
        box->setObjectName(name);
        box->setRange(minimum, maximum);
        add(text, box);
        connect(box, qOverload<int>(&QSpinBox::valueChanged), this, [set](int value) { set(value); });
        updates.emplace_back([box, get] {
            const QSignalBlocker block(box);
            box->setValue(int(get()));
        });
        return box;
    };
    const auto decimal = [&](const QString& text, const char* name, double minimum, double maximum, auto get, auto set) {
        auto* box = new QDoubleSpinBox;
        box->setObjectName(name);
        box->setRange(minimum, maximum);
        box->setDecimals(2);
        box->setSingleStep(1);
        add(text, box);
        connect(box, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [set](double value) { set(value); });
        updates.emplace_back([box, get] {
            const QSignalBlocker block(box);
            box->setValue(get());
        });
        return box;
    };
    const auto toggle = [&](const QString& text, const char* name, auto get, auto set) {
        auto* box = new QCheckBox(text);
        box->setObjectName(name);
        add({}, box);
        connect(box, &QCheckBox::toggled, this, [set](bool value) { set(value); });
        updates.emplace_back([box, get] {
            const QSignalBlocker block(box);
            box->setChecked(get());
        });
        return box;
    };
    const bool circular = kind == DrawShapeKind::Cylinder || kind == DrawShapeKind::Cone || kind == DrawShapeKind::UVSphere || kind == DrawShapeKind::Arch;
    if (circular) {
        auto* modes = new QWidget;
        auto* row = new QHBoxLayout(modes);
        row->setContentsMargins(0, 0, 0, 0);
        auto* edge = createBitmapToggleButton("CircleEdgeAligned.svg", tr("Radius to edge"));
        edge->setObjectName("ShapeCircleEdge");
        auto* vertex = createBitmapToggleButton("CircleVertexAligned.svg", tr("Radius to vertex"));
        vertex->setObjectName("ShapeCircleVertex");
        auto* scalable = createBitmapToggleButton("CircleScalable.svg", tr("Scalable circle with round ends"));
        scalable->setObjectName("ShapeCircleScalable");
        auto* group = new QButtonGroup(this);
        group->addButton(edge);
        group->addButton(vertex);
        group->addButton(scalable);
        row->addWidget(edge);
        row->addWidget(vertex);
        row->addWidget(scalable);
        row->addStretch();
        add(tr("Circle"), modes);
        auto* values = new QStackedWidget;
        auto* sides = new QSpinBox;
        auto* precision = new QSpinBox;
        sides->setObjectName("ShapeSides");
        sides->setRange(3, 256);
        precision->setObjectName("ShapePrecision");
        precision->setRange(0, 3);
        values->addWidget(sides);
        values->addWidget(precision);
        auto* label = add(tr("Sides"), values);
        connect(edge, &QToolButton::clicked, this, [&p] { p.setCircleShape(Model::convertCircleShape<Model::EdgeAlignedCircle>(p.circleShape())); });
        connect(vertex, &QToolButton::clicked, this, [&p] { p.setCircleShape(Model::convertCircleShape<Model::VertexAlignedCircle>(p.circleShape())); });
        connect(scalable, &QToolButton::clicked, this, [&p] { p.setCircleShape(Model::convertCircleShape<Model::ScalableCircle>(p.circleShape())); });
        connect(sides, qOverload<int>(&QSpinBox::valueChanged), this, [&p](int value) {
            if (std::holds_alternative<Model::EdgeAlignedCircle>(p.circleShape()))
                p.setCircleShape(Model::EdgeAlignedCircle{size_t(value)});
            else if (std::holds_alternative<Model::VertexAlignedCircle>(p.circleShape()))
                p.setCircleShape(Model::VertexAlignedCircle{size_t(value)});
        });
        connect(precision, qOverload<int>(&QSpinBox::valueChanged), this, [&p](int value) {
            if (std::holds_alternative<Model::ScalableCircle>(p.circleShape())) p.setCircleShape(Model::ScalableCircle{size_t(value)});
        });
        updates.emplace_back([=, &p] {
            const QSignalBlocker a(sides), b(precision);
            std::visit(
                [&](const auto& circle) {
                    if constexpr (requires { circle.numSides; }) {
                        sides->setValue(int(circle.numSides));
                        values->setCurrentWidget(sides);
                        label->setText(tr("Sides"));
                    } else {
                        precision->setValue(int(circle.precision));
                        values->setCurrentWidget(precision);
                        label->setText(tr("Precision"));
                    }
                },
                p.circleShape());
            edge->setChecked(std::holds_alternative<Model::EdgeAlignedCircle>(p.circleShape()));
            vertex->setChecked(std::holds_alternative<Model::VertexAlignedCircle>(p.circleShape()));
            scalable->setChecked(std::holds_alternative<Model::ScalableCircle>(p.circleShape()));
        });
    }
    if (kind == DrawShapeKind::Cylinder || kind == DrawShapeKind::Arch) {
        if (kind == DrawShapeKind::Cylinder)
            toggle(
                tr("Hollow"), "ShapeHollow", [&p] { return p.hollow(); }, [&p](bool v) { p.setHollow(v); });
        auto* box = decimal(
            tr("Thickness"), "ShapeThickness", 0.125, 65536, [&p] { return p.thickness(); }, [&p](double v) { p.setThickness(v); });
        updates.emplace_back([box, kind, &p] { box->setEnabled(kind == DrawShapeKind::Arch || p.hollow()); });
        if (kind == DrawShapeKind::Arch)
            toggle(
                tr("Fill above arch"), "ShapeArchSpandrel", [&p] { return p.createSpandrel(); }, [&p](bool v) { p.setCreateSpandrel(v); });
    }
    if (kind == DrawShapeKind::UVSphere) {
        auto* rings = integer(
            tr("Rings"), "ShapeRings", 1, 256, [&p] { return p.numRings(); }, [&p](int v) { p.setNumRings(size_t(v)); });
        updates.emplace_back([rings, &p] { rings->setEnabled(!std::holds_alternative<Model::ScalableCircle>(p.circleShape())); });
    }
    if (kind == DrawShapeKind::IcoSphere)
        integer(
            tr("Accuracy"), "ShapeAccuracy", 1, 4, [&p] { return p.accuracy(); }, [&p](int v) { p.setAccuracy(size_t(v)); });
    if (kind == DrawShapeKind::Stairs) {
        decimal(
            tr("Step height"), "ShapeStepHeight", 1, 65536, [&p] { return p.stepHeight(); }, [&p](double v) { p.setStepHeight(v); });
        auto* direction = new QComboBox;
        direction->setObjectName("ShapeStairDirection");
        direction->addItems({tr("+X"), tr("-X"), tr("+Y"), tr("-Y")});
        add(tr("Direction"), direction);
        connect(direction, qOverload<int>(&QComboBox::currentIndexChanged), this, [&p](int i) { p.setStairDirection(DrawShapeToolParameters::StairDirection(i)); });
        updates.emplace_back([direction, &p] {
            const QSignalBlocker block(direction);
            direction->setCurrentIndex(int(p.stairDirection()));
        });
    }
    if (kind == DrawShapeKind::Rock) {
        auto* type = new QComboBox;
        type->setObjectName("ShapeRockType");
        type->addItems({tr("Boulder"), tr("Shelf"), tr("Strata"), tr("Crag"), tr("Crystal"), tr("Columns"), tr("Basalt"), tr("Cluster")});
        add(tr("Formation"), type);
        connect(type, qOverload<int>(&QComboBox::currentIndexChanged), this, [&p](int i) { p.setRockType(Model::RockType(i)); });
        updates.emplace_back([type, &p] {
            const QSignalBlocker block(type);
            type->setCurrentIndex(int(p.rockType()));
        });
        auto* detail = decimal(
            tr("Detail"), "ShapeRockDetail", 0, 1, [&p] { return p.rockDetail(); }, [&p](double v) { p.setRockDetail(v); });
        detail->setSingleStep(0.05);
        auto* seed = new QWidget;
        auto* row = new QHBoxLayout(seed);
        row->setContentsMargins(0, 0, 0, 0);
        auto* value = new QSpinBox;
        value->setObjectName("ShapeRockSeed");
        value->setRange(0, int(DrawShapeToolParameters::MaxRockSeed));
        row->addWidget(value);
        auto* random = createBitmapButton("Repeat.svg", tr("Choose a new rock seed"));
        random->setObjectName("ShapeRockRandomize");
        row->addWidget(random);
        add(tr("Seed"), seed);
        connect(value, qOverload<int>(&QSpinBox::valueChanged), this, [&p](int v) { p.setRockSeed(uint32_t(v)); });
        connect(random, &QToolButton::clicked, this, [&p] { p.setRockSeed(QRandomGenerator::global()->generate() & DrawShapeToolParameters::MaxRockSeed); });
        updates.emplace_back([value, &p] {
            const QSignalBlocker block(value);
            value->setValue(int(p.rockSeed()));
        });
        toggle(
            tr("Next seed after creation"), "ShapeRockIncrementSeed", [&p] { return p.incrementRockSeed(); }, [&p](bool v) { p.setIncrementRockSeed(v); });
    }
    for (int col = 1; col < 6; col += 2)
        layout->setColumnStretch(col, 1);
    m_refresh = [updates = std::move(updates)] {
        for (const auto& update : updates)
            update();
    };
    m_connections += p.parametersDidChangeNotifier.connect([this] { m_refresh(); });
    m_refresh();
}
} // namespace TrenchBroom::View
