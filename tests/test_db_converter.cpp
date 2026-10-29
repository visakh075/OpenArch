#include <iostream>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <memory>
#include <vector>
#include <filesystem>

#include "db/DbConverter.h"
#include "db/DbManagerSQLite.h"
#include "db/DbManagerJson.h"

static void removeIfExists(const std::string& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

static std::string getTempFilePath(const std::string& filename) {
    std::error_code ec;
    auto tempDir = std::filesystem::temp_directory_path(ec);
    if (ec) {
        return filename;
    }
    return (tempDir / filename).string();
}

static const NodeData* findNode(const std::vector<NodeData>& list, NodeId id) {
    for (const auto& n : list) {
        if (n.id == id) return &n;
    }
    return nullptr;
}

int main()
{
    std::cout << "========================================\n";
    std::cout << " STARTING DB CONVERTER TESTS\n";
    std::cout << "========================================\n";

    // 1. Format Detection Tests
    std::cout << "[TEST] 1. Verifying format detection...\n";
    {
        assert(DbConverter::detectFormat("test.json") == DbConverter::Format::Json);
        assert(DbConverter::detectFormat("test.db") == DbConverter::Format::SQLite);
        assert(DbConverter::detectFormat("test.sqlite") == DbConverter::Format::SQLite);
        assert(DbConverter::detectFormat("test.sqlite3") == DbConverter::Format::SQLite);
        assert(DbConverter::detectFormat("unknown.txt") == DbConverter::Format::Unknown);
        std::cout << " -> PASSED: Extension detection works.\n";
    }

    // 2. Conversion JSON -> SQLite -> JSON Roundtrip
    std::cout << "[TEST] 2. Verifying JSON -> SQLite -> JSON roundtrip...\n";
    std::string sampleJson;
    std::vector<std::string> candidates = {
        "build/architecture.json",
        "../build/architecture.json",
        "examples/enterprise_cloud_architecture.json",
        "../examples/enterprise_cloud_architecture.json"
    };
    for (const auto& c : candidates) {
        std::ifstream f(c);
        if (f.good()) {
            sampleJson = c;
            break;
        }
    }
    assert(!sampleJson.empty() && "No sample architecture JSON found for test");

    std::string tempDb = getTempFilePath("test_roundtrip.db");
    std::string tempJson = getTempFilePath("test_roundtrip.json");

    removeIfExists(tempDb);
    removeIfExists(tempJson);

    // JSON -> SQLite
    Result r1 = DbConverter::jsonToSqlite(sampleJson, tempDb, true);
    assert(r1.ok);
    std::cout << " -> PASSED: jsonToSqlite succeeded: " << r1.message << "\n";

    // Verify SQLite contents via DbManagerSQLite
    {
        DbManagerSQLite sqliteDb;
        Result rOpen = sqliteDb.open(tempDb);
        assert(rOpen.ok);

        auto nodes = sqliteDb.getAllNodes();
        auto layers = sqliteDb.getAllLayers();
        auto edges = sqliteDb.getAllEdges();

        assert(nodes.size() == 10);
        assert(layers.size() == 4);
        assert(edges.size() == 7);

        // Check node 7 and 8 parent relationship
        const NodeData* n7 = findNode(nodes, 7);
        assert(n7 != nullptr);
        assert(n7->parentId.has_value());
        assert(*n7->parentId == 8);

        // Check node 8 has no parent
        const NodeData* n8 = findNode(nodes, 8);
        assert(n8 != nullptr);
        assert(!n8->parentId.has_value());

        // Check layout coordinates preserved in metadata
        assert(!n7->metadata.empty());
        assert(n7->metadata.find("\"x\"") != std::string::npos);

        sqliteDb.close();
    }
    std::cout << " -> PASSED: SQLite database validated.\n";

    // SQLite -> JSON
    Result r2 = DbConverter::sqliteToJson(tempDb, tempJson, true);
    assert(r2.ok);
    std::cout << " -> PASSED: sqliteToJson succeeded: " << r2.message << "\n";

    // Verify JSON contents via DbManagerJson
    {
        DbManagerJson jsonDb;
        Result rOpen = jsonDb.open(tempJson);
        assert(rOpen.ok);

        auto nodes = jsonDb.getAllNodes();
        auto layers = jsonDb.getAllLayers();
        auto edges = jsonDb.getAllEdges();

        assert(nodes.size() == 10);
        assert(layers.size() == 4);
        assert(edges.size() == 7);

        const NodeData* n7 = findNode(nodes, 7);
        assert(n7 != nullptr);
        assert(n7->parentId.has_value());
        assert(*n7->parentId == 8);

        // Check layout coordinates
        assert(!n7->metadata.empty());
        assert(n7->metadata.find("\"x\"") != std::string::npos);

        jsonDb.close();
    }
    std::cout << " -> PASSED: Roundtrip JSON validated.\n";

    // 3. Test generic convert API with auto-detection
    std::cout << "[TEST] 3. Verifying generic convert API with auto-detection...\n";
    std::string autoDb = getTempFilePath("test_auto.db");
    removeIfExists(autoDb);

    Result rAuto = DbConverter::convert(tempJson, autoDb, true);
    assert(rAuto.ok);

    std::string autoJson = getTempFilePath("test_auto.json");
    removeIfExists(autoJson);

    Result rAutoRev = DbConverter::convert(autoDb, autoJson, true);
    assert(rAutoRev.ok);
    std::cout << " -> PASSED: Auto-detection convert bidirectional succeeded.\n";

    // 4. Test error handling (missing file, same source and target, invalid format)
    std::cout << "[TEST] 4. Verifying error handling...\n";
    Result rErr1 = DbConverter::convert(getTempFilePath("definitely_does_not_exist_12345.json"), getTempFilePath("out.db"), true);
    assert(!rErr1.ok);

    Result rErr2 = DbConverter::convert(tempJson, tempJson, true);
    assert(!rErr2.ok);

    std::string dummyTxt = getTempFilePath("dummy.txt");
    {
        std::ofstream ofs(dummyTxt);
        ofs << "not a db or json";
    }
    Result rErr3 = DbConverter::convert(dummyTxt, getTempFilePath("out.db"), true);
    assert(!rErr3.ok);
    std::cout << " -> PASSED: Error handling handled invalid inputs correctly.\n";

    // Cleanup
    removeIfExists(tempDb);
    removeIfExists(tempJson);
    removeIfExists(autoDb);
    removeIfExists(autoJson);
    removeIfExists(dummyTxt);
    removeIfExists(dummyTxt);

    std::cout << "==============================================\n";
    std::cout << " ALL DB CONVERTER TESTS PASSED SUCCESSFULLY!\n";
    std::cout << "==============================================\n";

    return 0;
}
