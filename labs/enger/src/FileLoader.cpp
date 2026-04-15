#include "FileLoader.h"

namespace enger
{
    std::expected<std::filesystem::path, OpenDialogError> NFDEFileLoader::openDialog(std::span<const FileItem> items)
    {
        std::vector<nfdfilteritem_t> filters;
        for (auto& item: items)
        {
            filters.push_back({item.name.c_str(), item.filetype.c_str()});
        }

        NFD::UniquePath outPath;
        nfdresult_t result = NFD::OpenDialog(outPath,
            filters.data(), static_cast<nfdfiltersize_t>(filters.size()));

        if (result == NFD_OKAY)
        {
            return std::filesystem::path(outPath.get());
        }
        else if (result == NFD_CANCEL)
        {
            return std::unexpected(OpenDialogError::Cancelled);
        }
        else
        {
            return std::unexpected(OpenDialogError::Other);
        }
    }

    const char* NFDEFileLoader::getLastError() const
    {
        return NFD::GetError();
    }
}
