#include <iostream>
#include <sstream>
#include <string>
#include <cstring>

#include <readline/readline.h>
#include <readline/history.h>

#include "core/ArchitectureModel.h"
#include "db/DbManagerSQLite.h"

/* ============================================================
   Commands
   ============================================================ */
enum class Command {
    ADD_NODE, UPDATE_NODE, DELETE_NODE, LIST_NODES,
    ADD_LAYER, UPDATE_LAYER, DELETE_LAYER, LIST_LAYERS,
    ADD_NODE_LAYER, REMOVE_NODE_LAYER, LIST_LAYER_NODES,
    ADD_EDGE, UPDATE_EDGE, DELETE_EDGE, LIST_EDGES,
    HELP, EXIT, UNKNOWN
};

static Command parseCommand(const std::string& c) {
    if (c == "add_node") return Command::ADD_NODE;
    if (c == "update_node") return Command::UPDATE_NODE;
    if (c == "del_node") return Command::DELETE_NODE;
    if (c == "list_nodes") return Command::LIST_NODES;

    if (c == "add_layer") return Command::ADD_LAYER;
    if (c == "update_layer") return Command::UPDATE_LAYER;
    if (c == "del_layer") return Command::DELETE_LAYER;
    if (c == "list_layers") return Command::LIST_LAYERS;

    if (c == "add_node_layer") return Command::ADD_NODE_LAYER;
    if (c == "del_node_layer") return Command::REMOVE_NODE_LAYER;
    if (c == "list_layer_nodes") return Command::LIST_LAYER_NODES;

    if (c == "add_edge") return Command::ADD_EDGE;
    if (c == "update_edge") return Command::UPDATE_EDGE;
    if (c == "del_edge") return Command::DELETE_EDGE;
    if (c == "list_edges") return Command::LIST_EDGES;

    if (c == "help") return Command::HELP;
    if (c == "exit") return Command::EXIT;

    return Command::UNKNOWN;
}

/* ============================================================
   Readline completion
   ============================================================ */
static const char* COMMANDS[] = {
    "add_node", "update_node", "del_node", "list_nodes",
    "add_layer", "update_layer", "del_layer", "list_layers",
    "add_node_layer", "del_node_layer", "list_layer_nodes",
    "add_edge", "update_edge", "del_edge", "list_edges",
    "help", "exit",
    nullptr
};

static char* commandGenerator(const char* text, int state) {
    static int index;
    static size_t len;

    if (!state) {
        index = 0;
        len = std::strlen(text);
    }

    const char* name;
    while ((name = COMMANDS[index++])) {
        if (std::strncmp(name, text, len) == 0)
            return strdup(name);
    }
    return nullptr;
}

static char** commandCompletion(const char* text, int start, int) {
    if (start == 0)
        return rl_completion_matches(text, commandGenerator);
    return nullptr;
}

/* ============================================================
   Helpers
   ============================================================ */
static void printResult(const Result& r) {
    std::cout << (r.ok ? "[OK] " : "[ERR] ") << r.message << "\n";
}

static void usage(const char* msg) {
    std::cout << "[ERR] Usage: " << msg << "\n";
}

static void help() {
    std::cout << R"(

Nodes:
  add_node <name> <type>
  update_node <id> <name> <type>
  del_node <id>
  list_nodes

Layers:
  add_layer <name> <kind>
  update_layer <id> <name> <kind>
  del_layer <id>
  list_layers

Node–Layer:
  add_node_layer <nodeId> <layerId>
  del_node_layer <nodeId> <layerId>
  list_layer_nodes <layerId>

Edges (layer-aware):
  add_edge <srcNode> <srcLayer> <dstNode> <dstLayer> <type>
  update_edge <id> <type>
  del_edge <id>
  list_edges

General:
  help
  exit

)";
}

/* ============================================================
   Main
   ============================================================ */
