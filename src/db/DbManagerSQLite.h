#pragma once
#include "DbManager.h"
#include <sqlite3.h>
class DbManagerSQLite:public DbManager{
 sqlite3*db_{nullptr};
 Result exec(const char*);
public:
 Result open(const std::string&) override;
 void close() override;
 Result createNode(const NodeData&,NodeId&) override;
 Result updateNode(const NodeData&) override;
 Result deleteNode(NodeId) override;
 std::vector<NodeData> getAllNodes() override;
 Result createLayer(const LayerData&,LayerId&) override;
 Result updateLayer(const LayerData&) override;
 Result deleteLayer(LayerId) override;
 std::vector<LayerData> getAllLayers() override;
 Result addNodeToLayer(NodeId,LayerId) override;
 Result removeNodeFromLayer(NodeId,LayerId) override;
 std::vector<NodeLayer> getNodesInLayer(LayerId) override;
 Result createEdge(const EdgeData&,EdgeId&) override;
 Result updateEdge(const EdgeData&) override;
 Result deleteEdge(EdgeId) override;
 std::vector<EdgeData> getAllEdges() override;
};
