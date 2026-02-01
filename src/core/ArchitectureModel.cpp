#include "ArchitectureModel.h"

#include "Checksum.h"

#include <sstream>

#include <unordered_map>
#include <algorithm>
/* ============================================================
   Construction
   ============================================================ */

ArchitectureModel::ArchitectureModel(DbManager& db)
    : db_(db) {}

/* ============================================================
   Checksum helpers (core fields only)
   ============================================================ */

uint32_t ArchitectureModel::computeNodeChecksum(const NodeData& n) const {
    std::ostringstream os;
    os << n.name << "|" << n.type;
    return crc32(os.str());
}

uint32_t ArchitectureModel::computeLayerChecksum(const LayerData& l) const {
    std::ostringstream os;
    os << l.name << "|" << l.kind;
    return crc32(os.str());
}

uint32_t ArchitectureModel::computeEdgeChecksum(const EdgeData& e) const {
    std::ostringstream os;
    os << e.srcNode << "|"
       << e.srcLayer << "|"
       << e.dstNode << "|"
       << e.dstLayer << "|"
       << e.edgeType;
    return crc32(os.str());
}

/* ============================================================
   Nodes
   ============================================================ */

Result ArchitectureModel::addNode(NodeData& n, NodeId& outId) {
    n.checksum = computeNodeChecksum(n);
    n.status   = Status::New;
    n.reviewer.clear();
    return db_.createNode(n, outId);
}

Result ArchitectureModel::updateNode(NodeData& n) {
    auto all = db_.getAllNodes();
    for (auto& old : all) {
        if (old.id == n.id) {
            uint32_t newSum = computeNodeChecksum(n);

            if (newSum != old.checksum) {
                n.checksum = newSum;
                n.status   = Status::Changed;
                n.reviewer.clear();
            } else {
                n.checksum = old.checksum;
                n.status   = old.status;
                n.reviewer = old.reviewer;
            }
            return db_.updateNode(n);
        }
    }
    return Result::failure("Node not found");
}

Result ArchitectureModel::deleteNode(NodeId id) {
    return db_.deleteNode(id);
}

std::vector<NodeData> ArchitectureModel::nodes() const {
    return db_.getAllNodes();
}

Result ArchitectureModel::setNodeMetadata(NodeId id, const std::string& metadata) {
    auto all = db_.getAllNodes();
    for (auto& n : all) {
        if (n.id == id) {
            n.metadata = metadata;
            return db_.updateNode(n);
        }
    }
    return Result::failure("Node not found");
}

Result ArchitectureModel::setNodeAttributes(NodeId id, const std::string& attributes) {
    auto all = db_.getAllNodes();
    for (auto& n : all) {
        if (n.id == id) {
            n.attributes = attributes;
            return db_.updateNode(n);
        }
    }
    return Result::failure("Node not found");
}

Result ArchitectureModel::reviewNode(NodeId id, const std::string& reviewer) {
    auto all = db_.getAllNodes();
    for (auto& n : all) {
        if (n.id == id) {
            n.status   = Status::Reviewed;
            n.reviewer = reviewer;
            return db_.updateNode(n);
        }
    }
    return Result::failure("Node not found");
}

/* ============================================================
   Layers
   ============================================================ */

Result ArchitectureModel::addLayer(LayerData& l, LayerId& outId) {
    l.checksum = computeLayerChecksum(l);
    l.status   = Status::New;
    l.reviewer.clear();
    return db_.createLayer(l, outId);
}

Result ArchitectureModel::updateLayer(LayerData& l) {
    auto all = db_.getAllLayers();
    for (auto& old : all) {
        if (old.id == l.id) {
            uint32_t newSum = computeLayerChecksum(l);

            if (newSum != old.checksum) {
                l.checksum = newSum;
                l.status   = Status::Changed;
                l.reviewer.clear();
            } else {
                l.checksum = old.checksum;
                l.status   = old.status;
                l.reviewer = old.reviewer;
            }
            return db_.updateLayer(l);
        }
    }
    return Result::failure("Layer not found");
}

Result ArchitectureModel::deleteLayer(LayerId id) {
    return db_.deleteLayer(id);
}

std::vector<LayerData> ArchitectureModel::layers() const {
    return db_.getAllLayers();
}

Result ArchitectureModel::setLayerMetadata(LayerId id, const std::string& metadata) {
    auto all = db_.getAllLayers();
    for (auto& l : all) {
        if (l.id == id) {
            l.metadata = metadata;
            return db_.updateLayer(l);
        }
    }
    return Result::failure("Layer not found");
}

Result ArchitectureModel::setLayerAttributes(LayerId id, const std::string& attributes) {
    auto all = db_.getAllLayers();
    for (auto& l : all) {
        if (l.id == id) {
            l.attributes = attributes;
            return db_.updateLayer(l);
        }
    }
    return Result::failure("Layer not found");
}

Result ArchitectureModel::reviewLayer(LayerId id, const std::string& reviewer) {
    auto all = db_.getAllLayers();
    for (auto& l : all) {
        if (l.id == id) {
            l.status   = Status::Reviewed;
            l.reviewer = reviewer;
            return db_.updateLayer(l);
        }
    }
    return Result::failure("Layer not found");
}

/* ============================================================
   Node–Layer relationship
   ============================================================ */

Result ArchitectureModel::addNodeToLayer(NodeId nodeId, LayerId layerId) {
    return db_.addNodeToLayer(nodeId, layerId);
}

Result ArchitectureModel::removeNodeFromLayer(NodeId nodeId, LayerId layerId) {
    return db_.removeNodeFromLayer(nodeId, layerId);
}

