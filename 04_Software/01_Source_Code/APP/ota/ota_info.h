/*
 *OTA升级流程：
 *下载新固件到外部flash，写ota_state为0xfffe
 *重启后bootloader读ota_state，如果为0xfffe，则说明有新固件,准备更新
 *通过CRC32校验新固件，写ota_state为0xfffc
 *烧录新固件到APP，写ota_state为0xfff8
 *运行检测新固件
 *如果运行检测成功，则更新ota_state为0xfff0，并清0boot_count
 *下一次重启后，bootloader读ota_state，如果为0xfff0,则说明新固件运行检测通过，进入成功处理
 *如果运行检测失败，则重启后bootloader会检测到ota_state为0xfff8
 *重启后bootloader更新ota_state为0xffe0，且boot_count加1
 *如果boot_count大于3，且ota_state为0xffe0，则进入失败处理
 *成功处理：*把ota_state写0xffff
           *更新flag区
 *失败处理：
           *把ota_state写0xffff
           *回滚固件
           *清零boot_count
 */
#ifndef OTA_INFO_H
#define OTA_INFO_H

#include <stdint.h>

/*外部flash的OTA固件区*/

/*参数区*/
#define Image_INFO_MAGIC         0x424F4F54UL   // 'BOOT'

typedef struct {
    uint32_t     magic;        //魔术字，固定为0x424F4F54UL，即'BOOT'
    uint32_t     image_size;   //固件大小
    uint32_t     image_crc32;  //固件CRC32
} Image_Info_t;

/* 内部flag区 */

#define OTA_FLAG_MAGIC      0x4F544146UL   // 'OTAF'

#define OTA_IDLE       0xFFFF   //空闲状态
#define OTA_DOWNLOADED 0xFFFE   //下载完成
#define OTA_VERIFIED   0xFFFC   //下载后通过CRC32校验
#define OTA_INSTALLED  0xFFF8   //成功烧录到APP，等待运行检测
#define OTA_SUCCESSFUL 0xFFF0   //运行检测通过
#define OTA_FAILED     0xFFE0   //运行检测失败

/** Max trial boots from bootloader after OTA_INSTALLED before rolling back to committed external package. */
#define OTA_TRIAL_BOOT_MAX       3U

typedef struct {
    uint32_t magic;           // 魔术字，固定为0x4F544146UL，即'OTAF'
    uint32_t version;         // 当前app固件版本
    uint32_t seq;             // 递增序号，判断哪个flag区更新

    uint32_t ota_state;       // 升级状态机
    uint32_t committed_slot;  /* OTA_FW_SLOT_A/B 或 NONE：已确认的完整固件槽 */
    uint32_t staging_slot;    /* OTA_FW_SLOT_*：候选固件槽 */
    uint32_t new_crc32;       // 新固件CRC32,用于比较CRC32是否相同

    uint32_t boot_count;      // 新固件试运行启动次数，超过3次则回滚。
                              // 平常是0，当运行检测失败时，这个字段会加1。超过3次则回滚，字段会清0。
    uint32_t last_result;     // 上次升级/启动失败原因

    uint32_t flag_crc32;      // 对flag做crc校验，算crc时这个字段按0算，只校验其他字段
} OTA_Flag_t;

#endif /* OTA_INFO_H */