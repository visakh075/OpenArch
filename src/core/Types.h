#pragma once

#include <cstdint>
#include <string>

/* ============================================================
   Strong IDs
   ============================================================ */
using NodeId  = uint64_t;
using LayerId = uint64_t;
using EdgeId  = uint64_t;

/* ============================================================
   Review / Lifecycle Status
   ============================================================ */
enum class Status {
    New,
    Changed,
    Reviewed,
    Approved,
    Invalid,
    Deleted
};

/* ============================================================
   Helpers (string <-> enum)
   ============================================================ */
inline std::string to_string(Status s) {
    switch (s) {
        case Status::New:      return "new";
        case Status::Changed:  return "changed";
        case Status::Reviewed: return "reviewed";
        case Status::Approved: return "approved";
        case Status::Invalid:  return "invalid";
        case Status::Deleted:  return "deleted";
    }
    return "invalid";
}

inline Status status_from_string(const std::string& s) {
    if (s == "new")      return Status::New;
    if (s == "changed") return Status::Changed;
    if (s == "reviewed")return Status::Reviewed;
    if (s == "approved")return Status::Approved;
    if (s == "invalid") return Status::Invalid;
    if (s == "deleted") return Status::Deleted;
    return Status::Invalid;
}

/* ============================================================
   Domain Objects (SINGLE SOURCE OF TRUTH)
   ============================================================ */

struct NodeData {
    NodeId id{0};

    // Core (checksum-relevant)
    std::string name;
    std::string type;

    // Non-checksum
    std::string metadata;    // JSON (UI / drawing hints)
    std::string attributes;  // JSON (comments / extra info)

    // Governance
    uint32_t checksum{0};
    Status   status{Status::New};
    std::string reviewer;
};

struct LayerData {
    LayerId id{0};

    // Core (checksum-relevant)
    std::string name;
    std::string kind;

    // Non-checksum
    std::string metadata;
    std::string attributes;

    // Governance
    uint32_t checksum{0};
    Status   status{Status::New};
    std::string reviewer;
};

struct EdgeData {
    EdgeId id{0};

    // Core (checksum-relevant)
    NodeId  srcNode{0};
    LayerId srcLayer{0};
    NodeId  dstNode{0};
    LayerId dstLayer{0};
    std::string edgeType;

    // Non-checksum
    std::string metadata;
    std::string attributes;

    // Governance
    uint32_t checksum{0};
    Status   status{Status::New};
    std::string reviewer;
};

/* ============================================================
   Relationship Helper
   ============================================================ */
struct NodeLayer {
    NodeId  nodeId{0};
    LayerId layerId{0};
};
