#include "crc32.h"

#define POLY_REFLECTED 0xEDB88320u

void CRC32_Init(uint32_t *ctx)
{
    *ctx = 0xFFFFFFFFu;
}

void CRC32_Update(uint32_t *ctx, const uint8_t *data, size_t length)
{
    uint32_t reg = *ctx;

    for (size_t i = 0; i < length; i++) {
        reg ^= data[i];
        for (int b = 0; b < 8; b++) {
            reg = (reg >> 1u) ^ ((reg & 1u) ? POLY_REFLECTED : 0u);
        }
    }

    *ctx = reg;
}

uint32_t CRC32_Final(const uint32_t *ctx)
{
    return ~(*ctx);
}

void CRC32_Compute(uint32_t *crc, const uint8_t *data, size_t length)
{
    uint32_t ctx;

    CRC32_Init(&ctx);
    CRC32_Update(&ctx, data, length);
    *crc = CRC32_Final(&ctx);
}
