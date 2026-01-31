#include <iostream>
#include <sstream>
#include "core/ArchitectureModel.h"
#include "db/DbManagerSQLite.h"

void printResult(const Result& r) {
    std::cout << (r.ok ? "[OK] " : "[ERR] ") << r.message << "\n";
}

void help() {
    std::cout << R"(
Commands:

 Nodes:
   add_node <name> <type>
   update_node <id> <name> <type>
   set_node_meta <id> <key> <value>
   set_node_attr <id> <key> <value>
   list_nodes
   del_node <id>

 Layers:
   add_layer <name> <kind>
   update_layer <id> <name> <kind>
   set_layer_meta <id> <key> <value>
   set_layer_attr <id> <key> <value>
   list_layers
   del_layer <id>

 Edges:
   add_edge <srcNodeId> <dstNodeId> <edgeType>
   update_edge <id> <edgeType>
   set_edge_meta <id> <key> <value>
   set_edge_attr <id> <key> <value>
   list_edges
   del_edge <id>

 General:
   help
   exit | quit
)";
}

int main() {
    DbManagerSQLite db;
    auto r = db.open("arch.db");
    printResult(r);
    if (!r.ok) return 1;

    ArchitectureModel model(db);
    help();

    std::string line;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, line);
        if (!std::cin.good()) break;
        if (line.empty()) continue;

        std::istringstream is(line);
        std::string cmd;
        is >> cmd;

        if (cmd == "help") {
            help();
        }

        // ---------- Nodes ----------
        else if (cmd == "add_node") {
            NodeData n; NodeId id;
            is >> n.name >> n.type;
            printResult(model.addNode(n, id));
        }
        else if (cmd == "update_node") {
            NodeData n;
            is >> n.id >> n.name >> n.type;
            printResult(model.updateNode(n));
        }
        else if (cmd == "set_node_meta") {
            NodeData n; std::string k,v;
            is >> n.id >> k >> v;
            n.metadata[k] = v;
            printResult(model.updateNode(n));
        }
        else if (cmd == "set_node_attr") {
            NodeData n; std::string k,v;
            is >> n.id >> k >> v;
            n.attributes[k] = v;
            printResult(model.updateNode(n));
        }
        else if (cmd == "list_nodes") {
            for (auto& n : model.nodes())
                std::cout << n.id << " " << n.name << " " << n.type << "\n";
        }
        else if (cmd == "del_node") {
            NodeId id; is >> id;
            printResult(model.deleteNode(id));
        }

        // ---------- Layers ----------
        else if (cmd == "add_layer") {
            LayerData l; LayerId id;
            is >> l.name >> l.kind;
            printResult(model.addLayer(l, id));
        }
        else if (cmd == "update_layer") {
            LayerData l;
            is >> l.id >> l.name >> l.kind;
            printResult(model.updateLayer(l));
        }
        else if (cmd == "set_layer_meta") {
            LayerData l; std::string k,v;
            is >> l.id >> k >> v;
            l.metadata[k] = v;
            printResult(model.updateLayer(l));
        }
        else if (cmd == "set_layer_attr") {
            LayerData l; std::string k,v;
            is >> l.id >> k >> v;
            l.attributes[k] = v;
            printResult(model.updateLayer(l));
        }
        else if (cmd == "list_layers") {
            for (auto& l : model.layers())
                std::cout << l.id << " " << l.name << " " << l.kind << "\n";
        }
        else if (cmd == "del_layer") {
            LayerId id; is >> id;
            printResult(model.deleteLayer(id));
        }

        // ---------- Edges ----------
        else if (cmd == "add_edge") {
            EdgeData e; EdgeId id;
            is >> e.srcNode >> e.dstNode >> e.edgeType;
            printResult(model.addEdge(e, id));
        }
        else if (cmd == "update_edge") {
            EdgeData e;
            is >> e.id >> e.edgeType;
            printResult(model.updateEdge(e));
        }
        else if (cmd == "set_edge_meta") {
            EdgeData e; std::string k,v;
            is >> e.id >> k >> v;
            e.metadata[k] = v;
            printResult(model.updateEdge(e));
        }
        else if (cmd == "set_edge_attr") {
            EdgeData e; std::string k,v;
            is >> e.id >> k >> v;
            e.attributes[k] = v;
            printResult(model.updateEdge(e));
        }
        else if (cmd == "list_edges") {
            for (auto& e : model.edges())
                std::cout << e.id << " " << e.srcNode << "->" << e.dstNode
                          << " " << e.edgeType << "\n";
        }
        else if (cmd == "del_edge") {
            EdgeId id; is >> id;
            printResult(model.deleteEdge(id));
        }

        else if (cmd == "exit" || cmd == "quit") {
            return 0;
        }
        else {
            std::cout << "[ERR] Unknown command. Type 'help'.\n";
        }
    }
}
