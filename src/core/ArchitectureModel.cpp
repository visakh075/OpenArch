#include "ArchitectureModel.h"

#include "Checksum.h"

#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

/* ============================================================
   Construction
   ============================================================ */

ArchitectureModel::ArchitectureModel(DbManager& db)
    : db_(db)
{
    reloadCache();
}

/* ============================================================
   Cache
   ============================================================ */

void ArchitectureModel::reloadCache()
{
    m_nodes  = db_.getAllNodes();
    m_layers = db_.getAllLayers();
    m_edges  = db_.getAllEdges();
}

/* ============================================================
   Checksum helpers
   ============================================================ */

uint32_t ArchitectureModel::computeNodeChecksum(
    const NodeData& n) const
{
    std::ostringstream os;

    os << n.name << "|" << n.type;

    return crc32(os.str());
}

uint32_t ArchitectureModel::computeLayerChecksum(
    const LayerData& l) const
{
    std::ostringstream os;

    os << l.name << "|" << l.kind;

    return crc32(os.str());
}

uint32_t ArchitectureModel::computeEdgeChecksum(
    const EdgeData& e) const
{
    std::ostringstream os;

    os << e.srcNode << "|"
       << e.srcLayer << "|"
       << e.dstNode << "|"
       << e.dstLayer << "|"
       << e.edgeType;

    return crc32(os.str());
}

/* ============================================================
   Nodes
   ============================================================ */

Result ArchitectureModel::addNode(
    NodeData& n,
    NodeId& outId)
{
    n.checksum = computeNodeChecksum(n);
    n.status   = Status::New;
    n.reviewer.clear();

    auto result =
        db_.createNode(n, outId);

    if (result.ok)
    {
        n.id = outId;

        m_nodes.push_back(n);
    }

    return result;
}

Result ArchitectureModel::updateNode(NodeData& n)
{
    for (auto& old : m_nodes)
    {
        if (old.id == n.id)
        {
            uint32_t newSum =
                computeNodeChecksum(n);

            if (newSum != old.checksum)
            {
                n.checksum = newSum;
                n.status   = Status::Changed;
                n.reviewer.clear();
            }
            else
            {
                n.checksum = old.checksum;
                n.status   = old.status;
                n.reviewer = old.reviewer;
            }

            auto result =
                db_.updateNode(n);

            if (result.ok)
            {
                old = n;
            }

            return result;
        }
    }

    return Result::failure("Node not found");
}

Result ArchitectureModel::deleteNode(NodeId id)
{
    auto result =
        db_.deleteNode(id);

    if (result.ok)
    {
        m_nodes.erase(
            std::remove_if(
                m_nodes.begin(),
                m_nodes.end(),
                [id](const NodeData& n)
                {
                    return n.id == id;
                }),
            m_nodes.end());
    }

    return result;
}

const std::vector<NodeData>&
ArchitectureModel::nodes() const
{
    return m_nodes;
}

Result ArchitectureModel::setNodeMetadata(
    NodeId id,
    const std::string& metadata)
{
    for (auto& n : m_nodes)
    {
        if (n.id == id)
        {
            n.metadata = metadata;

            return db_.updateNode(n);
        }
    }

    return Result::failure("Node not found");
}

Result ArchitectureModel::setNodeAttributes(
    NodeId id,
    const std::string& attributes)
{
    for (auto& n : m_nodes)
    {
        if (n.id == id)
        {
            n.attributes = attributes;

            return db_.updateNode(n);
        }
    }

    return Result::failure("Node not found");
}

Result ArchitectureModel::reviewNode(
    NodeId id,
    const std::string& reviewer)
{
    for (auto& n : m_nodes)
    {
        if (n.id == id)
        {
            n.status   = Status::Reviewed;
            n.reviewer = reviewer;

            return db_.updateNode(n);
        }
    }

    return Result::failure("Node not found");
}

/* ============================================================
   Layers
   ============================================================ */

