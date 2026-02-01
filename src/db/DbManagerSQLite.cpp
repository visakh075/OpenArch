#include "DbManagerSQLite.h"

#include <sqlite3.h>
#include <sstream>

/* ============================================================
   Schema (authoritative)
   ============================================================ */
static const char* SCHEMA = R"(
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS nodes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    type TEXT NOT NULL,
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

/* ============================================================
   Helpers
   ============================================================ */

static Result exec(sqlite3* db, const char* sql) {
    char* err = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "SQL error";
        sqlite3_free(err);
        return Result::failure(msg);
    }
    return Result::success();
}

/* ============================================================
   Lifecycle
   ============================================================ */

Result DbManagerSQLite::open(const std::string& path) {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK)
        return Result::failure("Failed to open SQLite DB");

    return exec(db_, SCHEMA);
}

void DbManagerSQLite::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

/* ============================================================
   Nodes
   ============================================================ */

Result DbManagerSQLite::createNode(const NodeData& n, NodeId& outId) {
    sqlite3_stmt* st;
    sqlite3_prepare_v2(
        db_,
        "INSERT INTO nodes(name,type,metadata,attributes,checksum,status,reviewer)"
        "VALUES(?,?,?,?,?,?,?)",
        -1, &st, nullptr);

    sqlite3_bind_text(st, 1, n.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, n.type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, n.metadata.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, n.attributes.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 5, n.checksum);
    sqlite3_bind_text(st, 6, to_string(n.status).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 7, n.reviewer.empty() ? nullptr : n.reviewer.c_str(),
                      -1, SQLITE_TRANSIENT);

    if (sqlite3_step(st) != SQLITE_DONE) {
        sqlite3_finalize(st);
        return Result::failure("createNode failed");
    }

    sqlite3_finalize(st);
    outId = sqlite3_last_insert_rowid(db_);
    return Result::success();
}

Result DbManagerSQLite::updateNode(const NodeData& n) {
    sqlite3_stmt* st;
    sqlite3_prepare_v2(
        db_,
        "UPDATE nodes SET name=?,type=?,metadata=?,attributes=?,checksum=?,status=?,reviewer=? "
        "WHERE id=?",
        -1, &st, nullptr);

    sqlite3_bind_text(st, 1, n.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, n.type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, n.metadata.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, n.attributes.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 5, n.checksum);
    sqlite3_bind_text(st, 6, to_string(n.status).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 7, n.reviewer.empty() ? nullptr : n.reviewer.c_str(),
                      -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 8, n.id);

    sqlite3_step(st);
    sqlite3_finalize(st);
    return Result::success();
}

Result DbManagerSQLite::deleteNode(NodeId id) {
    sqlite3_stmt* st;
    sqlite3_prepare_v2(db_, "DELETE FROM nodes WHERE id=?", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, id);
    sqlite3_step(st);
    sqlite3_finalize(st);
    return Result::success();
}

std::vector<NodeData> DbManagerSQLite::getAllNodes() {
    std::vector<NodeData> out;
    sqlite3_stmt* st;

    sqlite3_prepare_v2(
        db_,
        "SELECT id,name,type,metadata,attributes,checksum,status,reviewer FROM nodes",
        -1, &st, nullptr);

    while (sqlite3_step(st) == SQLITE_ROW) {
        NodeData n;
        n.id         = sqlite3_column_int64(st, 0);
        n.name       = (const char*)sqlite3_column_text(st, 1);
        n.type       = (const char*)sqlite3_column_text(st, 2);
        n.metadata   = (const char*)sqlite3_column_text(st, 3);
        n.attributes = (const char*)sqlite3_column_text(st, 4);
        n.checksum   = sqlite3_column_int(st, 5);
        n.status     = status_from_string(
                           (const char*)sqlite3_column_text(st, 6));
        if (sqlite3_column_text(st, 7))
            n.reviewer = (const char*)sqlite3_column_text(st, 7);
        out.push_back(n);
    }

    sqlite3_finalize(st);
    return out;
}

/* ============================================================
   Layers
   ============================================================ */

