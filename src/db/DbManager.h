#pragma once

#include <string>
#include <vector>

#include "core/Types.h"

/* ============================================================
   Generic result type
   ============================================================ */
struct Result {
    bool ok{false};
    std::string message;

    static Result success(const std::string& msg = {}) {
        return {true, msg};
    }

    static Result failure(const std::string& msg) {
        return {false, msg};
    }
};

/* ============================================================
   DbManager – storage interface
   ============================================================ */
class DbManager {
public:
    virtual ~DbManager() = default;

    /* ========================================================
       Lifecycle
       ======================================================== */
    virtual Result open(const std::string& path) = 0;
    virtual void   close() = 0;

    /* ========================================================
       Nodes
       ======================================================== */
    virtual Result createNode(const NodeData& node, NodeId& outId) = 0;
    virtual Result updateNode(const NodeData& node) = 0;
    virtual Result deleteNode(NodeId id) = 0;
    virtual std::vector<NodeData> getAllNodes() = 0;

    /* ========================================================
       Layers
       ======================================================== */
    virtual Result createLayer(const LayerData& layer, LayerId& outId) = 0;
    virtual Result updateLayer(const LayerData& layer) = 0;
    virtual Result deleteLayer(LayerId id) = 0;
    virtual std::vector<LayerData> getAllLayers() = 0;

    /* ========================================================
       Node–Layer relationship
       ======================================================== */
    virtual Result addNodeToLayer(NodeId nodeId, LayerId layerId) = 0;
    virtual Result removeNodeFromLayer(NodeId nodeId, LayerId layerId) = 0;
    virtual std::vector<NodeLayer> getNodesInLayer(LayerId layerId) = 0;

    /* ========================================================
       Edges (layer-aware)
       ======================================================== */
    virtual Result createEdge(const EdgeData& edge, EdgeId& outId) = 0;
    virtual Result updateEdge(const EdgeData& edge) = 0;
    virtual Result deleteEdge(EdgeId id) = 0;
    virtual std::vector<EdgeData> getAllEdges() = 0;
};
