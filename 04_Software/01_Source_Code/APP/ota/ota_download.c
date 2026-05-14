#include "ota_download.h"

#include "crc32.h"
#include "flash_w25q256.h"
#include "ota_fw_slot.h"
#include "ota_info.h"
#include "system_storge.h"

#include <stdio.h>
#include <string.h>

#define OTA_STAGE_IO_BUF_SIZE 4096U

static uint8_t s_stage_wr_buf[OTA_STAGE_IO_BUF_SIZE];
static uint8_t s_stage_rd_buf[OTA_STAGE_IO_BUF_SIZE];

static int ota_is_allowed_erase_span(uint32_t address, uint32_t length)
{
    uint32_t end;

    if ((length == 0U) || ((address + length) < address)) {
        return 0;
    }

    end = address + length;

    if ((address >= OTA_FLAG1_ADDR) && (end <= (OTA_FLAG1_ADDR + OTA_FLAG_SIZE))) {
        return 1;
    }

    if ((address >= OTA_FLAG2_ADDR) && (end <= (OTA_FLAG2_ADDR + OTA_FLAG_SIZE))) {
        return 1;
    }

    if ((address >= OTA_FW_SLOT_A_ADDR) &&
        (address < (OTA_FW_SLOT_A_ADDR + OTA_FW_SLOT_A_SIZE)) &&
        (end <= (OTA_FW_SLOT_A_ADDR + OTA_FW_SLOT_A_SIZE))) {
        return 1;
    }

    if ((address >= OTA_FW_SLOT_B_ADDR) &&
        (address < (OTA_FW_SLOT_B_ADDR + OTA_FW_SLOT_B_SIZE)) &&
        (end <= (OTA_FW_SLOT_B_ADDR + OTA_FW_SLOT_B_SIZE))) {
        return 1;
    }

    return 0;
}

OTA_Update_Result_t OTA_EraseW25Aligned(uint32_t address, uint32_t length)
{
    uint32_t erase_addr;
    uint32_t erase_end;

    if (!ota_is_allowed_erase_span(address, length)) {
        return OTA_UPDATE_PARAM_ERROR;
    }

    if (W25Q256_Init() != W25Q256_OK) {
        return OTA_UPDATE_EXT_FLASH_ERROR;
    }

    erase_addr = address - (address % W25Q256_ERASE_SIZE);
    erase_end = address + length;
    erase_end = (erase_end + W25Q256_ERASE_SIZE - 1U) & ~(W25Q256_ERASE_SIZE - 1U);

    while (erase_addr < erase_end) {
        if (W25Q256_EraseSector(erase_addr) != W25Q256_OK) {
            printf("OTA erase external flash failed, addr=0x%08lX\r\n",
                   (unsigned long)erase_addr);
            return OTA_UPDATE_EXT_FLASH_ERROR;
        }

        erase_addr += W25Q256_ERASE_SIZE;
    }

    return OTA_OK;
}

OTA_Update_Result_t OTA_EraseFirmwareSlot(uint32_t slot_id)
{
    uint32_t base;
    uint32_t sz;

    base = OTA_Fw_Slot_BaseAddr(slot_id);
    sz = OTA_Fw_Slot_Size(slot_id);
    if (base == 0U || sz == 0U) {
        return OTA_UPDATE_PARAM_ERROR;
    }

    return OTA_EraseW25Aligned(base, sz);
}

OTA_Update_Result_t OTA_EraseExternalFlash(uint32_t address, uint32_t length)
{
    /*
     * Preserve legacy callers that targeted OTA_NEW_FW_ADDR region only.
     * Now delegates through generic bounded erase helper.
     */
    return OTA_EraseW25Aligned(address, length);
}

