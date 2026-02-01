#pragma once

#include <cstdint>
#include <string>

/*
 * Fast CRC32 checksum.
 * Used ONLY for change detection, not cryptographic security.
 */
uint32_t crc32(const std::string& data);
