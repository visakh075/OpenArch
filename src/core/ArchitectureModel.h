#pragma once
#include <vector>
#include "DomainObjects.h"
#include "core/Result.h"
#include "db/DbManager.h"
class ArchitectureModel{
 DbManager& db_;
public:
 explicit ArchitectureModel(DbManager&);
 Result addNode(const NodeData&,NodeId&);
 Result updateNode(const NodeData&);
 Result deleteNode(NodeId);
 std::vector<NodeData> nodes()const;
 Result addLayer(const LayerData&,LayerId&);
 Result updateLayer(const LayerData&);
 Result deleteLayer(LayerId);
 std::vector<LayerData> layers()const;
 Result addNodeToLayer(NodeId,LayerId);
 Result removeNodeFromLayer(NodeId,LayerId);
 std::vector<NodeLayer> nodesInLayer(LayerId)const;
 Result addEdge(const EdgeData&,EdgeId&);
 Result updateEdge(const EdgeData&);
 Result deleteEdge(EdgeId);
 std::vector<EdgeData> edges()const;
};
