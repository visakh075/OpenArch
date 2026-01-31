#pragma once
#include "DomainObjects.h"
#include "db/DbManager.h"
#include "core/Result.h"

class ArchitectureModel {
public:
    explicit ArchitectureModel(DbManager&);

    Result addNode(const NodeData&, NodeId&);
    Result updateNode(const NodeData&);
    Result deleteNode(NodeId);
    std::vector<NodeData> nodes() const;

    Result addLayer(const LayerData&, LayerId&);
    Result updateLayer(const LayerData&);
    Result deleteLayer(LayerId);
    std::vector<LayerData> layers() const;

    Result addEdge(const EdgeData&, EdgeId&);
    Result updateEdge(const EdgeData&);
    Result deleteEdge(EdgeId);
    std::vector<EdgeData> edges() const;

private:
    DbManager& db_;
};
