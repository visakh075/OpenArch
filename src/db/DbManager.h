#pragma once
#include <vector>
#include <string>
#include "core/DomainObjects.h"
#include "core/Result.h"
class DbManager{
public:
 virtual ~DbManager()=default;
 virtual Result open(const std::string&)=0;
 virtual void close()=0;
 virtual Result createNode(const NodeData&,NodeId&)=0;
 virtual Result updateNode(const NodeData&)=0;
 virtual Result deleteNode(NodeId)=0;
 virtual std::vector<NodeData> getAllNodes()=0;
 virtual Result createLayer(const LayerData&,LayerId&)=0;
 virtual Result updateLayer(const LayerData&)=0;
 virtual Result deleteLayer(LayerId)=0;
 virtual std::vector<LayerData> getAllLayers()=0;
 virtual Result addNodeToLayer(NodeId,LayerId)=0;
 virtual Result removeNodeFromLayer(NodeId,LayerId)=0;
 virtual std::vector<NodeLayer> getNodesInLayer(LayerId)=0;
 virtual Result createEdge(const EdgeData&,EdgeId&)=0;
 virtual Result updateEdge(const EdgeData&)=0;
 virtual Result deleteEdge(EdgeId)=0;
 virtual std::vector<EdgeData> getAllEdges()=0;
};
