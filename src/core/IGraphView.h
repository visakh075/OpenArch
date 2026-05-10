#pragma once

#include "GraphSnapshot.h"

/*
 * GraphView is a CONSUMER of graph data.
 * It does NOT own or define graph structures.
 * Rendering / layout logic may live here later.
 */
class IGraphView {
public:
    virtual ~IGraphView() = default;

    // Render a snapshot (GUI / visualization layer will implement this)
    virtual void render(const GraphSnapshot& snapshot) = 0;
};
