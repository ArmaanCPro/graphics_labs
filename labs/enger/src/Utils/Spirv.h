#pragma once

#include <expected>
#include <vector>
#include <fstream>
#include <filesystem>

#include "Profiling/Profiler.h"

namespace enger
{
    static std::expected<std::vector<uint32_t>, std::string> loadSpirvFromFile(const std::filesystem::path& path)
    {
        ENGER_PROFILE_FUNCTION_COLOR(ENGER_PROFILE_COLOR_CREATE)
        std::ifstream file(path, std::ios::ate | std::ios::binary);

        if (!file)
        {
            return std::unexpected("Failed to open file " + path.string());
        }

        const auto size = file.tellg();
        std::vector<uint32_t> data(size / sizeof(uint32_t));
        file.seekg(0);

        file.read(reinterpret_cast<char*>(data.data()), size);
        file.close();

        return data;
    }

}