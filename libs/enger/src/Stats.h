#pragma once

struct ENGER_EXPORT EngineStats
{
    float frameTime = 0.0f;
    int triangleCount = 0;
    int drawCalls = 0;
    float sceneUpdateTime = 0.0f;
    float meshDrawTime = 0.0f;
};
