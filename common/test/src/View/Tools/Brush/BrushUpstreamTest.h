#include "TestSourcePath.h"
#pragma once

#include "Error.h"
#include "IO/GameConfigParser.h"
#include "Logger.h"
#include "Model/GameImpl.h"
#include "View/Document/MapDocument.h"
#include "Model/BrushBuilder.h"
#include "Model/BrushNode.h"
#include "Model/LayerNode.h"
#include "Model/WorldNode.h"
#include "View/Document/MapDocumentCommandFacade.h"
#include <kdl/result.h>
#include <catch2/catch.hpp>

namespace TrenchBroom::View {
class BrushUpstreamTest {
protected:
    static std::filesystem::path repository() {
        return testSourceRoot();
    }
    NullLogger logger;
    Model::GameConfig config = IO::GameConfigParser(R"({
        "version": 8, "name": "Generic",
        "fileformats": [{"format": "Standard"}, {"format": "Valve"}],
        "filesystem": {"searchpath": ".", "packageformat": {"extension": ".pak", "format": "idpak"} },
        "textures": {"root": "textures", "format": {"extensions": [".png"], "format": "image"} },
        "entities": {"definitions": ["Generic.fgd"], "defaultcolor": "0.6 0.6 0.6 1.0"}
    })",repository()/"app/resources/games/Generic/GameConfig.cfg").parse();
    std::shared_ptr<Model::GameImpl> game = std::make_shared<Model::GameImpl>(config,std::filesystem::path{},logger);
    std::shared_ptr<MapDocument> document = MapDocumentCommandFacade::newMapDocument();
    Model::BrushNode* addBrush(const vm::bbox3& bounds,const std::string& texture="test") {
        const Model::BrushBuilder builder(document->world()->mapFormat(),document->worldBounds());
        auto result=builder.createCuboid(bounds,texture);REQUIRE(result.is_success());
        auto* node=new Model::BrushNode(std::move(result).value());
        REQUIRE_FALSE(document->addNodes({{document->world()->defaultLayer(),{node}}}).empty());return node;
    }
    void newMap() {
        REQUIRE(document->newDocument(Model::MapFormat::Standard,vm::bbox3{32768.0},game).is_success());
        document->selectAllNodes(); document->deleteObjects();
    }
};
}
