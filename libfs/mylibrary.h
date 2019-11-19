#ifndef _MYLIBRARY_H
#define _MYLIBRARY_H

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "fs.h"

/************************* GENERAL METHODS ********************************/

bool fs_mount_init(const char *diskname);
void fs_unmount_procedure();
bool fs_is_mounted();
void fs_print_info();

/************************* ROOT BLOCK ********************************/
bool fs_mount_read_root_directory_block();
void set_free_root_entry_cnt();

/************************* SUPER BLOCK ********************************/

bool fs_mount_read_superblock();

/************************* FAT BLOCK ********************************/

void set_free_FAT_block_cnt();
bool fs_mount_read_fat_section();

#endif /* _MYLIBRARY_H */