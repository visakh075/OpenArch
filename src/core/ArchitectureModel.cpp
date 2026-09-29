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

    os << n.name << "|" 
       << n.type << "|" 
       << n.attributes << "|"
       << (n.parentId ? *n.parentId : 0);

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
       << e.edgeType << "|"
       << e.attributes;

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

        if (n.parentId.has_value())
        {
            markAncestorsChanged(n.parentId);
        }
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

                auto result =
                    db_.updateNode(n);

                if (result.ok)
                {
                    NodeId oldPId = old.parentId.value_or(0);
                    old = n;

                    markAncestorsChanged(n.parentId);
                    if (oldPId != 0 && oldPId != n.parentId.value_or(0))
                    {
                        markAncestorsChanged(oldPId);
                    }
                }

                return result;
            }
            else
            {
                n.checksum = old.checksum;
                n.status   = old.status;
                n.reviewer = old.reviewer;

                auto result =
                    db_.updateNode(n);

                if (result.ok)
                {
                    old = n;
                }

                return result;
            }
        }
    }

    return Result::failure("Node not found");
}

Result ArchitectureModel::deleteNode(NodeId id)
{
    std::optional<NodeId> pId;
    for (const auto& n : m_nodes)
    {
        if (n.id == id)
        {
            pId = n.parentId;
            break;
        }
    }

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

        if (pId.has_value())
        {
            markAncestorsChanged(pId);
        }
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
            n.checksum = computeNodeChecksum(n);
            n.status = Status::Changed;
            n.reviewer.clear();

            auto r = db_.updateNode(n);
            if (r.ok)
            {
                markAncestorsChanged(n.parentId);
            }
            return r;
        }
    }

    return Result::failure("Node not found");
}

Result ArchitectureModel::reviewNode(
    NodeId id,
    const std::string& reviewer,
    Status status)
{
    for (auto& n : m_nodes)
    {
        if (n.id == id)
        {
            n.status   = status;
            n.reviewer = reviewer;

            auto r = db_.updateNode(n);
            if (r.ok)
            {
                if (status == Status::Changed || status == Status::Invalid || status == Status::New)
                {
                    markAncestorsChanged(n.parentId);
                }
            }
            return r;
        }
    }

    return Result::failure("Node not found");
}

Result ArchitectureModel::setNodeGovernance(
    NodeId id,
    Status status,
    const std::string& reviewer)
{
    return reviewNode(id, reviewer, status);
}

Result ArchitectureModel::cascadeNodeGovernance(
    NodeId parentId,
    Status status,
    const std::string& reviewer,
    bool includeInternalEdges)
{
    auto res = setNodeGovernance(parentId, status, reviewer);
    if (!res.ok) return res;

    auto childIds = getChildNodeIds(parentId, true);
    for (NodeId cid : childIds)
    {
        setNodeGovernance(cid, status, reviewer);
    }

    if (includeInternalEdges)
    {
        auto edgeIds = getInternalEdgeIds(childIds);
        for (EdgeId eid : edgeIds)
        {
            setEdgeGovernance(eid, status, reviewer);
        }
    }

    return Result::success();
}

std::vector<NodeId> ArchitectureModel::getChildNodeIds(NodeId parentId, bool recursive) const
{
    std::vector<NodeId> result;
    std::vector<NodeId> queue = { parentId };
    std::unordered_set<NodeId> visited = { parentId };

    size_t head = 0;
    while (head < queue.size())
    {
        NodeId cur = queue[head++];
        for (const auto& n : m_nodes)
        {
            if (n.parentId && *n.parentId == cur && visited.find(n.id) == visited.end())
            {
                visited.insert(n.id);
                result.push_back(n.id);
                if (recursive)
                {
                    queue.push_back(n.id);
                }
            }
        }
    }
    return result;
}

