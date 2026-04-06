#pragma once

#include "vk.h"

namespace enger
{
    struct ComputePipelineDesc
    {

    };

    struct Pipeline
    {
        vk::Pipeline handle;
        vk::PipelineLayout layout;
        vk::PipelineBindPoint bindPoint;
    };
}
