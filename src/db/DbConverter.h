#pragma once

#include <string>
#include "db/DbManager.h"

namespace DbConverter {

enum class Format {
    Unknown,
    SQLite,
    Json
};

// Detect format by file extension or file content
Format detectFormat(const std::string& path);

// Human-readable format name
std::string formatName(Format format);

// Explicit conversions
Result jsonToSqlite(const std::string& jsonPath, const std::string& sqlitePath, bool overwrite = true);
Result sqliteToJson(const std::string& sqlitePath, const std::string& jsonPath, bool overwrite = true);

// Generic convert that auto-detects source and target format
Result convert(const std::string& sourcePath, const std::string& targetPath, bool overwrite = true);

} // namespace DbConverter
