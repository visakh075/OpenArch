#include "DbConverter.h"
#include "DbManagerSQLite.h"
#include "DbManagerJson.h"

#include <sqlite3.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdio>

namespace DbConverter {

static const char* CONVERTER_SCHEMA = R"(
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS nodes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    type TEXT NOT NULL,
    parent_id INTEGER REFERENCES nodes(id) ON DELETE SET NULL,
    metadata TEXT,
    attributes TEXT,
    checksum INTEGER,
    status TEXT,
    reviewer TEXT
);

CREATE TABLE IF NOT EXISTS layers (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    kind TEXT NOT NULL,
    metadata TEXT,
    attributes TEXT,
    checksum INTEGER,
    status TEXT,
    reviewer TEXT
);

CREATE TABLE IF NOT EXISTS node_layers (
    node_id INTEGER NOT NULL,
    layer_id INTEGER NOT NULL,
    PRIMARY KEY (node_id, layer_id),
    FOREIGN KEY (node_id) REFERENCES nodes(id) ON DELETE CASCADE,
    FOREIGN KEY (layer_id) REFERENCES layers(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS edges (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    src_node_id INTEGER NOT NULL,
    src_layer_id INTEGER NOT NULL,
    dst_node_id INTEGER NOT NULL,
    dst_layer_id INTEGER NOT NULL,
    edge_type TEXT NOT NULL,
    metadata TEXT,
    attributes TEXT,
    checksum INTEGER,
    status TEXT,
    reviewer TEXT,
    FOREIGN KEY (src_node_id, src_layer_id)
        REFERENCES node_layers(node_id, layer_id) ON DELETE CASCADE,
    FOREIGN KEY (dst_node_id, dst_layer_id)
        REFERENCES node_layers(node_id, layer_id) ON DELETE CASCADE
);
)";

static std::string escapeJson(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        switch (c) {
        case '"':  o << "\\\""; break;
        case '\\': o << "\\\\"; break;
        case '\b': o << "\\b";  break;
        case '\f': o << "\\f";  break;
        case '\n': o << "\\n";  break;
        case '\r': o << "\\r";  break;
        case '\t': o << "\\t";  break;
        default:
            if ('\x00' <= c && c <= '\x1f') {
                o << "\\u" << std::hex << (int)c;
            } else {
                o << c;
            }
            break;
        }
    }
    return o.str();
}

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    return s;
}

Format detectFormat(const std::string& path) {
    // 1. Check extension first
    auto dot = path.find_last_of('.');
    if (dot != std::string::npos) {
        std::string ext = toLower(path.substr(dot));
        if (ext == ".json") {
            return Format::Json;
        }
        if (ext == ".db" || ext == ".sqlite" || ext == ".sqlite3") {
            return Format::SQLite;
        }
    }

    // 2. Check file header if file exists
    std::ifstream in(path, std::ios::binary);
    if (in.is_open()) {
        char buffer[16] = {0};
        in.read(buffer, sizeof(buffer));
        std::string header(buffer, in.gcount());

        if (header.find("SQLite format 3") != std::string::npos) {
            return Format::SQLite;
        }

        // Check for JSON
        in.seekg(0);
        char ch;
        while (in.get(ch)) {
            if (!std::isspace(static_cast<unsigned char>(ch))) {
                if (ch == '{' || ch == '[') {
                    return Format::Json;
                }
                break;
            }
        }
    }

    return Format::Unknown;
}

std::string formatName(Format format) {
    switch (format) {
    case Format::SQLite: return "SQLite";
    case Format::Json:   return "JSON";
    default:             return "Unknown";
    }
}