std::vector<EdgeId> ArchitectureModel::getInternalEdgeIds(const std::vector<NodeId>& nodeIds) const
{
    std::unordered_set<NodeId> nodeSet(nodeIds.begin(), nodeIds.end());
    std::vector<EdgeId> result;
    for (const auto& e : m_edges)
    {
        if (nodeSet.count(e.srcNode) || nodeSet.count(e.dstNode))
        {
            result.push_back(e.id);
        }
    }
    return result;
}

void ArchitectureModel::markAncestorsChanged(std::optional<NodeId> parentId)
{
    std::unordered_set<NodeId> visited;
    auto current = parentId;
    while (current.has_value() && *current != 0)
    {
        if (visited.count(*current)) break;
        visited.insert(*current);

        std::optional<NodeId> nextParent = std::nullopt;
        for (auto& n : m_nodes)
        {
            if (n.id == *current)
            {
                if (n.status != Status::Changed)
                {
                    n.status = Status::Changed;
                    n.reviewer.clear();
                    db_.updateNode(n);
                }
                nextParent = n.parentId;
                break;
            }
        }
        current = nextParent;
    }
}

void ArchitectureModel::markEdgeEndpointsContainersChanged(NodeId srcNodeId, NodeId dstNodeId)
{
    auto checkAndMark = [this](NodeId id) {
        if (id == 0) return;
        auto nodeOpt = getNodeById(id);
        if (!nodeOpt) return;

        if (nodeOpt->parentId.has_value())
        {
            markAncestorsChanged(nodeOpt->parentId);
        }

        bool isContainerNode = false;
        for (const auto& child : m_nodes)
        {
            if (child.parentId && *child.parentId == id)
            {
                isContainerNode = true;
                break;
            }
        }
        if (!isContainerNode)
        {
            std::string t = nodeOpt->type;
            std::transform(t.begin(), t.end(), t.begin(), ::tolower);
            if (t == "container" || t == "group" || t == "box" || t == "package" || t == "cluster")
            {
                isContainerNode = true;
            }
        }

        if (isContainerNode)
        {
            markAncestorsChanged(id);
        }
    };

    checkAndMark(srcNodeId);
    checkAndMark(dstNodeId);
}

ArchitectureModel::ContainerGovernanceSummary
ArchitectureModel::getContainerGovernanceSummary(NodeId containerId) const
{
    ContainerGovernanceSummary summary;
    auto parentOpt = getNodeById(containerId);
    if (!parentOpt) return summary;

    summary.childNodeIds = getChildNodeIds(containerId, true);
    summary.totalChildren = summary.childNodeIds.size();

    std::unordered_set<NodeId> containerSet(summary.childNodeIds.begin(), summary.childNodeIds.end());
    containerSet.insert(containerId);

    for (const auto& e : m_edges)
    {
        if (containerSet.count(e.srcNode) || containerSet.count(e.dstNode))
        {
            summary.internalEdgeIds.push_back(e.id);
        }
    }
    summary.totalInternalEdges = summary.internalEdgeIds.size();

    auto severity = [](Status s) -> int {
        switch (s) {
        case Status::Invalid:  return 0;
        case Status::Changed:  return 1;
        case Status::New:      return 2;
        case Status::Reviewed: return 3;
        case Status::Approved: return 4;
        case Status::Deleted:  return 5;
        }
        return 0;
    };

    int minSeverity = severity(parentOpt->status);

    for (NodeId cid : summary.childNodeIds)
    {
        auto cOpt = getNodeById(cid);
        if (!cOpt) continue;
        switch (cOpt->status) {
        case Status::Approved: summary.approvedChildren++; break;
        case Status::Reviewed: summary.reviewedChildren++; break;
        case Status::Changed:  summary.changedChildren++;  break;
        case Status::New:      summary.newChildren++;      break;
        case Status::Invalid:  summary.invalidChildren++;  break;
        case Status::Deleted:  break;
        }
        minSeverity = std::min(minSeverity, severity(cOpt->status));
    }

    for (EdgeId eid : summary.internalEdgeIds)
    {
        auto eOpt = getEdgeById(eid);
        if (!eOpt) continue;
        switch (eOpt->status) {
        case Status::Approved: summary.approvedEdges++; break;
        case Status::Reviewed: summary.reviewedEdges++; break;
        case Status::Changed:  summary.changedEdges++;  break;
        case Status::New:      summary.newEdges++;      break;
        case Status::Invalid:  summary.invalidEdges++;  break;
        case Status::Deleted:  break;
        }
        minSeverity = std::min(minSeverity, severity(eOpt->status));
    }

    switch (minSeverity) {
    case 0: summary.rollupStatus = Status::Invalid;  break;
    case 1: summary.rollupStatus = Status::Changed;  break;
    case 2: summary.rollupStatus = Status::New;      break;
    case 3: summary.rollupStatus = Status::Reviewed; break;
    case 4: summary.rollupStatus = Status::Approved; break;
    default: summary.rollupStatus = parentOpt->status; break;
    }

    summary.hasIssues = (summary.invalidChildren > 0 || summary.changedChildren > 0 ||
                         summary.newChildren > 0     || summary.invalidEdges > 0 ||
                         summary.changedEdges > 0    || summary.newEdges > 0 ||
                         (parentOpt->status == Status::Approved && (summary.reviewedChildren > 0 || summary.reviewedEdges > 0)));

    summary.allApproved = (summary.totalChildren > 0 &&
                           summary.approvedChildren == summary.totalChildren &&
                           summary.approvedEdges == summary.totalInternalEdges &&
                           parentOpt->status == Status::Approved);

    return summary;
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
    const std::string& reviewer,
    Status status)
{
    for (auto& l : m_layers)
    {
        if (l.id == id)
        {
            l.status   = status;
            l.reviewer = reviewer;

            return db_.updateLayer(l);
        }
    }

    return Result::failure("Layer not found");
}

