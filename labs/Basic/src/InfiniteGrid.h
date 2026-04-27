#pragma once

#include <glm/glm.hpp>

#include "Scene/SceneGraph.h"

struct GridPushConstants
{
    alignas(4) glm::mat4 mvp;
    alignas(4) glm::vec4 camPos;
    alignas(4) glm::vec4 origin;
};

class InfiniteGrid
{
public:
    InfiniteGrid(enger::Device& device, vk::SampleCountFlagBits samples, vk::Format renderFormat, vk::Format depthFormat);

    [[nodiscard]] enger::ProceduralDrawObject draw(const enger::DrawContext& dctx);

private:
    enger::Holder<enger::GraphicsPipelineHandle> m_GridPipeline;
    enger::Holder<enger::PipelineLayoutHandle> m_GridPipelineLayout;
};