OTA_Update_Result_t OTA_StagePayloadFromFile(FIL *fp,
                                             uint32_t file_payload_offset,
                                             uint32_t payload_size,
                                             uint32_t dst_addr,
                                             uint32_t dst_region_size,
                                             uint32_t expected_crc32)
{
    FRESULT fr;
    UINT read_len;
    uint32_t offset = 0U;
    uint32_t chunk;
    uint32_t crc_ctx;
    uint32_t crc_final;

    if (fp == NULL) {
        return OTA_UPDATE_PARAM_ERROR;
    }

    if ((payload_size == 0U) || (dst_region_size == 0U)) {
        return OTA_UPDATE_PARAM_ERROR;
    }

    if (payload_size > dst_region_size) {
        return OTA_UPDATE_SIZE_ERROR;
    }

    if (!ota_is_allowed_erase_span(dst_addr, payload_size)) {
        return OTA_UPDATE_PARAM_ERROR;
    }

    if (W25Q256_Init() != W25Q256_OK) {
        return OTA_UPDATE_EXT_FLASH_ERROR;
    }

    fr = f_lseek(fp, file_payload_offset);
    if (fr != FR_OK) {
        return OTA_UPDATE_READIMAGE_ERROR;
    }

    while (offset < payload_size) {
        chunk = payload_size - offset;
        if (chunk > OTA_STAGE_IO_BUF_SIZE) {
            chunk = OTA_STAGE_IO_BUF_SIZE;
        }

        fr = f_read(fp, s_stage_wr_buf, chunk, &read_len);
        if (fr != FR_OK) {
            printf("OTA read failed, fr=%d, offset=%lu\r\n",
                   (int)fr,
                   (unsigned long)offset);
            return OTA_UPDATE_READIMAGE_ERROR;
        }

        if (read_len == 0U) {
            break;
        }

        if (W25Q256_Write(dst_addr + offset, s_stage_wr_buf, read_len) != W25Q256_OK) {
            printf("OTA external flash write failed, offset=%lu\r\n",
                   (unsigned long)offset);
            return OTA_UPDATE_WRITE_ERROR;
        }

        if (W25Q256_Read(dst_addr + offset, s_stage_rd_buf, read_len) != W25Q256_OK) {
            printf("OTA external flash readback failed, offset=%lu\r\n",
                   (unsigned long)offset);
            return OTA_UPDATE_VERIFY_ERROR;
        }

        if (memcmp(s_stage_wr_buf, s_stage_rd_buf, read_len) != 0) {
            printf("OTA external flash verify mismatch, offset=%lu\r\n",
                   (unsigned long)offset);
            return OTA_UPDATE_VERIFY_ERROR;
        }

        offset += read_len;

        printf("OTA stage progress: %lu/%lu\r\n",
               (unsigned long)offset,
               (unsigned long)payload_size);
    }

    if (offset != payload_size) {
        printf("OTA size mismatch, staged=%lu, expect=%lu\r\n",
               (unsigned long)offset,
               (unsigned long)payload_size);
        return OTA_UPDATE_SIZE_ERROR;
    }

    CRC32_Init(&crc_ctx);
    offset = 0U;
    while (offset < payload_size) {
        chunk = payload_size - offset;
        if (chunk > OTA_STAGE_IO_BUF_SIZE) {
            chunk = OTA_STAGE_IO_BUF_SIZE;
        }

        if (W25Q256_Read(dst_addr + offset, s_stage_rd_buf, chunk) != W25Q256_OK) {
            return OTA_UPDATE_VERIFY_ERROR;
        }

        CRC32_Update(&crc_ctx, s_stage_rd_buf, chunk);
        offset += chunk;
    }

    crc_final = CRC32_Final(&crc_ctx);
    if (crc_final != expected_crc32) {
        printf("OTA staged CRC mismatch: got 0x%08lX expect 0x%08lX\r\n",
               (unsigned long)crc_final,
               (unsigned long)expected_crc32);
        return OTA_UPDATE_VERIFY_ERROR;
    }

    printf("OTA staged CRC OK\r\n");

    return OTA_OK;
}

