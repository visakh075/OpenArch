#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

using NodeId  = uint64_t;
using LayerId = uint64_t;
using EdgeId  = uint64_t;

using MetaData       = std::unordered_map<std::string, std::string>;
using DataAttributes = std::unordered_map<std::string, std::string>;