Result ArchitectureModel::setLayerGovernance(
    LayerId id,
    Status status,
    const std::string& reviewer)
{
    return reviewLayer(id, reviewer, status);
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

        markEdgeEndpointsContainersChanged(e.srcNode, e.dstNode);
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

                auto result =
                    db_.updateEdge(e);

                if (result.ok)
                {
                    NodeId oldSrc = old.srcNode;
                    NodeId oldDst = old.dstNode;
                    old = e;

                    markEdgeEndpointsContainersChanged(e.srcNode, e.dstNode);
                    if (oldSrc != e.srcNode || oldDst != e.dstNode)
                    {
                        markEdgeEndpointsContainersChanged(oldSrc, oldDst);
                    }
                }

                return result;
            }
            else
            {
                e.checksum = old.checksum;
                e.status   = old.status;
                e.reviewer = old.reviewer;

                auto result =
                    db_.updateEdge(e);

                if (result.ok)
                {
                    old = e;
                }

                return result;
            }
        }
    }

    return Result::failure("Edge not found");
}

Result ArchitectureModel::deleteEdge(EdgeId id)
{
    NodeId srcId = 0, dstId = 0;
    for (const auto& e : m_edges)
    {
        if (e.id == id)
        {
            srcId = e.srcNode;
            dstId = e.dstNode;
            break;
        }
    }

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

        if (srcId != 0 || dstId != 0)
        {
            markEdgeEndpointsContainersChanged(srcId, dstId);
        }
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
            e.checksum = computeEdgeChecksum(e);
            e.status = Status::Changed;
            e.reviewer.clear();

            auto r = db_.updateEdge(e);
            if (r.ok)
            {
                markEdgeEndpointsContainersChanged(e.srcNode, e.dstNode);
            }
            return r;
        }
    }

    return Result::failure("Edge not found");
}

Result ArchitectureModel::reviewEdge(
    EdgeId id,
    const std::string& reviewer,
    Status status)
{
    for (auto& e : m_edges)
    {
        if (e.id == id)
        {
            e.status   = status;
            e.reviewer = reviewer;

            auto r = db_.updateEdge(e);
            if (r.ok)
            {
                if (status == Status::Changed || status == Status::Invalid || status == Status::New)
                {
                    markEdgeEndpointsContainersChanged(e.srcNode, e.dstNode);
                }
            }
            return r;
        }
    }

    return Result::failure("Edge not found");
}

