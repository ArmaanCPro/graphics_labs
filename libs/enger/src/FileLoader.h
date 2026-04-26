#pragma once

#include <expected>
#include <filesystem>
#include <span>

#include <nfd.hpp>

#include "enger_export.h"

namespace enger
{
    struct ENGER_EXPORT FileItem
    {
        std::string name;
        std::string filetype;
    };

    enum class OpenDialogError
    {
        None,
        Cancelled,
        Other
    };

    class ENGER_EXPORT FileLoader
    {
    public:
        virtual ~FileLoader() = default;

        virtual std::expected<std::filesystem::path, OpenDialogError> openDialog(std::span<const FileItem> items) = 0;
        [[nodiscard]] virtual const char* getLastError() const = 0;
    };

    // Consider integrating with GLFW. Look at docs
    class ENGER_EXPORT NFDEFileLoader final : public FileLoader
    {
    public:
        NFDEFileLoader() = default;

        [[nodiscard]] std::expected<std::filesystem::path, OpenDialogError> openDialog(std::span<const FileItem> items) override;

        [[nodiscard]] const char* getLastError() const override;
    private:
        NFD::Guard nfd_;
    };
}