int main(int argc, char* argv[]) {
    std::string dbFile = "arch.db";
    if (argc > 1)
        dbFile = argv[1];

    DbManagerSQLite db;
    Result r = db.open(dbFile);
    printResult(r);
    if (!r.ok) return 1;

    ArchitectureModel model(db);

    rl_attempted_completion_function = commandCompletion;
    using_history();

    help();

    while (true) {
        char* input = readline("> ");
        if (!input)
            break;

        std::string line(input);
        free(input);

        if (line.empty())
            continue;

        add_history(line.c_str());

        std::istringstream is(line);
        std::string cmdStr;
        is >> cmdStr;

        Command cmd = parseCommand(cmdStr);

        switch (cmd) {
        case Command::HELP:
            help();
            break;

        case Command::EXIT:
            return 0;

        case Command::ADD_NODE: {
            NodeData n; NodeId id;
            if (!(is >> n.name >> n.type)) {
                usage("add_node <name> <type>");
                break;
            }
            printResult(model.addNode(n, id));
            break;
        }

        case Command::UPDATE_NODE: {
            NodeData n;
            if (!(is >> n.id >> n.name >> n.type)) {
                usage("update_node <id> <name> <type>");
                break;
            }
            printResult(model.updateNode(n));
            break;
        }

        case Command::DELETE_NODE: {
            NodeId id;
            if (!(is >> id)) {
                usage("del_node <id>");
                break;
            }
            printResult(model.deleteNode(id));
            break;
        }

        case Command::LIST_NODES:
            for (auto& n : model.nodes())
                std::cout << n.id << " " << n.name << " " << n.type << "\n";
            break;

        case Command::ADD_LAYER: {
            LayerData l; LayerId id;
            if (!(is >> l.name >> l.kind)) {
                usage("add_layer <name> <kind>");
                break;
            }
            printResult(model.addLayer(l, id));
            break;
        }

        case Command::UPDATE_LAYER: {
            LayerData l;
            if (!(is >> l.id >> l.name >> l.kind)) {
                usage("update_layer <id> <name> <kind>");
                break;
            }
            printResult(model.updateLayer(l));
            break;
        }

        case Command::DELETE_LAYER: {
            LayerId id;
            if (!(is >> id)) {
                usage("del_layer <id>");
                break;
            }
            printResult(model.deleteLayer(id));
            break;
        }

        case Command::LIST_LAYERS:
            for (auto& l : model.layers())
                std::cout << l.id << " " << l.name << " " << l.kind << "\n";
            break;

        case Command::ADD_NODE_LAYER: {
            NodeId n; LayerId l;
            if (!(is >> n >> l)) {
                usage("add_node_layer <nodeId> <layerId>");
                break;
            }
            printResult(model.addNodeToLayer(n, l));
            break;
        }

        case Command::REMOVE_NODE_LAYER: {
            NodeId n; LayerId l;
            if (!(is >> n >> l)) {
                usage("del_node_layer <nodeId> <layerId>");
                break;
            }
            printResult(model.removeNodeFromLayer(n, l));
            break;
        }

        case Command::LIST_LAYER_NODES: {
            LayerId l;
            if (!(is >> l)) {
                usage("list_layer_nodes <layerId>");
                break;
            }
            for (auto& nl : model.nodesInLayer(l))
                std::cout << nl.nodeId << "\n";
            break;
        }

        case Command::ADD_EDGE: {
            EdgeData e; EdgeId id;
            if (!(is >> e.srcNode >> e.srcLayer
                     >> e.dstNode >> e.dstLayer
                     >> e.edgeType)) {
                usage("add_edge <srcNode> <srcLayer> <dstNode> <dstLayer> <type>");
                break;
            }
            printResult(model.addEdge(e, id));
            break;
        }

        case Command::UPDATE_EDGE: {
            EdgeData e;
            if (!(is >> e.id >> e.edgeType)) {
                usage("update_edge <id> <type>");
                break;
            }
            printResult(model.updateEdge(e));
            break;
        }

        case Command::DELETE_EDGE: {
            EdgeId id;
            if (!(is >> id)) {
                usage("del_edge <id>");
                break;
            }
            printResult(model.deleteEdge(id));
            break;
        }

        case Command::LIST_EDGES:
            for (auto& e : model.edges()) {
                std::cout << e.id << ": ("
                          << e.srcNode << "," << e.srcLayer << ") -> ("
                          << e.dstNode << "," << e.dstLayer << ") "
                          << e.edgeType << "\n";
            }
            break;

        case Command::UNKNOWN:
        default:
            std::cout << "[ERR] Unknown command. Type 'help'.\n";
            break;
        }
    }
}
