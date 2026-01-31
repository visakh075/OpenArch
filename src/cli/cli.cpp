#include <iostream>
#include <sstream>
#include <string>

#include "core/ArchitectureModel.h"
#include "db/DbManagerSQLite.h"

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

static void printResult(const Result& r) {
    std::cout << (r.ok ? "[OK] " : "[ERR] ") << r.message << "\n";
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

int main() {
    DbManagerSQLite db;
    Result r = db.open("arch.db");
    printResult(r);
    if (!r.ok) return 1;

    ArchitectureModel model(db);
    help();

    std::string line;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, line);

        if (!std::cin.good())
            break;

        if (line.empty())
            continue;

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
            is >> n.name >> n.type;
            printResult(model.addNode(n, id));
            break;
        }

        case Command::UPDATE_NODE: {
            NodeData n;
            is >> n.id >> n.name >> n.type;
            printResult(model.updateNode(n));
            break;
        }

        case Command::DELETE_NODE: {
            NodeId id;
            is >> id;
            printResult(model.deleteNode(id));
            break;
        }

        case Command::LIST_NODES:
            for (auto& n : model.nodes())
                std::cout << n.id << " " << n.name << " " << n.type << "\n";
            break;

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
            is >> id;
            printResult(model.deleteLayer(id));
            break;
        }

        case Command::LIST_LAYERS:
            for (auto& l : model.layers())
                std::cout << l.id << " " << l.name << " " << l.kind << "\n";
            break;

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
            LayerId l;
            is >> l;
            for (auto& nl : model.nodesInLayer(l))
                std::cout << nl.nodeId << "\n";
            break;
        }

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
            is >> id;
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
