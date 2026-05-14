#ifndef OTA_ERROR_CODES_H
#define OTA_ERROR_CODES_H

typedef enum {
    OTA_OK = 0,
    OTA_UPDATE_FAILED = -1,
    OTA_UPDATE_PARAM_ERROR = -2,        //参数错误
    OTA_UPDATE_OPEN_ERROR = -3,         //文件打开错误
    OTA_UPDATE_SIZE_ERROR = -4,         //文件大小错误
    OTA_UPDATE_IMAGE_ERROR = -5,        //固件镜像错误
    OTA_UPDATE_EXT_FLASH_ERROR = -6,    //外部flash错误
    OTA_UPDATE_READFLAG_ERROR = -7,     //读取flag错误
    OTA_UPDATE_READIMAGE_ERROR = -8,    //读取固件错误
    OTA_UPDATE_WRITE_ERROR = -9,        //写入固件错误
    OTA_UPDATE_VERIFY_ERROR = -10,      //校验错误
    OTA_UPDATE_BOOT_INFO_ERROR = -11,   //写入bootinfo错误
    OTA_UPDATE_FLAG_ERROR = -12,        //flag错误
} OTA_Update_Result_t;

#endif /* OTA_ERROR_CODES_H */