#ifndef CRC32_H
#define CRC32_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * CRC-32 (IEEE 802.3 / Ethernet / PNG / ZIP).
 * Streaming: Init → Update (repeat) → Final.
 * CRC32_Compute is equivalent to Init + Update + Final in one call.
 */
void CRC32_Init(uint32_t *ctx);
void CRC32_Update(uint32_t *ctx, const uint8_t *data, size_t length);
uint32_t CRC32_Final(const uint32_t *ctx);

void CRC32_Compute(uint32_t *crc, const uint8_t *data, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* CRC32_H */
