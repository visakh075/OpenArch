#include "GraphJson.h"
#include "core/Types.h"

#include <sstream>

static std::string esc(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        switch (c) {
        case '"':  o << "\\\""; break;
        case '\\': o << "\\\\"; break;
        case '\n': o << "\\n";  break;
        case '\r': o << "\\r";  break;
        case '\t': o << "\\t";  break;
        default:   o << c;      break;
        }
    }
    return o.str();
}

std::string toJson(const GraphSnapshot& g) {
    std::ostringstream os;

    os << "{\n";

    // ---------------- Nodes ----------------
    os << "  \"nodes\": [\n";
    for (size_t i = 0; i < g.nodes.size(); ++i) {
        const auto& n = g.nodes[i];
        os << "    {\n";
        os << "      \"id\": " << n.id << ",\n";
        os << "      \"name\": \"" << esc(n.name) << "\",\n";
        os << "      \"type\": \"" << esc(n.type) << "\",\n";
        os << "      \"metadata\": \"" << esc(n.metadata) << "\",\n";
        os << "      \"attributes\": \"" << esc(n.attributes) << "\",\n";
        os << "      \"checksum\": " << n.checksum << ",\n";
        os << "      \"status\": \"" << to_string(n.status) << "\",\n";
        os << "      \"reviewer\": \"" << esc(n.reviewer) << "\"\n";
        os << "    }";
        if (i + 1 < g.nodes.size()) os << ",";
        os << "\n";
    }
    os << "  ],\n";

    // ---------------- Edges ----------------
    os << "  \"edges\": [\n";
    for (size_t i = 0; i < g.edges.size(); ++i) {
        const auto& e = g.edges[i];
        os << "    {\n";
        os << "      \"id\": " << e.id << ",\n";
        os << "      \"srcNode\": " << e.srcNode << ",\n";
        os << "      \"srcLayer\": " << e.srcLayer << ",\n";
        os << "      \"dstNode\": " << e.dstNode << ",\n";
        os << "      \"dstLayer\": " << e.dstLayer << ",\n";
        os << "      \"edgeType\": \"" << esc(e.edgeType) << "\",\n";
        os << "      \"metadata\": \"" << esc(e.metadata) << "\",\n";
        os << "      \"attributes\": \"" << esc(e.attributes) << "\",\n";
        os << "      \"checksum\": " << e.checksum << ",\n";
        os << "      \"status\": \"" << to_string(e.status) << "\",\n";
        os << "      \"reviewer\": \"" << esc(e.reviewer) << "\"\n";
        os << "    }";
        if (i + 1 < g.edges.size()) os << ",";
        os << "\n";
    }
    os << "  ]\n";

    os << "}\n";
    return os.str();
}