Result DbManagerSQLite::createLayer(const LayerData& l, LayerId& outId) {
    sqlite3_stmt* st;
    sqlite3_prepare_v2(
        db_,
        "INSERT INTO layers(name,kind,metadata,attributes,checksum,status,reviewer)"
        "VALUES(?,?,?,?,?,?,?)",
        -1, &st, nullptr);

    sqlite3_bind_text(st, 1, l.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, l.kind.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, l.metadata.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, l.attributes.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 5, l.checksum);
    sqlite3_bind_text(st, 6, to_string(l.status).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 7, l.reviewer.empty() ? nullptr : l.reviewer.c_str(),
                      -1, SQLITE_TRANSIENT);

    sqlite3_step(st);
    sqlite3_finalize(st);
    outId = sqlite3_last_insert_rowid(db_);
    return Result::success();
}

Result DbManagerSQLite::updateLayer(const LayerData& l) {
    sqlite3_stmt* st;
    sqlite3_prepare_v2(
        db_,
        "UPDATE layers SET name=?,kind=?,metadata=?,attributes=?,checksum=?,status=?,reviewer=? "
        "WHERE id=?",
        -1, &st, nullptr);

    sqlite3_bind_text(st, 1, l.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, l.kind.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, l.metadata.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, l.attributes.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 5, l.checksum);
    sqlite3_bind_text(st, 6, to_string(l.status).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 7, l.reviewer.empty() ? nullptr : l.reviewer.c_str(),
                      -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 8, l.id);

    sqlite3_step(st);
    sqlite3_finalize(st);
    return Result::success();
}

Result DbManagerSQLite::deleteLayer(LayerId id) {
    sqlite3_stmt* st;
    sqlite3_prepare_v2(db_, "DELETE FROM layers WHERE id=?", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, id);
    sqlite3_step(st);
    sqlite3_finalize(st);
    return Result::success();
}

std::vector<LayerData> DbManagerSQLite::getAllLayers() {
    std::vector<LayerData> out;
    sqlite3_stmt* st;

    sqlite3_prepare_v2(
        db_,
        "SELECT id,name,kind,metadata,attributes,checksum,status,reviewer FROM layers",
        -1, &st, nullptr);

    while (sqlite3_step(st) == SQLITE_ROW) {
        LayerData l;
        l.id         = sqlite3_column_int64(st, 0);
        l.name       = (const char*)sqlite3_column_text(st, 1);
        l.kind       = (const char*)sqlite3_column_text(st, 2);
        l.metadata   = (const char*)sqlite3_column_text(st, 3);
        l.attributes = (const char*)sqlite3_column_text(st, 4);
        l.checksum   = sqlite3_column_int(st, 5);
        l.status     = status_from_string(
                           (const char*)sqlite3_column_text(st, 6));
        if (sqlite3_column_text(st, 7))
            l.reviewer = (const char*)sqlite3_column_text(st, 7);
        out.push_back(l);
    }

    sqlite3_finalize(st);
    return out;
}

/* ============================================================
   Node–Layer
   ============================================================ */

Result DbManagerSQLite::addNodeToLayer(NodeId n, LayerId l) {
    sqlite3_stmt* st;
    sqlite3_prepare_v2(
        db_,
        "INSERT OR IGNORE INTO node_layers(node_id,layer_id) VALUES(?,?)",
        -1, &st, nullptr);

    sqlite3_bind_int64(st, 1, n);
    sqlite3_bind_int64(st, 2, l);
    sqlite3_step(st);
    sqlite3_finalize(st);
    return Result::success();
}

Result DbManagerSQLite::removeNodeFromLayer(NodeId n, LayerId l) {
    sqlite3_stmt* st;
    sqlite3_prepare_v2(
        db_,
        "DELETE FROM node_layers WHERE node_id=? AND layer_id=?",
        -1, &st, nullptr);

    sqlite3_bind_int64(st, 1, n);
    sqlite3_bind_int64(st, 2, l);
    sqlite3_step(st);
    sqlite3_finalize(st);
    return Result::success();
}

std::vector<NodeLayer> DbManagerSQLite::getNodesInLayer(LayerId l) {
    std::vector<NodeLayer> out;
    sqlite3_stmt* st;

    sqlite3_prepare_v2(
        db_,
        "SELECT node_id,layer_id FROM node_layers WHERE layer_id=?",
        -1, &st, nullptr);

    sqlite3_bind_int64(st, 1, l);

    while (sqlite3_step(st) == SQLITE_ROW) {
        NodeLayer nl;
        nl.nodeId  = sqlite3_column_int64(st, 0);
        nl.layerId = sqlite3_column_int64(st, 1);
        out.push_back(nl);
    }

    sqlite3_finalize(st);
    return out;
}

