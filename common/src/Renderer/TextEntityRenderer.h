#pragma once

#include "AttrString.h"
#include "Color.h"
#include "FontDescriptor.h"
#include "GLVertexType.h"
#include "Renderable.h"
#include "VertexArray.h"
#include "TextAnchor.h"
#include "TextEntity.h"
#include "PreferenceManager.h"
#include "Preferences.h"

#include <vm/forward.h>
#include <vm/vec.h>

#include <vector>
#include <map>

namespace TrenchBroom::Renderer {

using TextVertexList = std::vector<GLVertexTypes::P3T2C4::Vertex>;
using RectVertexList = std::vector<GLVertexTypes::P3C4::Vertex>;

typedef std::map<FontDescriptor, std::vector<TextEntity>> EntityMap;

class TextEntityRenderer : public DirectRenderable {
    struct VertexCache {
      const FontDescriptor &m_fontDescriptor;
      VertexArray m_textVertices{};
      VertexArray m_rectVertices{};

      bool empty() {
          return
              m_textVertices.empty() &&
              m_rectVertices.empty();
      }
    };

    std::vector<VertexCache> m_renderCache;
    std::vector<VertexCache> m_topRenderCache;

  private:
    EntityMap m_entityCache;
  public:
    TextEntityRenderer();

  private:

    void doRender(RenderContext &renderContext) override;

    void renderVertexCache(VertexCache &vertexCache, RenderContext &renderContext);

    void doPrepareVertices(VboManager &vboManager) override;

    void prepareVertexCache(VboManager &vboManager);

    void computeEntityVertices(const TextEntity &entity, TextVertexList &textVertices, RectVertexList &rectVertices);

  public:
    void addTextEntity(TextEntity textEntity, RenderContext &renderContext);
};

}
