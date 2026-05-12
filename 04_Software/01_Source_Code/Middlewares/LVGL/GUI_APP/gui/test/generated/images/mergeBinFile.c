/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#include "lvgl.h"

#if LV_USE_FS_RAWFS

const rawfs_size_t rawfs_file_count = 3;
rawfs_file_t rawfs_files[3] = {
	0x0, 0, 768004, "/xiangtai.bin",
	0xbb804, 0, 768004, "/xiaomi.bin",
	0x177008, 0, 30004, "/chaiqvan.bin",

};

#endif  /*LV_USE_FS_RAWFS*/ 