#include "MIMETypes.h"

#include <map>
#include <filesystem>

namespace fs = std::filesystem;

namespace MimeTypes 
{
    static const std::map<std::string, std::string>& initMap()
    {
        static std::map<std::string, std::string> m =
        {
            { ".html", "text/html" },
            { ".htm",  "text/html" },
            { ".css",  "text/css" },
            { ".js",   "application/javascript" },
            { ".json", "application/json" },
            { ".txt",  "text/plain" },
            { ".pdf",  "application/pdf" },
            { ".png",  "image/png" },
            { ".jpg",  "image/jpeg" },
            { ".jpeg", "image/jpeg" },
            { ".gif",  "image/gif" },
            { ".svg",  "image/svg+xml" },
            { ".ico",  "image/x-icon" },
            { ".mp4",  "video/mp4" },
            { ".mp3",  "audio/mpeg" },
            { ".zip",  "application/zip" },
            { ".xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet" },
            // add anything else you need…
        };
        return m;
    }

    const 
    std::string& 
    get(
        const std::string& filename,
        const std::string& default_type
    )
    {
        auto ext = fs::path(filename).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        const auto& m = initMap();
        auto it = m.find(ext);
        return (it != m.end() ? it->second : default_type);
    }
}