Result jsonToSqlite(const std::string& jsonPath, const std::string& sqlitePath, bool overwrite) {
    if (jsonPath == sqlitePath) {
        return Result::failure("Source and target paths cannot be the same: " + jsonPath);
    }

    {
        std::ifstream checkFile(jsonPath);
        if (!checkFile.good()) {
            return Result::failure("Source JSON file does not exist or cannot be read: " + jsonPath);
        }
    }

    if (!overwrite) {
        std::ifstream checkTarget(sqlitePath);
        if (checkTarget.good()) {
            return Result::failure("Target SQLite file already exists and overwrite is disabled: " + sqlitePath);
        }
    }

    // 1. Read source JSON
    DbManagerJson jsonDb;
    Result openRes = jsonDb.open(jsonPath);
    if (!openRes.ok) {
        return Result::failure("Failed to read JSON source file: " + openRes.message);
    }

    std::vector<LayerData> layers = jsonDb.getAllLayers();
    std::vector<NodeData> nodes = jsonDb.getAllNodes();
    std::vector<NodeLayer> nodeLayers;
    for (const auto& l : layers) {
        auto nls = jsonDb.getNodesInLayer(l.id);
        nodeLayers.insert(nodeLayers.end(), nls.begin(), nls.end());
    }
    std::vector<EdgeData> edges = jsonDb.getAllEdges();
    jsonDb.close();

    // 2. Prepare target SQLite database
    if (overwrite) {
        std::remove(sqlitePath.c_str());
    }

    sqlite3* db = nullptr;
    if (sqlite3_open(sqlitePath.c_str(), &db) != SQLITE_OK) {
        std::string err = db ? sqlite3_errmsg(db) : "Cannot open file";
        if (db) sqlite3_close(db);
        return Result::failure("Failed to create SQLite target: " + err);
    }

    char* errMsg = nullptr;
    if (sqlite3_exec(db, CONVERTER_SCHEMA, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string msg = errMsg ? errMsg : "Schema execution error";
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return Result::failure("Failed to initialize SQLite schema: " + msg);
    }

    // Disable foreign keys during bulk insertion to allow any insertion order, then re-enable
    sqlite3_exec(db, "PRAGMA foreign_keys = OFF;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    // 3. Insert Layers
    {
        sqlite3_stmt* st = nullptr;
        const char* sql = "INSERT INTO layers(id, name, kind, metadata, attributes, checksum, status, reviewer) "
                          "VALUES(?, ?, ?, ?, ?, ?, ?, ?);";
        if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) == SQLITE_OK) {
            for (const auto& l : layers) {
                sqlite3_bind_int64(st, 1, static_cast<sqlite3_int64>(l.id));
                sqlite3_bind_text(st, 2, l.name.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(st, 3, l.kind.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(st, 4, l.metadata.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(st, 5, l.attributes.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int(st, 6, static_cast<int>(l.checksum));
                sqlite3_bind_text(st, 7, to_string(l.status).c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(st, 8, l.reviewer.c_str(), -1, SQLITE_TRANSIENT);

                sqlite3_step(st);
                sqlite3_reset(st);
            }
            sqlite3_finalize(st);
        }
    }

    // 4. Insert Nodes
    {
        sqlite3_stmt* st = nullptr;
        const char* sql = "INSERT INTO nodes(id, name, type, parent_id, metadata, attributes, checksum, status, reviewer) "
                          "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?);";
        if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) == SQLITE_OK) {
            for (const auto& n : nodes) {
                sqlite3_bind_int64(st, 1, static_cast<sqlite3_int64>(n.id));
                sqlite3_bind_text(st, 2, n.name.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(st, 3, n.type.c_str(), -1, SQLITE_TRANSIENT);
                if (n.parentId.has_value()) {
                    sqlite3_bind_int64(st, 4, static_cast<sqlite3_int64>(*n.parentId));
                } else {
                    sqlite3_bind_null(st, 4);
                }
                sqlite3_bind_text(st, 5, n.metadata.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(st, 6, n.attributes.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int(st, 7, static_cast<int>(n.checksum));
                sqlite3_bind_text(st, 8, to_string(n.status).c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(st, 9, n.reviewer.c_str(), -1, SQLITE_TRANSIENT);

                sqlite3_step(st);
                sqlite3_reset(st);
            }
            sqlite3_finalize(st);
        }
    }

    // 5. Insert Node-Layers
    {
        sqlite3_stmt* st = nullptr;
        const char* sql = "INSERT OR IGNORE INTO node_layers(node_id, layer_id) VALUES(?, ?);";
        if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) == SQLITE_OK) {
            for (const auto& nl : nodeLayers) {
                sqlite3_bind_int64(st, 1, static_cast<sqlite3_int64>(nl.nodeId));
                sqlite3_bind_int64(st, 2, static_cast<sqlite3_int64>(nl.layerId));
                sqlite3_step(st);
                sqlite3_reset(st);
            }
            sqlite3_finalize(st);
        }
    }

    // 6. Insert Edges
    {
        sqlite3_stmt* st = nullptr;
        const char* sql = "INSERT INTO edges(id, src_node_id, src_layer_id, dst_node_id, dst_layer_id, "
                          "edge_type, metadata, attributes, checksum, status, reviewer) "
                          "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
        if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) == SQLITE_OK) {
            for (const auto& e : edges) {
                sqlite3_bind_int64(st, 1, static_cast<sqlite3_int64>(e.id));
                sqlite3_bind_int64(st, 2, static_cast<sqlite3_int64>(e.srcNode));
                sqlite3_bind_int64(st, 3, static_cast<sqlite3_int64>(e.srcLayer));
                sqlite3_bind_int64(st, 4, static_cast<sqlite3_int64>(e.dstNode));
                sqlite3_bind_int64(st, 5, static_cast<sqlite3_int64>(e.dstLayer));
                sqlite3_bind_text(st, 6, e.edgeType.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(st, 7, e.metadata.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(st, 8, e.attributes.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int(st, 9, static_cast<int>(e.checksum));
                sqlite3_bind_text(st, 10, to_string(e.status).c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(st, 11, e.reviewer.c_str(), -1, SQLITE_TRANSIENT);

                sqlite3_step(st);
                sqlite3_reset(st);
            }
            sqlite3_finalize(st);
        }
    }

    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);

    sqlite3_close(db);

    return Result::success("Converted JSON to SQLite: " + std::to_string(nodes.size()) + " nodes, " +
                           std::to_string(layers.size()) + " layers, " +
                           std::to_string(edges.size()) + " edges.");
}

Result sqliteToJson(const std::string& sqlitePath, const std::string& jsonPath, bool overwrite) {
    if (sqlitePath == jsonPath) {
        return Result::failure("Source and target paths cannot be the same: " + sqlitePath);
    }

    {
        std::ifstream checkFile(sqlitePath);
        if (!checkFile.good()) {
            return Result::failure("Source SQLite file does not exist or cannot be read: " + sqlitePath);
        }
    }

    if (!overwrite) {
        std::ifstream checkTarget(jsonPath);
        if (checkTarget.good()) {
            return Result::failure("Target JSON file already exists and overwrite is disabled: " + jsonPath);
        }
    }

    // 1. Read SQLite source
    DbManagerSQLite sqliteDb;
    Result openRes = sqliteDb.open(sqlitePath);
    if (!openRes.ok) {
        return Result::failure("Failed to open SQLite database: " + openRes.message);
    }

    std::vector<LayerData> layers = sqliteDb.getAllLayers();
    std::vector<NodeData> nodes = sqliteDb.getAllNodes();
    std::vector<NodeLayer> nodeLayers;
    for (const auto& l : layers) {
        auto nls = sqliteDb.getNodesInLayer(l.id);
        nodeLayers.insert(nodeLayers.end(), nls.begin(), nls.end());
    }
    std::vector<EdgeData> edges = sqliteDb.getAllEdges();
    sqliteDb.close();

    // 2. Format JSON
    std::ostringstream os;
    os << "{\n";

    // 2.1 Layers
    os << "  \"layers\": [\n";
    for (size_t i = 0; i < layers.size(); ++i) {
        const auto& l = layers[i];
        os << "    {\n"
           << "      \"id\": " << l.id << ",\n"
           << "      \"name\": \"" << escapeJson(l.name) << "\",\n"
           << "      \"kind\": \"" << escapeJson(l.kind) << "\",\n"
           << "      \"attributes\": \"" << escapeJson(l.attributes) << "\",\n"
           << "      \"checksum\": " << l.checksum << ",\n"
           << "      \"status\": \"" << escapeJson(to_string(l.status)) << "\",\n"
           << "      \"reviewer\": \"" << escapeJson(l.reviewer) << "\"\n"
           << "    }" << (i + 1 < layers.size() ? "," : "") << "\n";
    }
    os << "  ],\n";

    // 2.2 Nodes
    os << "  \"nodes\": [\n";
    for (size_t i = 0; i < nodes.size(); ++i) {
        const auto& n = nodes[i];
        os << "    {\n"
           << "      \"id\": " << n.id << ",\n"
           << "      \"name\": \"" << escapeJson(n.name) << "\",\n"
           << "      \"type\": \"" << escapeJson(n.type) << "\",\n";

        if (n.parentId.has_value()) {
            os << "      \"parent_id\": " << *n.parentId << ",\n";
        } else {
            os << "      \"parent_id\": null,\n";
        }

        os << "      \"attributes\": \"" << escapeJson(n.attributes) << "\",\n"
           << "      \"checksum\": " << n.checksum << ",\n"
           << "      \"status\": \"" << escapeJson(to_string(n.status)) << "\",\n"
           << "      \"reviewer\": \"" << escapeJson(n.reviewer) << "\"\n"
           << "    }" << (i + 1 < nodes.size() ? "," : "") << "\n";
    }
    os << "  ],\n";

    // 2.3 Node-Layers
    os << "  \"node_layers\": [\n";
    for (size_t i = 0; i < nodeLayers.size(); ++i) {
        const auto& nl = nodeLayers[i];
        os << "    {\n"
           << "      \"node_id\": " << nl.nodeId << ",\n"
           << "      \"layer_id\": " << nl.layerId << "\n"
           << "    }" << (i + 1 < nodeLayers.size() ? "," : "") << "\n";
    }
    os << "  ],\n";

    // 2.4 Edges
    os << "  \"edges\": [\n";
    for (size_t i = 0; i < edges.size(); ++i) {
        const auto& e = edges[i];
        os << "    {\n"
           << "      \"id\": " << e.id << ",\n"
           << "      \"src_node_id\": " << e.srcNode << ",\n"
           << "      \"src_layer_id\": " << e.srcLayer << ",\n"
           << "      \"dst_node_id\": " << e.dstNode << ",\n"
           << "      \"dst_layer_id\": " << e.dstLayer << ",\n"
           << "      \"edge_type\": \"" << escapeJson(e.edgeType) << "\",\n"
           << "      \"attributes\": \"" << escapeJson(e.attributes) << "\",\n"
           << "      \"checksum\": " << e.checksum << ",\n"
           << "      \"status\": \"" << escapeJson(to_string(e.status)) << "\",\n"
           << "      \"reviewer\": \"" << escapeJson(e.reviewer) << "\"\n"
           << "    }" << (i + 1 < edges.size() ? "," : "") << "\n";
    }
    os << "  ],\n";

    // 2.5 Layout Section
    os << "  \"layout\": {\n";

    // Layout -> Nodes
    os << "    \"nodes\": {\n";
    size_t count = 0;
    for (const auto& n : nodes) {
        std::string meta = n.metadata.empty() ? "{}" : n.metadata;
        os << "      \"" << n.id << "\": " << meta << (++count < nodes.size() ? ",\n" : "\n");
    }
    os << "    },\n";

    // Layout -> Layers
    os << "    \"layers\": {\n";
    count = 0;
    for (const auto& l : layers) {
        std::string meta = l.metadata.empty() ? "{}" : l.metadata;
        os << "      \"" << l.id << "\": " << meta << (++count < layers.size() ? ",\n" : "\n");
    }
    os << "    },\n";

    // Layout -> Edges
    os << "    \"edges\": {\n";
    count = 0;
    for (const auto& e : edges) {
        std::string meta = e.metadata.empty() ? "{}" : e.metadata;
        os << "      \"" << e.id << "\": " << meta << (++count < edges.size() ? ",\n" : "\n");
    }
    os << "    }\n";

    os << "  }\n";
    os << "}\n";

    // 3. Write to JSON file
    std::ofstream out(jsonPath, std::ios::trunc);
    if (!out.is_open()) {
        return Result::failure("Cannot open target JSON file for writing: " + jsonPath);
    }
    out << os.str();

    return Result::success("Converted SQLite to JSON: " + std::to_string(nodes.size()) + " nodes, " +
                           std::to_string(layers.size()) + " layers, " +
                           std::to_string(edges.size()) + " edges.");
}

Result convert(const std::string& sourcePath, const std::string& targetPath, bool overwrite) {
    if (sourcePath == targetPath) {
        return Result::failure("Source and target paths cannot be the same: " + sourcePath);
    }

    {
        std::ifstream checkSource(sourcePath);
        if (!checkSource.good()) {
            return Result::failure("Source file does not exist or cannot be read: " + sourcePath);
        }
    }

    Format srcFormat = detectFormat(sourcePath);
    Format dstFormat = detectFormat(targetPath);

    if (srcFormat == Format::Unknown) {
        return Result::failure("Could not determine format of source file: " + sourcePath);
    }
    if (dstFormat == Format::Unknown) {
        return Result::failure("Could not determine format of target file: " + targetPath);
    }
    if (srcFormat == dstFormat) {
        return Result::failure("Source and target have the same format (" + formatName(srcFormat) + ").");
    }

    if (srcFormat == Format::Json && dstFormat == Format::SQLite) {
        return jsonToSqlite(sourcePath, targetPath, overwrite);
    } else if (srcFormat == Format::SQLite && dstFormat == Format::Json) {
        return sqliteToJson(sourcePath, targetPath, overwrite);
    }

    return Result::failure("Unsupported conversion from " + formatName(srcFormat) + " to " + formatName(dstFormat));
}

} // namespace DbConverter
