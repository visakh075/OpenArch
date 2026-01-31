#pragma once
#include "Types.h"

struct NodeData {
    NodeId id{};
    std::string name;
    std::string type;
    MetaData metadata;
    DataAttributes attributes;
};

struct LayerData {
    LayerId id{};
    std::string name;
    std::string kind;
    MetaData metadata;
    DataAttributes attributes;
};

struct EdgeData {
    EdgeId id{};
    NodeId srcNode{};
    NodeId dstNode{};
    std::string edgeType;
    MetaData metadata;
    DataAttributes attributes;
};
