#include "lv_port_flash_fs.h"

#include <string.h>

#include "flash_w25q256.h"

#ifndef LV_PORT_FLASH_FS_BASE_ADDR
#define LV_PORT_FLASH_FS_BASE_ADDR 0U
#endif

typedef uint32_t rawfs_addr_t;
typedef uint32_t rawfs_size_t;

typedef struct {
    rawfs_addr_t base;
    rawfs_addr_t offset;
    rawfs_size_t size;
    const char * name;
} rawfs_file_t;

#ifdef LV_USE_FS_RAWFS
#undef LV_USE_FS_RAWFS
#endif
#define LV_USE_FS_RAWFS 1

#include "../../GUI_APP/gui/test/generated/images/mergeBinFile.c"//能强制编译文件的内容，无视条件编译

typedef struct {
    uint32_t addr;
    uint32_t size;
    uint32_t pos;
} flash_file_t;

static void * flash_open(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode);
static lv_fs_res_t flash_close(lv_fs_drv_t * drv, void * file_p);
static lv_fs_res_t flash_read(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br);
static lv_fs_res_t flash_seek(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence);
static lv_fs_res_t flash_tell(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p);
static const char * flash_normalize_path(const char *path);
static const rawfs_file_t * flash_find_resource(const char *path);

void lv_port_flash_fs_init(void)
{
    static lv_fs_drv_t drv;

    lv_fs_drv_init(&drv);

    /* F: read-only resources in W25Q256, indexed by GUI Guider mergeBinFile.c. */
    drv.letter = 'F';
    drv.open_cb = flash_open;
    drv.close_cb = flash_close;
    drv.read_cb = flash_read;
    drv.seek_cb = flash_seek;
    drv.tell_cb = flash_tell;

    lv_fs_drv_register(&drv);
}

static void * flash_open(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode)
{
    const rawfs_file_t *res;
    flash_file_t *file;

    LV_UNUSED(drv);

    if(mode != LV_FS_MODE_RD) {
        return NULL;
    }

    res = flash_find_resource(path);
    if(res == NULL) {
        return NULL;
    }

    file = lv_mem_alloc(sizeof(flash_file_t));
    if(file == NULL) {
        return NULL;
    }

    file->addr = LV_PORT_FLASH_FS_BASE_ADDR + res->base + res->offset;
    file->size = res->size;
    file->pos = 0U;
    return file;
}

static lv_fs_res_t flash_close(lv_fs_drv_t * drv, void * file_p)
{
    LV_UNUSED(drv);

    if(file_p == NULL) {
        return LV_FS_RES_INV_PARAM;
    }

    lv_mem_free(file_p);
    return LV_FS_RES_OK;
}

static lv_fs_res_t flash_read(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br)
{
    flash_file_t *file = (flash_file_t *)file_p;
    uint32_t remain;
    uint32_t read_len;

    LV_UNUSED(drv);

    if(br != NULL) {
        *br = 0U;
    }

    if((file == NULL) || (buf == NULL)) {
        return LV_FS_RES_INV_PARAM;
    }

    if(file->pos > file->size) {
        return LV_FS_RES_INV_PARAM;
    }

    remain = file->size - file->pos;
    read_len = (btr > remain) ? remain : btr;
    if(read_len == 0U) {
        return LV_FS_RES_OK;
    }

    if(W25Q256_Read(file->addr + file->pos, (uint8_t *)buf, read_len) != W25Q256_OK) {
        return LV_FS_RES_HW_ERR;
    }

    file->pos += read_len;
    if(br != NULL) {
        *br = read_len;
    }

    return LV_FS_RES_OK;
}

static lv_fs_res_t flash_seek(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence)
{
    flash_file_t *file = (flash_file_t *)file_p;
    uint32_t target = 0U;

    LV_UNUSED(drv);

    if(file == NULL) {
        return LV_FS_RES_INV_PARAM;
    }

    switch(whence) {
        case LV_FS_SEEK_SET:
            target = pos;
            break;
        case LV_FS_SEEK_CUR:
            if(pos > (file->size - file->pos)) {
                return LV_FS_RES_INV_PARAM;
            }
            target = file->pos + pos;
            break;
        case LV_FS_SEEK_END:
            if(pos > file->size) {
                return LV_FS_RES_INV_PARAM;
            }
            target = file->size - pos;
            break;
        default:
            return LV_FS_RES_INV_PARAM;
    }

    if(target > file->size) {
        return LV_FS_RES_INV_PARAM;
    }

    file->pos = target;
    return LV_FS_RES_OK;
}

static lv_fs_res_t flash_tell(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p)
{
    flash_file_t *file = (flash_file_t *)file_p;

    LV_UNUSED(drv);

    if((file == NULL) || (pos_p == NULL)) {
        return LV_FS_RES_INV_PARAM;
    }

    *pos_p = file->pos;
    return LV_FS_RES_OK;
}

static const char * flash_normalize_path(const char *path)
{
    if(path == NULL) {
        return NULL;
    }

    return (path[0] == '/') ? (path + 1) : path;
}

static const rawfs_file_t * flash_find_resource(const char *path)
{
    const char *name = flash_normalize_path(path);
    const char *entry_name;
    uint32_t index;

    if((name == NULL) || (name[0] == '\0')) {
        return NULL;
    }

    for(index = 0U; index < (uint32_t)rawfs_file_count; index++) {
        entry_name = flash_normalize_path(rawfs_files[index].name);
        if((entry_name != NULL) && (strcmp(entry_name, name) == 0)) {
            return &rawfs_files[index];
        }
    }

    return NULL;
}