Result ArchitectureModel::addLayer(
    LayerData& l,
    LayerId& outId)
{
    l.checksum = computeLayerChecksum(l);
    l.status   = Status::New;
    l.reviewer.clear();

    auto result =
        db_.createLayer(l, outId);

    if (result.ok)
    {
        l.id = outId;

        m_layers.push_back(l);
    }

    return result;
}

Result ArchitectureModel::updateLayer(LayerData& l)
{
    for (auto& old : m_layers)
    {
        if (old.id == l.id)
        {
            uint32_t newSum =
                computeLayerChecksum(l);

            if (newSum != old.checksum)
            {
                l.checksum = newSum;
                l.status   = Status::Changed;
                l.reviewer.clear();
            }
            else
            {
                l.checksum = old.checksum;
                l.status   = old.status;
                l.reviewer = old.reviewer;
            }

            auto result =
                db_.updateLayer(l);

            if (result.ok)
            {
                old = l;
            }

            return result;
        }
    }

    return Result::failure("Layer not found");
}

Result ArchitectureModel::deleteLayer(LayerId id)
{
    auto result =
        db_.deleteLayer(id);

    if (result.ok)
    {
        m_layers.erase(
            std::remove_if(
                m_layers.begin(),
                m_layers.end(),
                [id](const LayerData& l)
                {
                    return l.id == id;
                }),
            m_layers.end());
    }

    return result;
}

const std::vector<LayerData>&
ArchitectureModel::layers() const
{
    return m_layers;
}

Result ArchitectureModel::setLayerMetadata(
    LayerId id,
    const std::string& metadata)
{
    for (auto& l : m_layers)
    {
        if (l.id == id)
        {
            l.metadata = metadata;

            return db_.updateLayer(l);
        }
    }

    return Result::failure("Layer not found");
}

Result ArchitectureModel::setLayerAttributes(
    LayerId id,
    const std::string& attributes)
{
    for (auto& l : m_layers)
    {
        if (l.id == id)
        {
            l.attributes = attributes;

            return db_.updateLayer(l);
        }
    }

    return Result::failure("Layer not found");
}

Result ArchitectureModel::reviewLayer(
    LayerId id,
    const std::string& reviewer)
{
    for (auto& l : m_layers)
    {
        if (l.id == id)
        {
            l.status   = Status::Reviewed;
            l.reviewer = reviewer;

            return db_.updateLayer(l);
        }
    }

    return Result::failure("Layer not found");
}

/* ============================================================
   Node–Layer relationship
   ============================================================ */

Result ArchitectureModel::addNodeToLayer(
    NodeId nodeId,
    LayerId layerId)
{
    return db_.addNodeToLayer(nodeId, layerId);
}

Result ArchitectureModel::removeNodeFromLayer(
    NodeId nodeId,
    LayerId layerId)
{
    return db_.removeNodeFromLayer(nodeId, layerId);
}

std::vector<NodeLayer>
ArchitectureModel::nodesInLayer(LayerId layerId) const
{
    return db_.getNodesInLayer(layerId);
}

/* ============================================================
   Edges
   ============================================================ */

Result ArchitectureModel::addEdge(
    EdgeData& e,
    EdgeId& outId)
{
    e.checksum = computeEdgeChecksum(e);
    e.status   = Status::New;
    e.reviewer.clear();

    auto result =
        db_.createEdge(e, outId);

    if (result.ok)
    {
        e.id = outId;

        m_edges.push_back(e);
    }

    return result;
}

Result ArchitectureModel::updateEdge(EdgeData& e)
{
    for (auto& old : m_edges)
    {
        if (old.id == e.id)
        {
            uint32_t newSum =
                computeEdgeChecksum(e);

            if (newSum != old.checksum)
            {
                e.checksum = newSum;
                e.status   = Status::Changed;
                e.reviewer.clear();
            }
            else
            {
                e.checksum = old.checksum;
                e.status   = old.status;
                e.reviewer = old.reviewer;
            }

            auto result =
                db_.updateEdge(e);

            if (result.ok)
            {
                old = e;
            }

            return result;
        }
    }

    return Result::failure("Edge not found");
}