OTA_Update_Result_t OTA_StageFullFirmwarePkgFromFile(FIL *fp, uint32_t slot_id)
{
    FRESULT fr;
    UINT read_len;
    uint32_t file_size;
    uint32_t offset;
    uint32_t chunk;
    uint32_t dst_base;
    uint32_t max_slot;
    uint32_t crc_ctx;
    uint32_t crc_final;
    Image_Info_t hdr;
    UINT br;
    uint32_t hdr_sz;
    uint32_t total_expect;

    if (fp == NULL) {
        return OTA_UPDATE_PARAM_ERROR;
    }

    dst_base = OTA_Fw_Slot_BaseAddr(slot_id);
    max_slot = OTA_Fw_Slot_Size(slot_id);
    if (dst_base == 0U || max_slot == 0U) {
        return OTA_UPDATE_PARAM_ERROR;
    }

    if (W25Q256_Init() != W25Q256_OK) {
        return OTA_UPDATE_EXT_FLASH_ERROR;
    }

    file_size = (uint32_t)f_size(fp);

    hdr_sz = (uint32_t)sizeof(Image_Info_t);
    if (file_size <= hdr_sz || file_size > max_slot) {
        return OTA_UPDATE_SIZE_ERROR;
    }

    fr = f_lseek(fp, 0U);
    if (fr != FR_OK) {
        return OTA_UPDATE_READIMAGE_ERROR;
    }

    fr = f_read(fp, &hdr, sizeof(hdr), &br);
    if (fr != FR_OK || br != sizeof(hdr)) {
        return OTA_UPDATE_READIMAGE_ERROR;
    }

    if (hdr.image_size == 0U || hdr.magic != Image_INFO_MAGIC) {
        return OTA_UPDATE_IMAGE_ERROR;
    }

    total_expect = hdr_sz + hdr.image_size;
    if (file_size != total_expect) {
        return OTA_UPDATE_SIZE_ERROR;
    }

    if (total_expect > max_slot) {
        return OTA_UPDATE_SIZE_ERROR;
    }

    /* Only erase spans covering this package (4KB-aligned inside OTA_EraseW25Aligned), not entire 2MB slot. */
    if (OTA_EraseW25Aligned(dst_base, total_expect) != OTA_OK) {
        return OTA_UPDATE_EXT_FLASH_ERROR;
    }

    fr = f_lseek(fp, 0U);
    if (fr != FR_OK) {
        return OTA_UPDATE_READIMAGE_ERROR;
    }

    offset = 0U;
    while (offset < file_size) {
        chunk = file_size - offset;
        if (chunk > OTA_STAGE_IO_BUF_SIZE) {
            chunk = OTA_STAGE_IO_BUF_SIZE;
        }

        fr = f_read(fp, s_stage_wr_buf, chunk, &read_len);
        if (fr != FR_OK || read_len != chunk) {
            return OTA_UPDATE_READIMAGE_ERROR;
        }

        if (W25Q256_Write(dst_base + offset, s_stage_wr_buf, read_len) != W25Q256_OK) {
            return OTA_UPDATE_WRITE_ERROR;
        }

        if (W25Q256_Read(dst_base + offset, s_stage_rd_buf, read_len) != W25Q256_OK) {
            return OTA_UPDATE_VERIFY_ERROR;
        }

        if (memcmp(s_stage_wr_buf, s_stage_rd_buf, read_len) != 0) {
            return OTA_UPDATE_VERIFY_ERROR;
        }

        offset += read_len;
    }

    /* CRC computed over payload portion only */
    CRC32_Init(&crc_ctx);
    offset = hdr_sz;
    while (offset < file_size) {
        chunk = file_size - offset;
        if (chunk > OTA_STAGE_IO_BUF_SIZE) {
            chunk = OTA_STAGE_IO_BUF_SIZE;
        }

        if (W25Q256_Read(dst_base + offset, s_stage_rd_buf, chunk) != W25Q256_OK) {
            return OTA_UPDATE_VERIFY_ERROR;
        }

        CRC32_Update(&crc_ctx, s_stage_rd_buf, chunk);
        offset += chunk;
    }

    crc_final = CRC32_Final(&crc_ctx);
    if (crc_final != hdr.image_crc32) {
        printf("OTA staged full package CRC mismatch\r\n");
        return OTA_UPDATE_VERIFY_ERROR;
    }

    printf("OTA full package staged OK in slot=%lu\r\n", (unsigned long)slot_id);
    return OTA_OK;
}
