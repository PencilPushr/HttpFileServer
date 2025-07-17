#pragma once

#include <string>

namespace MimeTypes 
{
    /// Lookup the mime‑type for a given file name (by extension).  
    /// If the extension isn’t known, returns the given default (or text/plain).
    const std::string& get(const std::string& filename,
        const std::string& default_type = "text/plain");
}