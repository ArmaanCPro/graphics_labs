#pragma once

#include <expected>
#include <filesystem>
#include <span>

#include <nfd.hpp>

namespace enger
{
    struct FileItem
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

    class FileLoader
    {
    public:
        virtual ~FileLoader() = default;

        virtual std::expected<std::filesystem::path, OpenDialogError> openDialog(std::span<const FileItem> items) = 0;
        virtual const char* getLastError() const = 0;
    };

    // Consider integrating with GLFW. Look at docs
    class NFDEFileLoader final : public FileLoader
    {
    public:
        NFDEFileLoader() = default;

        [[nodiscard]] std::expected<std::filesystem::path, OpenDialogError> openDialog(std::span<const FileItem> items) override;

        [[nodiscard]] const char* getLastError() const override;
    private:
        NFD::Guard nfd_;
    };
}
