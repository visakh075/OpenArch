#include "DbManagerJson.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

/* ============================================================
   Lightweight JSON Parsing / Serialization Helpers
   ============================================================ */

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

static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

static std::string unescapeJson(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char next = s[++i];
            switch (next) {
            case '"':  out += '"'; break;
            case '\\': out += '\\'; break;
            case '/':  out += '/'; break;
            case 'b':  out += '\b'; break;
            case 'f':  out += '\f'; break;
            case 'n':  out += '\n'; break;
            case 'r':  out += '\r'; break;
            case 't':  out += '\t'; break;
            default:   out += next; break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

static std::string extractField(const std::string& jsonBlock, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t pos = jsonBlock.find(searchKey);
    if (pos == std::string::npos) return "";

    pos = jsonBlock.find(':', pos);
    if (pos == std::string::npos) return "";
    ++pos;

    while (pos < jsonBlock.size() && (jsonBlock[pos] == ' ' || jsonBlock[pos] == '\t' || jsonBlock[pos] == '\r' || jsonBlock[pos] == '\n')) {
        ++pos;
    }
    if (pos >= jsonBlock.size()) return "";

    if (jsonBlock[pos] == '"') {
        ++pos;
        std::string val;
        bool escaped = false;
        while (pos < jsonBlock.size()) {
            char c = jsonBlock[pos++];
            if (escaped) {
                val += '\\';
                val += c;
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                break;
            } else {
                val += c;
            }
        }
        return unescapeJson(val);
    } else {
        size_t end = jsonBlock.find_first_of(",}\n\r", pos);
        if (end == std::string::npos) end = jsonBlock.size();
        return trim(jsonBlock.substr(pos, end - pos));
    }
}

static std::vector<std::string> extractObjectList(const std::string& json, const std::string& arrayName) {
    std::vector<std::string> objects;
    std::string searchKey = "\"" + arrayName + "\"";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return objects;

    pos = json.find('[', pos);
    if (pos == std::string::npos) return objects;

    int depth = 0;
    size_t objStart = std::string::npos;
    bool inString = false;
    bool escaped = false;

    for (size_t i = pos + 1; i < json.size(); ++i) {
        char c = json[i];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (c == '\\') {
            escaped = true;
            continue;
        }
        if (c == '"') {
            inString = !inString;
            continue;
        }
        if (inString) continue;

        if (c == '{') {
            if (depth == 0) objStart = i;
            depth++;
        } else if (c == '}') {
            depth--;
            if (depth == 0 && objStart != std::string::npos) {
                objects.push_back(json.substr(objStart, i - objStart + 1));
                objStart = std::string::npos;
            }
        } else if (c == ']' && depth == 0) {
            break;
        }
    }
    return objects;
}

static std::unordered_map<std::string, std::string> extractDictionary(const std::string& json, const std::string& dictName) {
    std::unordered_map<std::string, std::string> dict;
    std::string searchKey = "\"" + dictName + "\"";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return dict;

    pos = json.find('{', pos);
    if (pos == std::string::npos) return dict;

    int depth = 0;
    std::string currentKey;
    size_t valStart = std::string::npos;
    bool inString = false;
    bool escaped = false;
    bool expectingKey = true;

    for (size_t i = pos + 1; i < json.size(); ++i) {
        char c = json[i];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (c == '\\') {
            escaped = true;
            continue;
        }
        if (c == '"') {
            if (!inString && expectingKey && depth == 0) {
                size_t kStart = i + 1;
                size_t kEnd = json.find('"', kStart);
                if (kEnd != std::string::npos) {
                    currentKey = json.substr(kStart, kEnd - kStart);
                    i = kEnd;
                    expectingKey = false;
                }
            }
            inString = !inString;
            continue;
        }
        if (inString) continue;

        if (c == '{') {
            if (depth == 0) valStart = i;
            depth++;
        } else if (c == '}') {
            depth--;
            if (depth == 0 && valStart != std::string::npos && !currentKey.empty()) {
                dict[currentKey] = trim(json.substr(valStart, i - valStart + 1));
                valStart = std::string::npos;
                currentKey.clear();
                expectingKey = true;
            } else if (depth < 0) {
                break;
            }
        }
    }
    return dict;
}

/* ============================================================
   DbManagerJson Implementation
   ============================================================ */

DbManagerJson::~DbManagerJson() {
    close();
}

Result DbManagerJson::open(const std::string& path) {
    filePath_ = path;

    std::ifstream file(filePath_);
    if (!file.good()) {
        return saveToFile();
    }

    return loadFromFile();
}

void DbManagerJson::close() {
    if (!filePath_.empty()) {
        saveToFile();
        nodes_.clear();
        layers_.clear();
        edges_.clear();
        nodeLayers_.clear();
        nodeLayoutMap_.clear();
        layerLayoutMap_.clear();
        edgeLayoutMap_.clear();
        filePath_.clear();
    }
}

Result DbManagerJson::loadFromFile() {
    std::ifstream file(filePath_);
    if (!file.is_open()) {
        return Result::failure("Unable to open file for reading: " + filePath_);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    nodes_.clear();
    layers_.clear();
    edges_.clear();
    nodeLayers_.clear();
    nodeLayoutMap_.clear();
    layerLayoutMap_.clear();
    edgeLayoutMap_.clear();

    maxNodeId_ = 0;
    maxLayerId_ = 0;
    maxEdgeId_ = 0;

    // 1. Extract layout sections
    auto rawNodeLayout = extractDictionary(content, "nodes");
    for (const auto& pair : rawNodeLayout) {
        try { nodeLayoutMap_[std::stoull(pair.first)] = pair.second; } catch (...) {}
    }

    auto rawLayerLayout = extractDictionary(content, "layers");
    for (const auto& pair : rawLayerLayout) {
        try { layerLayoutMap_[std::stoull(pair.first)] = pair.second; } catch (...) {}
    }

    auto rawEdgeLayout = extractDictionary(content, "edges");
    for (const auto& pair : rawEdgeLayout) {
        try { edgeLayoutMap_[std::stoull(pair.first)] = pair.second; } catch (...) {}
    }

    // 2. Load Layers
    auto layerBlocks = extractObjectList(content, "layers");
    for (const auto& blk : layerBlocks) {
        LayerData l;
        std::string idStr = extractField(blk, "id");
        if (!idStr.empty()) l.id = static_cast<LayerId>(std::stoull(idStr));
        l.name       = extractField(blk, "name");
        l.kind       = extractField(blk, "kind");
        l.attributes = extractField(blk, "attributes");
        std::string csStr = extractField(blk, "checksum");
        if (!csStr.empty()) l.checksum = static_cast<uint32_t>(std::stoul(csStr));
        l.status     = status_from_string(extractField(blk, "status"));
        l.reviewer   = extractField(blk, "reviewer");

        // Hydrate metadata from separate layout section
        if (layerLayoutMap_.count(l.id)) {
            l.metadata = layerLayoutMap_[l.id];
        }

        layers_.push_back(l);
        maxLayerId_ = std::max(maxLayerId_, l.id);
    }

    // 3. Load Nodes
    auto nodeBlocks = extractObjectList(content, "nodes");
    for (const auto& blk : nodeBlocks) {
        NodeData n;
        std::string idStr = extractField(blk, "id");
        if (!idStr.empty()) n.id = static_cast<NodeId>(std::stoull(idStr));
        n.name       = extractField(blk, "name");
        n.type       = extractField(blk, "type");
        n.attributes = extractField(blk, "attributes");
        std::string csStr = extractField(blk, "checksum");
        if (!csStr.empty()) n.checksum = static_cast<uint32_t>(std::stoul(csStr));
        n.status     = status_from_string(extractField(blk, "status"));
        n.reviewer   = extractField(blk, "reviewer");

        // Hydrate metadata from separate layout section
        if (nodeLayoutMap_.count(n.id)) {
            n.metadata = nodeLayoutMap_[n.id];
        }

        nodes_.push_back(n);
        maxNodeId_ = std::max(maxNodeId_, n.id);
    }

    // 4. Load Node-Layer mappings
    auto nlBlocks = extractObjectList(content, "node_layers");
    for (const auto& blk : nlBlocks) {
        NodeLayer nl;
        std::string nId = extractField(blk, "node_id");
        std::string lId = extractField(blk, "layer_id");
        if (!nId.empty() && !lId.empty()) {
            nl.nodeId = static_cast<NodeId>(std::stoull(nId));
            nl.layerId = static_cast<LayerId>(std::stoull(lId));
            nodeLayers_.push_back(nl);
        }
    }

    // 5. Load Edges
    auto edgeBlocks = extractObjectList(content, "edges");
    for (const auto& blk : edgeBlocks) {
        EdgeData e;
        std::string idStr = extractField(blk, "id");
        if (!idStr.empty()) e.id = static_cast<EdgeId>(std::stoull(idStr));
        std::string sn = extractField(blk, "src_node_id");
        std::string sl = extractField(blk, "src_layer_id");
        std::string dn = extractField(blk, "dst_node_id");
        std::string dl = extractField(blk, "dst_layer_id");
        if (!sn.empty()) e.srcNode = static_cast<NodeId>(std::stoull(sn));
        if (!sl.empty()) e.srcLayer = static_cast<LayerId>(std::stoull(sl));
        if (!dn.empty()) e.dstNode = static_cast<NodeId>(std::stoull(dn));
        if (!dl.empty()) e.dstLayer = static_cast<LayerId>(std::stoull(dl));
        e.edgeType   = extractField(blk, "edge_type");
        e.attributes = extractField(blk, "attributes");
        std::string csStr = extractField(blk, "checksum");
        if (!csStr.empty()) e.checksum = static_cast<uint32_t>(std::stoul(csStr));
        e.status     = status_from_string(extractField(blk, "status"));
        e.reviewer   = extractField(blk, "reviewer");

        // Hydrate metadata from separate layout section
        if (edgeLayoutMap_.count(e.id)) {
            e.metadata = edgeLayoutMap_[e.id];
        }

        edges_.push_back(e);
        maxEdgeId_ = std::max(maxEdgeId_, e.id);
    }

    return Result::success();
}

Result DbManagerJson::saveToFile() {
    if (filePath_.empty())
        return Result::failure("No output file specified.");

    std::ostringstream os;
    os << "{\n";

    // 1. Layers (Clean domain fields only)
    os << "  \"layers\": [\n";
    for (size_t i = 0; i < layers_.size(); ++i) {
        const auto& l = layers_[i];
        os << "    {\n"
           << "      \"id\": " << l.id << ",\n"
           << "      \"name\": \"" << escapeJson(l.name) << "\",\n"
           << "      \"kind\": \"" << escapeJson(l.kind) << "\",\n"
           << "      \"attributes\": \"" << escapeJson(l.attributes) << "\",\n"
           << "      \"checksum\": " << l.checksum << ",\n"
           << "      \"status\": \"" << escapeJson(to_string(l.status)) << "\",\n"
           << "      \"reviewer\": \"" << escapeJson(l.reviewer) << "\"\n"
           << "    }" << (i + 1 < layers_.size() ? "," : "") << "\n";
    }
    os << "  ],\n";

    // 2. Nodes (Clean domain fields only)
    os << "  \"nodes\": [\n";
    for (size_t i = 0; i < nodes_.size(); ++i) {
        const auto& n = nodes_[i];
        os << "    {\n"
           << "      \"id\": " << n.id << ",\n"
           << "      \"name\": \"" << escapeJson(n.name) << "\",\n"
           << "      \"type\": \"" << escapeJson(n.type) << "\",\n"
           << "      \"attributes\": \"" << escapeJson(n.attributes) << "\",\n"
           << "      \"checksum\": " << n.checksum << ",\n"
           << "      \"status\": \"" << escapeJson(to_string(n.status)) << "\",\n"
           << "      \"reviewer\": \"" << escapeJson(n.reviewer) << "\"\n"
           << "    }" << (i + 1 < nodes_.size() ? "," : "") << "\n";
    }
    os << "  ],\n";

    // 3. Node-Layers
    os << "  \"node_layers\": [\n";
    for (size_t i = 0; i < nodeLayers_.size(); ++i) {
        const auto& nl = nodeLayers_[i];
        os << "    {\n"
           << "      \"node_id\": " << nl.nodeId << ",\n"
           << "      \"layer_id\": " << nl.layerId << "\n"
           << "    }" << (i + 1 < nodeLayers_.size() ? "," : "") << "\n";
    }
    os << "  ],\n";

    // 4. Edges (Clean domain fields only)
    os << "  \"edges\": [\n";
    for (size_t i = 0; i < edges_.size(); ++i) {
        const auto& e = edges_[i];
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
           << "    }" << (i + 1 < edges_.size() ? "," : "") << "\n";
    }
    os << "  ],\n";

    // 5. Completely Separated Layout Block
    os << "  \"layout\": {\n";

    // Layout -> Nodes
    os << "    \"nodes\": {\n";
    size_t count = 0;
    for (const auto& n : nodes_) {
        std::string meta = n.metadata.empty() ? "{}" : n.metadata;
        os << "      \"" << n.id << "\": " << meta << (++count < nodes_.size() ? ",\n" : "\n");
    }
    os << "    },\n";

    // Layout -> Layers
    os << "    \"layers\": {\n";
    count = 0;
    for (const auto& l : layers_) {
        std::string meta = l.metadata.empty() ? "{}" : l.metadata;
        os << "      \"" << l.id << "\": " << meta << (++count < layers_.size() ? ",\n" : "\n");
    }
    os << "    },\n";

    // Layout -> Edges
    os << "    \"edges\": {\n";
    count = 0;
    for (const auto& e : edges_) {
        std::string meta = e.metadata.empty() ? "{}" : e.metadata;
        os << "      \"" << e.id << "\": " << meta << (++count < edges_.size() ? ",\n" : "\n");
    }
    os << "    }\n";

    os << "  }\n";
    os << "}\n";

    std::ofstream file(filePath_, std::ios::trunc);
    if (!file.is_open()) {
        return Result::failure("Unable to open file for writing: " + filePath_);
    }

    file << os.str();
    file.close();

    return Result::success();
}

NodeId DbManagerJson::getNextNodeId() { return ++maxNodeId_; }
LayerId DbManagerJson::getNextLayerId() { return ++maxLayerId_; }
EdgeId DbManagerJson::getNextEdgeId() { return ++maxEdgeId_; }

/* ================= Nodes ================= */

Result DbManagerJson::createNode(const NodeData& node, NodeId& outId) {
    NodeData copy = node;
    copy.id = getNextNodeId();
    outId = copy.id;
    nodes_.push_back(copy);
    return saveToFile();
}

Result DbManagerJson::updateNode(const NodeData& node) {
    for (auto& n : nodes_) {
        if (n.id == node.id) {
            n = node;
            return saveToFile();
        }
    }
    return Result::failure("Node not found");
}

Result DbManagerJson::deleteNode(NodeId id) {
    nodes_.erase(
        std::remove_if(nodes_.begin(), nodes_.end(), [id](const NodeData& n) { return n.id == id; }),
        nodes_.end());

    nodeLayers_.erase(
        std::remove_if(nodeLayers_.begin(), nodeLayers_.end(), [id](const NodeLayer& nl) { return nl.nodeId == id; }),
        nodeLayers_.end());

    edges_.erase(
        std::remove_if(edges_.begin(), edges_.end(), [id](const EdgeData& e) { return e.srcNode == id || e.dstNode == id; }),
        edges_.end());

    return saveToFile();
}

std::vector<NodeData> DbManagerJson::getAllNodes() {
    return nodes_;
}

/* ================= Layers ================= */

Result DbManagerJson::createLayer(const LayerData& layer, LayerId& outId) {
    LayerData copy = layer;
    copy.id = getNextLayerId();
    outId = copy.id;
    layers_.push_back(copy);
    return saveToFile();
}

Result DbManagerJson::updateLayer(const LayerData& layer) {
    for (auto& l : layers_) {
        if (l.id == layer.id) {
            l = layer;
            return saveToFile();
        }
    }
    return Result::failure("Layer not found");
}

Result DbManagerJson::deleteLayer(LayerId id) {
    layers_.erase(
        std::remove_if(layers_.begin(), layers_.end(), [id](const LayerData& l) { return l.id == id; }),
        layers_.end());

    nodeLayers_.erase(
        std::remove_if(nodeLayers_.begin(), nodeLayers_.end(), [id](const NodeLayer& nl) { return nl.layerId == id; }),
        nodeLayers_.end());

    edges_.erase(
        std::remove_if(edges_.begin(), edges_.end(), [id](const EdgeData& e) { return e.srcLayer == id || e.dstLayer == id; }),
        edges_.end());

    return saveToFile();
}

std::vector<LayerData> DbManagerJson::getAllLayers() {
    return layers_;
}

/* ================= Node-Layers ================= */

Result DbManagerJson::addNodeToLayer(NodeId nodeId, LayerId layerId) {
    for (const auto& nl : nodeLayers_) {
        if (nl.nodeId == nodeId && nl.layerId == layerId)
            return Result::success();
    }
    nodeLayers_.push_back({nodeId, layerId});
    return saveToFile();
}

Result DbManagerJson::removeNodeFromLayer(NodeId nodeId, LayerId layerId) {
    nodeLayers_.erase(
        std::remove_if(nodeLayers_.begin(), nodeLayers_.end(),
                       [nodeId, layerId](const NodeLayer& nl) {
                           return nl.nodeId == nodeId && nl.layerId == layerId;
                       }),
        nodeLayers_.end());
    return saveToFile();
}

std::vector<NodeLayer> DbManagerJson::getNodesInLayer(LayerId layerId) {
    std::vector<NodeLayer> out;
    for (const auto& nl : nodeLayers_) {
        if (nl.layerId == layerId)
            out.push_back(nl);
    }
    return out;
}

/* ================= Edges ================= */

Result DbManagerJson::createEdge(const EdgeData& edge, EdgeId& outId) {
    EdgeData copy = edge;
    copy.id = getNextEdgeId();
    outId = copy.id;
    edges_.push_back(copy);
    return saveToFile();
}

Result DbManagerJson::updateEdge(const EdgeData& edge) {
    for (auto& e : edges_) {
        if (e.id == edge.id) {
            e = edge;
            return saveToFile();
        }
    }
    return Result::failure("Edge not found");
}

Result DbManagerJson::deleteEdge(EdgeId id) {
    edges_.erase(
        std::remove_if(edges_.begin(), edges_.end(), [id](const EdgeData& e) { return e.id == id; }),
        edges_.end());
    return saveToFile();
}

std::vector<EdgeData> DbManagerJson::getAllEdges() {
    return edges_;
}