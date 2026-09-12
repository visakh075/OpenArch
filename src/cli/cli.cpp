#include <iostream>
#include <sstream>
#include <vector>
#include <memory>
#include <cstring>
#include <string>

#include <readline/readline.h>
#include <readline/history.h>

#include "core/ArchitectureModel.h"
#include "core/GraphJson.h"
#include "db/DbManager.h"
#include "db/DbManagerSQLite.h"
#include "db/DbManagerJson.h"

static const char* COMMANDS[] = {
    "add_node", "update_node", "del_node", "list_nodes",
    "set_node_meta", "set_node_attr", "review_node",

    "add_layer", "update_layer", "del_layer", "list_layers",
    "set_layer_meta", "set_layer_attr", "review_layer",

    "add_node_layer", "del_node_layer", "list_layer_nodes",

    "add_edge", "update_edge", "del_edge", "list_edges",
    "set_edge_meta", "set_edge_attr", "review_edge",

    "dump_graph", "dump_graph_json",
    "help", "exit",
    nullptr
};

/* ============================================================
   Command enum
   ============================================================ */
enum class Command {
    ADD_NODE, UPDATE_NODE, DELETE_NODE, LIST_NODES,
    SET_NODE_META, SET_NODE_ATTR, REVIEW_NODE,

    ADD_LAYER, UPDATE_LAYER, DELETE_LAYER, LIST_LAYERS,
    SET_LAYER_META, SET_LAYER_ATTR, REVIEW_LAYER,

    ADD_NODE_LAYER, REMOVE_NODE_LAYER, LIST_LAYER_NODES,

    ADD_EDGE, UPDATE_EDGE, DELETE_EDGE, LIST_EDGES,
    SET_EDGE_META, SET_EDGE_ATTR, REVIEW_EDGE,
    
    DUMP_GRAPH,
    DUMP_GRAPH_JSON,

    HELP, EXIT, UNKNOWN
};

static char* command_generator(const char* text, int state) {
    static int index;
    static size_t len;

    if (state == 0) {
        index = 0;
        len = strlen(text);
    }

    const char* name;
    while ((name = COMMANDS[index++])) {
        if (strncmp(name, text, len) == 0)
            return strdup(name);
    }

    return nullptr;
}

static char** cli_completion(const char* text, int start, int end) {
    (void)end;
    if (start == 0)
        return rl_completion_matches(text, command_generator);
    return nullptr;
}

/* ============================================================
   Parsing
   ============================================================ */
static Command parse(const std::string& c) {
    #define CMD(x,s) if(c==s) return Command::x
    CMD(ADD_NODE,"add_node"); CMD(UPDATE_NODE,"update_node");
    CMD(DELETE_NODE,"del_node"); CMD(LIST_NODES,"list_nodes");
    CMD(SET_NODE_META,"set_node_meta"); CMD(SET_NODE_ATTR,"set_node_attr");
    CMD(REVIEW_NODE,"review_node");

    CMD(ADD_LAYER,"add_layer"); CMD(UPDATE_LAYER,"update_layer");
    CMD(DELETE_LAYER,"del_layer"); CMD(LIST_LAYERS,"list_layers");
    CMD(SET_LAYER_META,"set_layer_meta"); CMD(SET_LAYER_ATTR,"set_layer_attr");
    CMD(REVIEW_LAYER,"review_layer");

    CMD(ADD_NODE_LAYER,"add_node_layer");
    CMD(REMOVE_NODE_LAYER,"del_node_layer");
    CMD(LIST_LAYER_NODES,"list_layer_nodes");

    CMD(ADD_EDGE,"add_edge"); CMD(UPDATE_EDGE,"update_edge");
    CMD(DELETE_EDGE,"del_edge"); CMD(LIST_EDGES,"list_edges");
    CMD(SET_EDGE_META,"set_edge_meta"); CMD(SET_EDGE_ATTR,"set_edge_attr");
    CMD(REVIEW_EDGE,"review_edge");

    CMD(DUMP_GRAPH,"dump_graph");
    CMD(DUMP_GRAPH_JSON,"dump_graph_json");
    CMD(HELP,"help"); CMD(EXIT,"exit");
    return Command::UNKNOWN;
}

static void printResult(const Result& r) {
    std::cout << (r.ok ? "[OK] " : "[ERR] ") << r.message << "\n";
}

/* ============================================================
   Help
   ============================================================ */
