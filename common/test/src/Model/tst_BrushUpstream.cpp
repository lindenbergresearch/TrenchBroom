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

#include "Error.h"
#include "Model/Brush.h"
#include "Model/BrushFace.h"
#include "Model/BrushBuilder.h"
#include "Model/MapFormat.h"
#include "Model/Polyhedron.h"
#include "Model/Polyhedron3.h"
#include <kdl/result.h>
#include <vm/polygon.h>
#include <catch2/catch.hpp>

namespace TrenchBroom::Model {
TEST_CASE("Brush hull handles imprecise horizons", "[brush-port]") {
const auto vertices = std::vector<vm::vec3>({
        vm::vec3(-9633, 1505, -1513), vm::vec3(-9622, 1504, -1488),
        vm::vec3(-9622, 1504, -1550), vm::vec3(-9656, 1504, -1488),
        vm::vec3(-9208, 1744, -1488), vm::vec3(-9216, 1504, -1488),
        vm::vec3(-9432, 1744, -1488), vm::vec3(-9322, 1625, -1489),
        vm::vec3(-9432, 1504, -1488), vm::vec3(-9208, 1968, -1488),
        vm::vec3(-9432, 1968, -1488), vm::vec3(-9320, 1856, -1489),
        vm::vec3(-9208, 2084, -1488), vm::vec3(-9320, 2026, -1489),
        vm::vec3(-9432, 2084, -1488), vm::vec3(-9208, 2308, -1488),
        vm::vec3(-9432, 2308, -1488), vm::vec3(-9320, 2196, -1489),
        vm::vec3(-9400, 5122, -1489), vm::vec3(-9344, 5220, -1488),
        vm::vec3(-9432, 4996, -1488), vm::vec3(-9408, 5220, -1488),
      });

          const Polyhedron3 hull(vertices);
    REQUIRE(hull.polyhedron());
    CHECK(hull.closed());
    CHECK(hull.vertexCount() == 10);
    CHECK(hull.faceCount() == 14);
}

TEST_CASE("Hollow cylinders stay inside their outer bounds when filled", "[brush-port]") {
    const BrushBuilder builder(MapFormat::Standard,vm::bbox3{8192.0});
    for (const auto axis : {vm::axis::x,vm::axis::y,vm::axis::z}) {
        const auto bounds=vm::bbox3{{-32,-32,-32},{32,32,32}};
        for(const auto thickness : {16.0,32.0,64.0,128.0}) {
            CAPTURE(axis,thickness);
            auto result=builder.createHollowCylinder(bounds,thickness,8,RadiusMode::ToEdge,axis,"test");
            REQUIRE(result.is_success());
            REQUIRE(result.value().size()==8);
            for(const auto& brush:result.value()) {
                CHECK(brush.fullySpecified());
                CHECK(bounds.contains(brush.bounds()));
            }
        }
    }
}

TEST_CASE("Ico accuracy errors are recoverable", "[brush-port]") {
    const BrushBuilder builder(MapFormat::Standard,vm::bbox3{8192.0});
    CHECK(builder.createIcoSphere(vm::bbox3{32.0},0,"test").is_error());
    CHECK(builder.createIcoSphere(vm::bbox3{32.0},5,"test").is_error());
    auto sphere=builder.createIcoSphere(vm::bbox3{32.0},1,"test");
    REQUIRE(sphere.is_success());
    CHECK(sphere.value().faceCount()==80);
}
}

