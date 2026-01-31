#pragma once
#include "Types.h"
struct NodeData{NodeId id{};std::string name;std::string type;MetaData metadata;DataAttributes attributes;};
struct LayerData{LayerId id{};std::string name;std::string kind;MetaData metadata;DataAttributes attributes;};
struct NodeLayer{NodeId nodeId{};LayerId layerId{};};
struct EdgeData{EdgeId id{};NodeId srcNode{};LayerId srcLayer{};NodeId dstNode{};LayerId dstLayer{};std::string edgeType;MetaData metadata;DataAttributes attributes;};