Result ArchitectureModel::deleteEdge(EdgeId id)
{
    auto result =
        db_.deleteEdge(id);

    if (result.ok)
    {
        m_edges.erase(
            std::remove_if(
                m_edges.begin(),
                m_edges.end(),
                [id](const EdgeData& e)
                {
                    return e.id == id;
                }),
            m_edges.end());
    }

    return result;
}

const std::vector<EdgeData>&
ArchitectureModel::edges() const
{
    return m_edges;
}

Result ArchitectureModel::setEdgeMetadata(
    EdgeId id,
    const std::string& metadata)
{
    for (auto& e : m_edges)
    {
        if (e.id == id)
        {
            e.metadata = metadata;

            return db_.updateEdge(e);
        }
    }

    return Result::failure("Edge not found");
}

Result ArchitectureModel::setEdgeAttributes(
    EdgeId id,
    const std::string& attributes)
{
    for (auto& e : m_edges)
    {
        if (e.id == id)
        {
            e.attributes = attributes;

            return db_.updateEdge(e);
        }
    }

    return Result::failure("Edge not found");
}

Result ArchitectureModel::reviewEdge(
    EdgeId id,
    const std::string& reviewer)
{
    for (auto& e : m_edges)
    {
        if (e.id == id)
        {
            e.status   = Status::Reviewed;
            e.reviewer = reviewer;

            return db_.updateEdge(e);
        }
    }

    return Result::failure("Edge not found");
}

GraphSnapshot ArchitectureModel::extractGraph(
    std::optional<LayerId> layerId) const
{
    GraphSnapshot snap;
    snap.layerFilter = layerId;

    //
    // 1. Layers
    //
    auto allLayers = layers();

    if (layerId)
    {
        for (const auto& l : allLayers)
        {
            if (l.id == *layerId)
            {
                snap.layers.push_back(l);
                break;
            }
        }
    }
    else
    {
        snap.layers = allLayers;
    }

    //
    // 2. Determine visible nodes
    //
    std::unordered_set<NodeId> visibleNodes;

    if (layerId)
    {
        for (const auto& nl : nodesInLayer(*layerId))
        {
            visibleNodes.insert(nl.nodeId);
        }
    }
    else
    {
        for (const auto& n : nodes())
        {
            visibleNodes.insert(n.id);
        }
    }

    //
    // 3. Nodes
    //
    for (const auto& n : nodes())
    {
        if (!layerId || visibleNodes.count(n.id))
        {
            snap.nodes.push_back(n);
        }
    }

    //
    // 4. Edges
    //
    for (const auto& e : edges())
    {
        if (visibleNodes.count(e.srcNode) &&
            visibleNodes.count(e.dstNode))
        {
            if (!layerId ||
                e.srcLayer == *layerId ||
                e.dstLayer == *layerId)
            {
                snap.edges.push_back(e);
            }
        }
    }

    return snap;
}

std::optional<NodeData>
ArchitectureModel::getNodeById(NodeId id) const
{
    for (const auto& n : m_nodes)
    {
        if (n.id == id)
            return n;
    }

    return std::nullopt;
}

std::optional<LayerData>
ArchitectureModel::getLayerById(LayerId id) const
{
    for (const auto& l : m_layers)
    {
        if (l.id == id)
            return l;
    }

    return std::nullopt;
}

std::optional<EdgeData>
ArchitectureModel::getEdgeById(EdgeId id) const
{
    for (const auto& e : m_edges)
    {
        if (e.id == id)
            return e;
    }

    return std::nullopt;
}

std::vector<NodeLayer>
ArchitectureModel::layersForNode(NodeId nodeId) const
{
    std::vector<NodeLayer> result;

    for (const auto& layer : layers())
    {
        for (const auto& nl : nodesInLayer(layer.id))
        {
            if (nl.nodeId == nodeId)
            {
                result.push_back(nl);
            }
        }
    }

    return result;
}