static void help() {
    std::cout << R"(
Nodes:
  add_node <name> <type> [parentId|-]
  update_node <id> <name> <type> [parentId|-]
  set_node_meta <id> <json>
  set_node_attr <id> <json>
  review_node <id> <reviewer>
  del_node <id>
  list_nodes

Layers:
  add_layer <name> <kind>
  update_layer <id> <name> <kind>
  set_layer_meta <id> <json>
  set_layer_attr <id> <json>
  review_layer <id> <reviewer>
  del_layer <id>
  list_layers

Node-Layer:
  add_node_layer <nodeId> <layerId>
  del_node_layer <nodeId> <layerId>
  list_layer_nodes <layerId>

Edges:
  add_edge <srcNode> <srcLayer> <dstNode> <dstLayer> <type>
  update_edge <id> <type>
  set_edge_meta <id> <json>
  set_edge_attr <id> <json>
  review_edge <id> <reviewer>
  del_edge <id>
  list_edges

Graph:
  dump_graph [layerId]
  dump_graph_json [layerId]

General:
  help
  exit
)";
}

/* ============================================================
   Main
   ============================================================ */
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: openarch <dbfile.db|architecture.json>\n";
        return 1;
    }

    std::string dbPath = argv[1];
    std::unique_ptr<DbManager> db;

    // Backend selection based on file extension
    std::string ext;
    auto dotPos = dbPath.find_last_of('.');
    if (dotPos != std::string::npos) {
        ext = dbPath.substr(dotPos);
    }

    if (ext == ".db" || ext == ".sqlite" || ext == ".sqlite3") {
        db = std::make_unique<DbManagerSQLite>();
    } else {
        db = std::make_unique<DbManagerJson>();
    }

    Result r = db->open(dbPath);
    if (!r.ok) {
        std::cerr << "[ERR] " << r.message << "\n";
        return 1;
    }

    ArchitectureModel model(*db);
    using_history();
    help();
    rl_attempted_completion_function = cli_completion;

    while (true) {
        char* input = readline("> ");
        if (!input) break;

        std::string line(input);
        free(input);

        if (line.empty()) continue;
        add_history(line.c_str());

        std::istringstream is(line);
        std::string cmdStr;
        is >> cmdStr;

        Command cmd = parse(cmdStr);

        switch (cmd) {

        case Command::HELP: help(); break;
        case Command::EXIT: return 0;

        /* ===================== Nodes ===================== */
        case Command::ADD_NODE: {
            NodeData n; NodeId id;
            is >> n.name >> n.type;
            
            std::string parentStr;
            if (is >> parentStr && parentStr != "-" && parentStr != "null") {
                try {
                    n.parentId = std::stoull(parentStr);
                } catch (...) {
                    n.parentId = std::nullopt;
                }
            } else {
                n.parentId = std::nullopt;
            }

            printResult(model.addNode(n, id));
            break;
        }

        case Command::UPDATE_NODE: {
            NodeData n;
            is >> n.id >> n.name >> n.type;

            std::string parentStr;
            if (is >> parentStr && parentStr != "-" && parentStr != "null") {
                try {
                    n.parentId = std::stoull(parentStr);
                } catch (...) {
                    n.parentId = std::nullopt;
                }
            } else {
                n.parentId = std::nullopt;
            }

            printResult(model.updateNode(n));
            break;
        }

        case Command::DELETE_NODE: {
            NodeId id;
            if (is >> id)
                printResult(model.deleteNode(id));
            else
                std::cout << "[ERR] Usage: del_node <id>\n";
            break;
        }

        case Command::SET_NODE_META: {
            NodeId id; std::string meta;
            is >> id; std::getline(is, meta);
            printResult(model.setNodeMetadata(id, meta));
            break;
        }

        case Command::SET_NODE_ATTR: {
            NodeId id; std::string attr;
            is >> id; std::getline(is, attr);
            printResult(model.setNodeAttributes(id, attr));
            break;
        }

        case Command::REVIEW_NODE: {
            NodeId id; std::string reviewer;
            is >> id >> reviewer;
            printResult(model.reviewNode(id, reviewer));
            break;
        }

        case Command::LIST_NODES:
            for (auto& n : model.nodes()) {
                std::string parentInfo = n.parentId ? std::to_string(*n.parentId) : "none";
                std::cout << n.id << " " << n.name << " " << n.type
                          << " [parent=" << parentInfo << "]"
                          << " [" << to_string(n.status) << "]"
                          << " chk=" << n.checksum
                          << " reviewer=" << n.reviewer << "\n";
            }
            break;

        /* ===================== Layers ===================== */
        case Command::ADD_LAYER: {
            LayerData l; LayerId id;
            is >> l.name >> l.kind;
            printResult(model.addLayer(l, id));
            break;
        }

        case Command::UPDATE_LAYER: {
            LayerData l;
            is >> l.id >> l.name >> l.kind;
            printResult(model.updateLayer(l));
            break;
        }

        case Command::DELETE_LAYER: {
            LayerId id;
            if (is >> id)
                printResult(model.deleteLayer(id));
            else
                std::cout << "[ERR] Usage: del_layer <id>\n";
            break;
        }

        case Command::SET_LAYER_META: {
            LayerId id; std::string meta;
            is >> id; std::getline(is, meta);
            printResult(model.setLayerMetadata(id, meta));
            break;
        }

        case Command::SET_LAYER_ATTR: {
            LayerId id; std::string attr;
            is >> id; std::getline(is, attr);
            printResult(model.setLayerAttributes(id, attr));
            break;
        }

        case Command::REVIEW_LAYER: {
            LayerId id; std::string reviewer;
            is >> id >> reviewer;
            printResult(model.reviewLayer(id, reviewer));
            break;
        }

        case Command::LIST_LAYERS:
            for (auto& l : model.layers())
                std::cout << l.id << " " << l.name << " " << l.kind
                          << " [" << to_string(l.status)
                          << "] chk=" << l.checksum
                          << " reviewer=" << l.reviewer << "\n";
            break;

        /* ===================== Node–Layer ===================== */
        case Command::ADD_NODE_LAYER: {
            NodeId n; LayerId l;
            is >> n >> l;
            printResult(model.addNodeToLayer(n, l));
            break;
        }

        case Command::REMOVE_NODE_LAYER: {
            NodeId n; LayerId l;
            is >> n >> l;
            printResult(model.removeNodeFromLayer(n, l));
            break;
        }

        case Command::LIST_LAYER_NODES: {
            LayerId l; is >> l;
            for (auto& nl : model.nodesInLayer(l))
                std::cout << nl.nodeId << "\n";
            break;
        }

        /* ===================== Edges ===================== */
        case Command::ADD_EDGE: {
            EdgeData e; EdgeId id;
            is >> e.srcNode >> e.srcLayer
               >> e.dstNode >> e.dstLayer
               >> e.edgeType;
            printResult(model.addEdge(e, id));
            break;
        }

        case Command::UPDATE_EDGE: {
            EdgeData e;
            is >> e.id >> e.edgeType;
            printResult(model.updateEdge(e));
            break;
        }

        case Command::DELETE_EDGE: {
            EdgeId id;
            if (is >> id)
                printResult(model.deleteEdge(id));
            else
                std::cout << "[ERR] Usage: del_edge <id>\n";
            break;
        }

        case Command::SET_EDGE_META: {
            EdgeId id; std::string meta;
            is >> id; std::getline(is, meta);
            printResult(model.setEdgeMetadata(id, meta));
            break;
        }

        case Command::SET_EDGE_ATTR: {
            EdgeId id; std::string attr;
            is >> id; std::getline(is, attr);
            printResult(model.setEdgeAttributes(id, attr));
            break;
        }

        case Command::REVIEW_EDGE: {
            EdgeId id; std::string reviewer;
            is >> id >> reviewer;
            printResult(model.reviewEdge(id, reviewer));
            break;
        }

        case Command::LIST_EDGES:
            for (auto& e : model.edges())
                std::cout << e.id << ": ("
                          << e.srcNode << "," << e.srcLayer << ") -> ("
                          << e.dstNode << "," << e.dstLayer << ") "
                          << e.edgeType << " ["
                          << to_string(e.status) << "] chk="
                          << e.checksum << " reviewer="
                          << e.reviewer << "\n";
            break;

        /* ===================== Graph Dumps ===================== */
        case Command::DUMP_GRAPH: {
            std::optional<LayerId> layer;

            if (!is.eof()) {
                LayerId l;
                if (is >> l)
                    layer = l;
            }

            auto snap = model.extractGraph(layer);

            std::cout << "Nodes:\n";
            for (const auto& n : snap.nodes) {
                std::string parentInfo = n.parentId ? std::to_string(*n.parentId) : "none";
                std::cout << "  [" << n.id << "] "
                          << n.name << " type=" << n.type
                          << " parent=" << parentInfo
                          << " status=" << to_string(n.status)
                          << "\n";
            }

            std::cout << "Edges:\n";
            for (const auto& e : snap.edges) {
                std::cout << "  [" << e.id << "] ("
                          << e.srcNode << ":" << e.srcLayer
                          << " -> "
                          << e.dstNode << ":" << e.dstLayer
                          << ") type=" << e.edgeType
                          << " status=" << to_string(e.status)
                          << "\n";
            }
            break;
        }

        case Command::DUMP_GRAPH_JSON: {
            std::optional<LayerId> layer;

            if (!is.eof()) {
                LayerId l;
                if (is >> l)
                    layer = l;
            }

            auto snap = model.extractGraph(layer);
            std::cout << toJson(snap) << "\n";
            break;
        }

        default:
            std::cout << "[ERR] Unknown command. Type 'help'.\n";
        }
    }
}