/* ============================================================
   Edges
   ============================================================ */

Result DbManagerSQLite::createEdge(const EdgeData& e, EdgeId& outId) {
    sqlite3_stmt* st;
    sqlite3_prepare_v2(
        db_,
        "INSERT INTO edges(src_node_id,src_layer_id,dst_node_id,dst_layer_id,"
        "edge_type,metadata,attributes,checksum,status,reviewer)"
        "VALUES(?,?,?,?,?,?,?,?,?,?)",
        -1, &st, nullptr);

    sqlite3_bind_int64(st, 1, e.srcNode);
    sqlite3_bind_int64(st, 2, e.srcLayer);
    sqlite3_bind_int64(st, 3, e.dstNode);
    sqlite3_bind_int64(st, 4, e.dstLayer);
    sqlite3_bind_text(st, 5, e.edgeType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 6, e.metadata.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 7, e.attributes.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 8, e.checksum);
    sqlite3_bind_text(st, 9, to_string(e.status).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 10, e.reviewer.empty() ? nullptr : e.reviewer.c_str(),
                      -1, SQLITE_TRANSIENT);

    sqlite3_step(st);
    sqlite3_finalize(st);
    outId = sqlite3_last_insert_rowid(db_);
    return Result::success();
}

Result DbManagerSQLite::updateEdge(const EdgeData& e) {
    sqlite3_stmt* st;
    sqlite3_prepare_v2(
        db_,
        "UPDATE edges SET src_node_id=?,src_layer_id=?,dst_node_id=?,dst_layer_id=?,"
        "edge_type=?,metadata=?,attributes=?,checksum=?,status=?,reviewer=? WHERE id=?",
        -1, &st, nullptr);

    sqlite3_bind_int64(st, 1, e.srcNode);
    sqlite3_bind_int64(st, 2, e.srcLayer);
    sqlite3_bind_int64(st, 3, e.dstNode);
    sqlite3_bind_int64(st, 4, e.dstLayer);
    sqlite3_bind_text(st, 5, e.edgeType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 6, e.metadata.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 7, e.attributes.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 8, e.checksum);
    sqlite3_bind_text(st, 9, to_string(e.status).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 10, e.reviewer.empty() ? nullptr : e.reviewer.c_str(),
                      -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 11, e.id);

    sqlite3_step(st);
    sqlite3_finalize(st);
    return Result::success();
}

Result DbManagerSQLite::deleteEdge(EdgeId id) {
    sqlite3_stmt* st;
    sqlite3_prepare_v2(db_, "DELETE FROM edges WHERE id=?", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, id);
    sqlite3_step(st);
    sqlite3_finalize(st);
    return Result::success();
}

std::vector<EdgeData> DbManagerSQLite::getAllEdges() {
    std::vector<EdgeData> out;
    sqlite3_stmt* st;

    sqlite3_prepare_v2(
        db_,
        "SELECT id,src_node_id,src_layer_id,dst_node_id,dst_layer_id,"
        "edge_type,metadata,attributes,checksum,status,reviewer FROM edges",
        -1, &st, nullptr);

    while (sqlite3_step(st) == SQLITE_ROW) {
        EdgeData e;
        e.id        = sqlite3_column_int64(st, 0);
        e.srcNode   = sqlite3_column_int64(st, 1);
        e.srcLayer  = sqlite3_column_int64(st, 2);
        e.dstNode   = sqlite3_column_int64(st, 3);
        e.dstLayer  = sqlite3_column_int64(st, 4);
        e.edgeType  = (const char*)sqlite3_column_text(st, 5);
        e.metadata  = (const char*)sqlite3_column_text(st, 6);
        e.attributes= (const char*)sqlite3_column_text(st, 7);
        e.checksum  = sqlite3_column_int(st, 8);
        e.status    = status_from_string(
                          (const char*)sqlite3_column_text(st, 9));
        if (sqlite3_column_text(st, 10))
            e.reviewer = (const char*)sqlite3_column_text(st, 10);
        out.push_back(e);
    }

    sqlite3_finalize(st);
    return out;
}
