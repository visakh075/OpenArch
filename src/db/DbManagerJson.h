#pragma once

#include "DbManager.h"
#include <string>
#include <vector>
#include <unordered_map>

class DbManagerJson : public DbManager {
public:
    DbManagerJson() = default;
    ~DbManagerJson() override;

    Result open(const std::string& path) override;
    void close() override;

    Result createNode(const NodeData& node, NodeId& outId) override;
    Result updateNode(const NodeData& node) override;
    Result deleteNode(NodeId id) override;
    std::vector<NodeData> getAllNodes() override;

    Result createLayer(const LayerData& layer, LayerId& outId) override;
    Result updateLayer(const LayerData& layer) override;
    Result deleteLayer(LayerId id) override;
    std::vector<LayerData> getAllLayers() override;

    Result addNodeToLayer(NodeId nodeId, LayerId layerId) override;
    Result removeNodeFromLayer(NodeId nodeId, LayerId layerId) override;
    std::vector<NodeLayer> getNodesInLayer(LayerId layerId) override;

    Result createEdge(const EdgeData& edge, EdgeId& outId) override;
    Result updateEdge(const EdgeData& edge) override;
    Result deleteEdge(EdgeId id) override;
    std::vector<EdgeData> getAllEdges() override;

private:
    Result loadFromFile();
    Result saveToFile();
    NodeId getNextNodeId();
    LayerId getNextLayerId();
    EdgeId getNextEdgeId();

    std::string filePath_;

    std::vector<NodeData> nodes_;
    std::vector<LayerData> layers_;
    std::vector<EdgeData> edges_;
    std::vector<NodeLayer> nodeLayers_;

    // Separated in-memory layout maps (keyed by ID)
    std::unordered_map<NodeId, std::string> nodeLayoutMap_;
    std::unordered_map<LayerId, std::string> layerLayoutMap_;
    std::unordered_map<EdgeId, std::string> edgeLayoutMap_;

    NodeId maxNodeId_ = 0;
    LayerId maxLayerId_ = 0;
    EdgeId maxEdgeId_ = 0;
};