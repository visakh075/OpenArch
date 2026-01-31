#include "ArchitectureModel.h"

ArchitectureModel::ArchitectureModel(DbManager& d):db_(d){}

Result ArchitectureModel::addNode(const NodeData& n, NodeId& id){ return db_.createNode(n,id); }
Result ArchitectureModel::updateNode(const NodeData& n){ return db_.updateNode(n); }
Result ArchitectureModel::deleteNode(NodeId i){ return db_.deleteNode(i); }
std::vector<NodeData> ArchitectureModel::nodes() const{ return db_.getAllNodes(); }

Result ArchitectureModel::addLayer(const LayerData& l, LayerId& id){ return db_.createLayer(l,id); }
Result ArchitectureModel::updateLayer(const LayerData& l){ return db_.updateLayer(l); }
Result ArchitectureModel::deleteLayer(LayerId i){ return db_.deleteLayer(i); }
std::vector<LayerData> ArchitectureModel::layers() const{ return db_.getAllLayers(); }

Result ArchitectureModel::addEdge(const EdgeData& e, EdgeId& id){ return db_.createEdge(e,id); }
Result ArchitectureModel::updateEdge(const EdgeData& e){ return db_.updateEdge(e); }
Result ArchitectureModel::deleteEdge(EdgeId i){ return db_.deleteEdge(i); }
std::vector<EdgeData> ArchitectureModel::edges() const{ return db_.getAllEdges(); }
