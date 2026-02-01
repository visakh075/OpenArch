#include "GraphJson.h"
#include <sstream>

static std::string esc(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"') o += "\\\"";
        else if (c == '\n') o += "\\n";
        else o += c;
    }
    return o;
}

std::string toJson(const GraphSnapshot& snap) {
    std::ostringstream os;

    os << "{\n";
    os << "  \"nodes\": [\n";

    for (size_t i = 0; i < snap.nodes.size(); ++i) {
        const auto& n = snap.nodes[i];
        os << "    {\n";
        os << "      \"id\": " << n.id << ",\n";
        os << "      \"label\": \"" << esc(n.label) << "\",\n";
        os << "      \"type\": \"" << esc(n.type) << "\",\n";
        os << "      \"status\": \"" << to_string(n.status) << "\",\n";
        os << "      \"reviewer\": \"" << esc(n.reviewer) << "\",\n";

        os << "      \"layers\": [";
        for (size_t j = 0; j < n.layers.size(); ++j) {
            os << n.layers[j];
            if (j + 1 < n.layers.size()) os << ",";
        }
        os << "],\n";

        os << "      \"metadata\": \"" << esc(n.metadata) << "\",\n";
        os << "      \"attributes\": \"" << esc(n.attributes) << "\"\n";
        os << "    }";
        if (i + 1 < snap.nodes.size()) os << ",";
        os << "\n";
    }

    os << "  ],\n";
    os << "  \"edges\": [\n";

    for (size_t i = 0; i < snap.edges.size(); ++i) {
        const auto& e = snap.edges[i];
        os << "    {\n";
        os << "      \"id\": " << e.id << ",\n";
        os << "      \"srcNode\": " << e.srcNode << ",\n";
        os << "      \"srcLayer\": " << e.srcLayer << ",\n";
        os << "      \"dstNode\": " << e.dstNode << ",\n";
        os << "      \"dstLayer\": " << e.dstLayer << ",\n";
        os << "      \"type\": \"" << esc(e.type) << "\",\n";
        os << "      \"status\": \"" << to_string(e.status) << "\",\n";
        os << "      \"metadata\": \"" << esc(e.metadata) << "\"\n";
        os << "    }";
        if (i + 1 < snap.edges.size()) os << ",";
        os << "\n";
    }

    os << "  ]\n";
    os << "}\n";

    return os.str();
}
