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
        const std::string& reviewer,
        Status status = Status::Reviewed);

    Result setNodeGovernance(
        NodeId id,
        Status status,
        const std::string& reviewer);

    Result cascadeNodeGovernance(
        NodeId parentId,
        Status status,
        const std::string& reviewer,
        bool includeInternalEdges = true);

    std::vector<NodeId> getChildNodeIds(NodeId parentId, bool recursive = true) const;
    std::vector<EdgeId> getInternalEdgeIds(const std::vector<NodeId>& nodeIds) const;

    void markAncestorsChanged(std::optional<NodeId> parentId);
    void markEdgeEndpointsContainersChanged(NodeId srcNodeId, NodeId dstNodeId);

    struct ContainerGovernanceSummary
    {
        std::vector<NodeId> childNodeIds;
        std::vector<EdgeId> internalEdgeIds;

        size_t totalChildren{0};
        size_t approvedChildren{0};
        size_t reviewedChildren{0};
        size_t changedChildren{0};
        size_t newChildren{0};
        size_t invalidChildren{0};

        size_t totalInternalEdges{0};
        size_t approvedEdges{0};
        size_t reviewedEdges{0};
        size_t changedEdges{0};
        size_t newEdges{0};
        size_t invalidEdges{0};

        bool hasIssues{false};
        bool allApproved{false};
        Status rollupStatus{Status::New};
    };

    ContainerGovernanceSummary getContainerGovernanceSummary(NodeId containerId) const;

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
        const std::string& reviewer,
        Status status = Status::Reviewed);

    Result setLayerGovernance(
        LayerId id,
        Status status,
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
        const std::string& reviewer,
        Status status = Status::Reviewed);

    Result setEdgeGovernance(
        EdgeId id,
        Status status,
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

    /* ============================================================
       Checksum & Integrity Verification
       ============================================================ */

    uint32_t computeNodeChecksum(const NodeData& n) const;
    uint32_t computeLayerChecksum(const LayerData& l) const;
    uint32_t computeEdgeChecksum(const EdgeData& e) const;

    bool verifyNodeIntegrity(NodeId id) const;
    bool verifyLayerIntegrity(LayerId id) const;
    bool verifyEdgeIntegrity(EdgeId id) const;

    struct GovernanceAuditReport
    {
        size_t totalNodes{0};
        size_t totalLayers{0};
        size_t totalEdges{0};

        size_t newCount{0};
        size_t changedCount{0};
        size_t reviewedCount{0};
        size_t approvedCount{0};
        size_t invalidCount{0};
        size_t deletedCount{0};

        size_t tamperedNodes{0};
        size_t tamperedLayers{0};
        size_t tamperedEdges{0};

        std::vector<std::string> issues;
    };

    GovernanceAuditReport auditGovernance() const;

private:
    DbManager& db_;

    std::vector<NodeData>  m_nodes;
    std::vector<LayerData> m_layers;
    std::vector<EdgeData>  m_edges;
};
