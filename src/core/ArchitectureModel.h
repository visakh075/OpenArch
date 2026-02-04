#pragma once

#include <vector>
#include <string>

#include "Types.h"


#include <optional>
#include "db/DbManager.h"
#include "core/DomainObjects.h"
#include "core/GraphSnapshot.h"
/*
 * ArchitectureModel
 *
 * This is the SINGLE place where:
 *  - checksums are computed
 *  - status transitions are enforced
 *  - reviewers are invalidated or applied
 *
 * CLI must talk ONLY to this layer.
 * DB implementations must remain dumb storage.
 */
class ArchitectureModel {
public:
    explicit ArchitectureModel(DbManager& db);

    /* ========================================================
       Nodes
       ======================================================== */
    Result addNode(NodeData& node, NodeId& outId);
    Result updateNode(NodeData& node);
    Result deleteNode(NodeId id);
    std::vector<NodeData> nodes() const;

    Result setNodeMetadata(NodeId id, const std::string& metadata);
    Result setNodeAttributes(NodeId id, const std::string& attributes);
    Result reviewNode(NodeId id, const std::string& reviewer);

    /* ========================================================
       Layers
       ======================================================== */
    Result addLayer(LayerData& layer, LayerId& outId);
    Result updateLayer(LayerData& layer);
    Result deleteLayer(LayerId id);
    std::vector<LayerData> layers() const;

    Result setLayerMetadata(LayerId id, const std::string& metadata);
    Result setLayerAttributes(LayerId id, const std::string& attributes);
    Result reviewLayer(LayerId id, const std::string& reviewer);

    /* ========================================================
       Node–Layer relationship
       ======================================================== */
    Result addNodeToLayer(NodeId nodeId, LayerId layerId);
    Result removeNodeFromLayer(NodeId nodeId, LayerId layerId);
    std::vector<NodeLayer> nodesInLayer(LayerId layerId) const;

    /* ========================================================
       Edges (layer-aware)
       ======================================================== */
    Result addEdge(EdgeData& edge, EdgeId& outId);
    Result updateEdge(EdgeData& edge);
    Result deleteEdge(EdgeId id);
    std::vector<EdgeData> edges() const;

    Result setEdgeMetadata(EdgeId id, const std::string& metadata);
    Result setEdgeAttributes(EdgeId id, const std::string& attributes);
    Result reviewEdge(EdgeId id, const std::string& reviewer);

    GraphSnapshot extractGraph(
        std::optional<LayerId> layerId = std::nullopt
    ) const;
    
    std::optional<NodeData>  getNodeById(NodeId id) const;
    std::optional<LayerData> getLayerById(LayerId id) const;

    std::vector<NodeLayer> layersForNode(NodeId nodeId) const;

private:
    DbManager& db_;

    /* ========================================================
       Internal helpers (defined in .cpp)
       ======================================================== */
    uint32_t computeNodeChecksum(const NodeData& node) const;
    uint32_t computeLayerChecksum(const LayerData& layer) const;
    uint32_t computeEdgeChecksum(const EdgeData& edge) const;
};
