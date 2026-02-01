#pragma once

#include <string>
#include <vector>
#include <optional>

#include "core/Types.h"

struct GraphNodeView {
    NodeId id{};
    std::string label;
    std::string type;

    std::vector<LayerId> layers;

    Status status{};
    std::string reviewer;

    std::string metadata;
    std::string attributes;
};

struct GraphEdgeView {
    EdgeId id{};

    NodeId srcNode{};
    NodeId dstNode{};

    LayerId srcLayer{};
    LayerId dstLayer{};

    std::string type;
    Status status{};

    std::string metadata;
};

struct GraphSnapshot {
    std::vector<GraphNodeView> nodes;
    std::vector<GraphEdgeView> edges;

    std::vector<LayerData> visibleLayers;
};