namespace TrenchBroom::Model {
TEST_CASE("New round shapes are closed and stay inside the requested bounds", "[brush-port]") {
    const BrushBuilder builder(MapFormat::Valve,vm::bbox3{8192.0});
    const vm::bbox3 bounds{{-64,-48,-32},{64,48,32}};
    for(const CircleShape circle : {CircleShape{EdgeAlignedCircle{12}},CircleShape{VertexAlignedCircle{12}},CircleShape{ScalableCircle{0}},CircleShape{ScalableCircle{1}},CircleShape{ScalableCircle{2}},CircleShape{ScalableCircle{3}}}) {
        for(const auto axis:{vm::axis::x,vm::axis::y,vm::axis::z}) {
            size_t shapeIndex=0;
            for(auto result:{builder.createCylinder(bounds,circle,axis,"stone"),builder.createCone(bounds,circle,axis,"stone"),builder.createUvSphere(bounds,circle,8,axis,"stone")}) {
                CAPTURE(circle.index(),axis,shapeIndex);++shapeIndex;
                if(result.is_error()) UNSCOPED_INFO(std::get<Error>(result.error()).msg);
                REQUIRE(result.is_success());
                const auto& brush=result.value();
                CHECK(brush.fullySpecified());
                CHECK(brush.faceCount()>=4);
                // Floating point rotations can move an extremum by a few ULPs.
                CHECK(bounds.expand(0.0001).contains(brush.bounds()));
                for(const auto& face:brush.faces()) CHECK(face.attributes().textureName()=="stone");
            }
        }
    }
}
TEST_CASE("Arch and spandrel fill remain valid along every tunnel axis", "[brush-port]") {
    const BrushBuilder builder(MapFormat::Standard,vm::bbox3{8192.0});
    const vm::bbox3 bounds{{-64,-48,0},{64,48,96}};
    for(const auto axis:{vm::axis::x,vm::axis::y,vm::axis::z}) {
        for(const CircleShape circle : {CircleShape{EdgeAlignedCircle{8}},CircleShape{VertexAlignedCircle{9}},CircleShape{ScalableCircle{1}}}) {
            auto arch=builder.createArch(bounds,16,circle,axis,"arch");
            auto fill=builder.createSpandrelForArch(bounds,16,circle,axis,"fill");
            REQUIRE(arch.is_success()); REQUIRE(fill.is_success());
            CHECK_FALSE(arch.value().empty());
            for(const auto& brush:arch.value()) {CHECK(brush.fullySpecified());CHECK(bounds.expand(0.0001).contains(brush.bounds()));}
            for(const auto& brush:fill.value()) {CHECK(brush.fullySpecified());CHECK(bounds.expand(0.0001).contains(brush.bounds()));}
        }
    }
}
TEST_CASE("Every rock formation is deterministic and bounded", "[brush-port]") {
    const BrushBuilder builder(MapFormat::Standard,vm::bbox3{8192.0});
    const vm::bbox3 bounds{{-128,-128,0},{128,128,256}};
    for(const auto type:RockTypes) {
        for(const auto detail:{0.0,0.5,1.0}) {
            CAPTURE(int(type),detail);
            const auto points=makeRockFormation(bounds,type,detail,42);
            CHECK(points==makeRockFormation(bounds,type,detail,42));
            auto result=builder.createRockFormation(bounds,type,detail,42,"rock");
            REQUIRE(result.is_success()); CHECK_FALSE(result.value().empty());
            for(const auto& brush:result.value()) {CHECK(brush.fullySpecified());CHECK(bounds.expand(0.0001).contains(brush.bounds()));}
        }
    }
}
}

namespace TrenchBroom::Model {
TEST_CASE("Component matrix transformations reject degenerate geometry","[brush-port]") {
    const auto bounds=vm::bbox3{8192.0};const BrushBuilder builder(MapFormat::Valve,bounds);
    auto brush=builder.createCube(128,"test").value();
    const auto all=brush.vertexPositions();
    CHECK_FALSE(brush.canTransformVertices(bounds,all,vm::scaling_matrix(vm::vec3{1,1,0})));
    CHECK_FALSE(brush.canTransformVertices(bounds,all,vm::translation_matrix(vm::vec3{9000,0,0})));
    const auto top=brush.face(*brush.findFace(vm::vec3{0,0,1})).polygon();
    const auto scale=vm::scaling_matrix(vm::vec3{0.5,0.5,1});
    REQUIRE(brush.canTransformFaces(bounds,{top},scale));REQUIRE(brush.transformFaces(bounds,{top},scale,true).is_success());
    CHECK(brush.fullySpecified());CHECK(brush.hasFace(top.transform(scale)));
}
}

#include <chrono>
#include <iostream>
namespace TrenchBroom::Model {
TEST_CASE("Brush component operation benchmark", "[.][brush-benchmark]") {
    const auto world=vm::bbox3{8192.0};const BrushBuilder builder(MapFormat::Valve,world);
    for(const size_t sides:{8u,32u,64u}) {
        const auto source=builder.createCylinder(vm::bbox3{128.0},sides,RadiusMode::ToEdge,vm::axis::z,"test").value();
        const auto selected=source.face(*source.findFace(vm::vec3{0,0,1})).vertexPositions();
        const auto matrix=vm::scaling_matrix(vm::vec3{0.875,0.875,1});
        std::vector<double> samples;
        for(size_t sample=0;sample<5;++sample) {
            const auto start=std::chrono::steady_clock::now();
            for(size_t i=0;i<100;++i) {
                auto brush=source;
                if(!brush.canTransformVertices(world,selected,matrix) || brush.transformVertices(world,selected,matrix,true).is_error()) FAIL("Benchmark geometry failed");
            }
            samples.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
        }
        std::sort(samples.begin(),samples.end());
        std::cout<<"BRUSH_BENCH sides="<<sides<<" median_ms_per_100="<<samples[2]<<'\n';
    }
    SUCCEED();
}
}
