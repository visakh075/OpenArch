#include "Checksum.h"

/*
 * Standard CRC32 (IEEE 802.3 polynomial)
 * Fast, portable, no external dependencies.
 */

static uint32_t crc_table[256];
static bool crc_table_initialized = false;

static void init_crc_table() {
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int j = 0; j < 8; ++j) {
            if (c & 1)
                c = 0xEDB88320u ^ (c >> 1);
            else
                c >>= 1;
        }
        crc_table[i] = c;
    }
    crc_table_initialized = true;
}

uint32_t crc32(const std::string& data) {
    if (!crc_table_initialized)
        init_crc_table();

    uint32_t crc = 0xFFFFFFFFu;
    for (unsigned char ch : data) {
        crc = crc_table[(crc ^ ch) & 0xFFu] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}