Result ArchitectureModel::setEdgeGovernance(
    EdgeId id,
    Status status,
    const std::string& reviewer)
{
    return reviewEdge(id, reviewer, status);
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

        // Draw containers in a non-connected layer if any submodule is in the layer
        bool addedAny = true;
        while (addedAny)
        {
            addedAny = false;
            for (const auto& n : nodes())
            {
                if (visibleNodes.count(n.id) && n.parentId.has_value())
                {
                    if (visibleNodes.find(*n.parentId) == visibleNodes.end())
                    {
                        visibleNodes.insert(*n.parentId);
                        addedAny = true;
                    }
                }
            }
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
        if (visibleNodes.count(n.id))
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

bool ArchitectureModel::verifyNodeIntegrity(NodeId id) const
{
    auto opt = getNodeById(id);
    if (!opt) return false;
    return computeNodeChecksum(*opt) == opt->checksum;
}

bool ArchitectureModel::verifyLayerIntegrity(LayerId id) const
{
    auto opt = getLayerById(id);
    if (!opt) return false;
    return computeLayerChecksum(*opt) == opt->checksum;
}

bool ArchitectureModel::verifyEdgeIntegrity(EdgeId id) const
{
    auto opt = getEdgeById(id);
    if (!opt) return false;
    return computeEdgeChecksum(*opt) == opt->checksum;
}

ArchitectureModel::GovernanceAuditReport ArchitectureModel::auditGovernance() const
{
    GovernanceAuditReport report;
    report.totalNodes = m_nodes.size();
    report.totalLayers = m_layers.size();
    report.totalEdges = m_edges.size();

    auto countStatus = [&](Status s) {
        switch (s) {
        case Status::New:      report.newCount++; break;
        case Status::Changed:  report.changedCount++; break;
        case Status::Reviewed: report.reviewedCount++; break;
        case Status::Approved: report.approvedCount++; break;
        case Status::Invalid:  report.invalidCount++; break;
        case Status::Deleted:  report.deletedCount++; break;
        }
    };

    for (const auto& n : m_nodes) {
        countStatus(n.status);
        if (computeNodeChecksum(n) != n.checksum) {
            report.tamperedNodes++;
            report.issues.push_back("Node '" + n.name + "' (ID " + std::to_string(n.id) + ") checksum mismatch! Stored: " + std::to_string(n.checksum) + ", Computed: " + std::to_string(computeNodeChecksum(n)));
        }
        auto summary = getContainerGovernanceSummary(n.id);
        if (summary.totalChildren > 0 && n.status == Status::Approved && summary.hasIssues) {
            report.issues.push_back("Container '" + n.name + "' (ID " + std::to_string(n.id) + ") is marked Approved, but has unapproved child components (" +
                std::to_string(summary.changedChildren) + " changed, " +
                std::to_string(summary.newChildren) + " new, " +
                std::to_string(summary.invalidChildren) + " invalid child nodes; " +
                std::to_string(summary.changedEdges) + " changed, " +
                std::to_string(summary.newEdges) + " new internal edges)");
        }
    }

    for (const auto& l : m_layers) {
        countStatus(l.status);
        if (computeLayerChecksum(l) != l.checksum) {
            report.tamperedLayers++;
            report.issues.push_back("Layer '" + l.name + "' (ID " + std::to_string(l.id) + ") checksum mismatch! Stored: " + std::to_string(l.checksum) + ", Computed: " + std::to_string(computeLayerChecksum(l)));
        }
    }

    for (const auto& e : m_edges) {
        countStatus(e.status);
        if (computeEdgeChecksum(e) != e.checksum) {
            report.tamperedEdges++;
            report.issues.push_back("Edge ID " + std::to_string(e.id) + " (" + e.edgeType + ") checksum mismatch! Stored: " + std::to_string(e.checksum) + ", Computed: " + std::to_string(computeEdgeChecksum(e)));
        }
    }

    return report;
}