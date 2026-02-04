#pragma once

#include <vector>
#include <optional>

#include "Types.h"
#include "DomainObjects.h"

struct GraphSnapshot {
    std::optional<LayerId> layerFilter;

    std::vector<NodeData>  nodes;
    std::vector<EdgeData>  edges;
    std::vector<LayerData> layers;
};
