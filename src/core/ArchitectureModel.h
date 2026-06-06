#pragma once

#include "DbManager.h"
#include "db/DbManager.h"
#include "core/DomainObjects.h"
#include "core/GraphSnapshot.h"
#include <optional>
#include <vector>

class ArchitectureModel
{
public:
    explicit ArchitectureModel(DbManager& db);

    void reloadCache();

    /* ============================================================
       Nodes
       ============================================================ */

    Result addNode(
        NodeData& n,
        NodeId& outId);

    Result updateNode(NodeData& n);

    Result deleteNode(NodeId id);

    const std::vector<NodeData>&
    nodes() const;

    Result setNodeMetadata(
        NodeId id,
        const std::string& metadata);

    Result setNodeAttributes(
        NodeId id,
        const std::string& attributes);

    Result reviewNode(
        NodeId id,
        const std::string& reviewer);

    /* ============================================================
       Layers
       ============================================================ */

    Result addLayer(
        LayerData& l,
        LayerId& outId);

    Result updateLayer(LayerData& l);

    Result deleteLayer(LayerId id);

    const std::vector<LayerData>&
    layers() const;

    Result setLayerMetadata(
        LayerId id,
        const std::string& metadata);

    Result setLayerAttributes(
        LayerId id,
        const std::string& attributes);

    Result reviewLayer(
        LayerId id,
        const std::string& reviewer);

    /* ============================================================
       Node-Layer relationship
       ============================================================ */

    Result addNodeToLayer(
        NodeId nodeId,
        LayerId layerId);

    Result removeNodeFromLayer(
        NodeId nodeId,
        LayerId layerId);

    std::vector<NodeLayer>
    nodesInLayer(LayerId layerId) const;

    std::vector<NodeLayer>
    layersForNode(NodeId nodeId) const;

    /* ============================================================
       Edges
       ============================================================ */

    Result addEdge(
        EdgeData& e,
        EdgeId& outId);

    Result updateEdge(EdgeData& e);

    Result deleteEdge(EdgeId id);

    const std::vector<EdgeData>&
    edges() const;

    Result setEdgeMetadata(
        EdgeId id,
        const std::string& metadata);

    Result setEdgeAttributes(
        EdgeId id,
        const std::string& attributes);

    Result reviewEdge(
        EdgeId id,
        const std::string& reviewer);

    /* ============================================================
       Graph extraction
       ============================================================ */

    GraphSnapshot extractGraph(
        std::optional<LayerId> layerId) const;

    /* ============================================================
       Lookup helpers
       ============================================================ */

    std::optional<NodeData>
    getNodeById(NodeId id) const;

    std::optional<LayerData>
    getLayerById(LayerId id) const;

    std::optional<EdgeData>
    getEdgeById(EdgeId id) const;

private:
    uint32_t computeNodeChecksum(
        const NodeData& n) const;

    uint32_t computeLayerChecksum(
        const LayerData& l) const;

    uint32_t computeEdgeChecksum(
        const EdgeData& e) const;

private:
    DbManager& db_;

    std::vector<NodeData>  m_nodes;
    std::vector<LayerData> m_layers;
    std::vector<EdgeData>  m_edges;
};