std::vector<NodeLayer> ArchitectureModel::nodesInLayer(LayerId layerId) const {
    return db_.getNodesInLayer(layerId);
}

/* ============================================================
   Edges
   ============================================================ */

Result ArchitectureModel::addEdge(EdgeData& e, EdgeId& outId) {
    e.checksum = computeEdgeChecksum(e);
    e.status   = Status::New;
    e.reviewer.clear();
    return db_.createEdge(e, outId);
}

Result ArchitectureModel::updateEdge(EdgeData& e) {
    auto all = db_.getAllEdges();
    for (auto& old : all) {
        if (old.id == e.id) {
            uint32_t newSum = computeEdgeChecksum(e);

            if (newSum != old.checksum) {
                e.checksum = newSum;
                e.status   = Status::Changed;
                e.reviewer.clear();
            } else {
                e.checksum = old.checksum;
                e.status   = old.status;
                e.reviewer = old.reviewer;
            }
            return db_.updateEdge(e);
        }
    }
    return Result::failure("Edge not found");
}

Result ArchitectureModel::deleteEdge(EdgeId id) {
    return db_.deleteEdge(id);
}

std::vector<EdgeData> ArchitectureModel::edges() const {
    return db_.getAllEdges();
}

Result ArchitectureModel::setEdgeMetadata(EdgeId id, const std::string& metadata) {
    auto all = db_.getAllEdges();
    for (auto& e : all) {
        if (e.id == id) {
            e.metadata = metadata;
            return db_.updateEdge(e);
        }
    }
    return Result::failure("Edge not found");
}

Result ArchitectureModel::setEdgeAttributes(EdgeId id, const std::string& attributes) {
    auto all = db_.getAllEdges();
    for (auto& e : all) {
        if (e.id == id) {
            e.attributes = attributes;
            return db_.updateEdge(e);
        }
    }
    return Result::failure("Edge not found");
}

Result ArchitectureModel::reviewEdge(EdgeId id, const std::string& reviewer) {
    auto all = db_.getAllEdges();
    for (auto& e : all) {
        if (e.id == id) {
            e.status   = Status::Reviewed;
            e.reviewer = reviewer;
            return db_.updateEdge(e);
        }
    }
    return Result::failure("Edge not found");
}

GraphSnapshot ArchitectureModel::extractGraph(
    std::optional<LayerId> layerFilter
) const {
    GraphSnapshot snap;

    // --------------------------------------------------
    // Fetch everything ONCE
    // --------------------------------------------------
    auto allNodes  = db_.getAllNodes();
    auto allLayers = db_.getAllLayers();
    auto allEdges  = db_.getAllEdges();

    // Node → layers map
    std::unordered_map<NodeId, std::vector<LayerId>> nodeLayers;
    for (const auto& layer : allLayers) {
        auto members = db_.getNodesInLayer(layer.id);
        for (const auto& nl : members)
            nodeLayers[nl.nodeId].push_back(layer.id);
    }

    // --------------------------------------------------
    // Layer visibility
    // --------------------------------------------------
    if (layerFilter) {
        for (const auto& l : allLayers)
            if (l.id == *layerFilter)
                snap.visibleLayers.push_back(l);
    } else {
        snap.visibleLayers = allLayers;
    }

    // --------------------------------------------------
    // Node selection
    // --------------------------------------------------
    std::unordered_map<NodeId, NodeData> selectedNodes;

    for (const auto& n : allNodes) {
        if (!layerFilter) {
            selectedNodes[n.id] = n;
        } else {
            auto it = nodeLayers.find(n.id);
            if (it != nodeLayers.end()) {
                for (auto lid : it->second) {
                    if (lid == *layerFilter) {
                        selectedNodes[n.id] = n;
                        break;
                    }
                }
            }
        }
    }

    // --------------------------------------------------
    // Edge selection
    // --------------------------------------------------
    for (const auto& e : allEdges) {
        bool include = false;

        if (!layerFilter)
            include = true;
        else if (e.srcLayer == *layerFilter || e.dstLayer == *layerFilter)
            include = true;

        if (!include)
            continue;

        // Ensure endpoints exist
        for (auto nid : {e.srcNode, e.dstNode}) {
            if (!selectedNodes.count(nid)) {
                for (const auto& n : allNodes)
                    if (n.id == nid)
                        selectedNodes[n.id] = n;
            }
        }

        GraphEdgeView ev;
        ev.id       = e.id;
        ev.srcNode  = e.srcNode;
        ev.dstNode  = e.dstNode;
        ev.srcLayer = e.srcLayer;
        ev.dstLayer = e.dstLayer;
        ev.type     = e.edgeType;
        ev.status   = e.status;
        ev.metadata = e.metadata;

        snap.edges.push_back(std::move(ev));
    }

    // --------------------------------------------------
    // Build node views
    // --------------------------------------------------
    for (const auto& [id, n] : selectedNodes) {
        GraphNodeView v;
        v.id         = n.id;
        v.label      = n.name;
        v.type       = n.type;
        v.status     = n.status;
        v.reviewer   = n.reviewer;
        v.metadata   = n.metadata;
        v.attributes = n.attributes;
        v.layers     = nodeLayers[n.id];

        snap.nodes.push_back(std::move(v));
    }

    // --------------------------------------------------
    // Stable ordering
    // --------------------------------------------------
    std::sort(snap.nodes.begin(), snap.nodes.end(),
              [](auto& a, auto& b) { return a.id < b.id; });

    std::sort(snap.edges.begin(), snap.edges.end(),
              [](auto& a, auto& b) { return a.id < b.id; });

    return snap;
}
