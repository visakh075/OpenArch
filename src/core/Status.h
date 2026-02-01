#pragma once
#include <string>

enum class Status {
    New,
    Changed,
    Reviewed,
    Approved,
    Invalid,
    Deleted
};

inline std::string to_string(Status s) {
    switch (s) {
    case Status::New:      return "new";
    case Status::Changed:  return "changed";
    case Status::Reviewed: return "reviewed";
    case Status::Approved: return "approved";
    case Status::Invalid:  return "invalid";
    case Status::Deleted:  return "deleted";
    }
    return "unknown";
}

inline Status status_from_string(const std::string& s) {
    if (s == "new") return Status::New;
    if (s == "changed") return Status::Changed;
    if (s == "reviewed") return Status::Reviewed;
    if (s == "approved") return Status::Approved;
    if (s == "invalid") return Status::Invalid;
    if (s == "deleted") return Status::Deleted;
    return Status::Invalid;
}
