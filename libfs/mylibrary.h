#ifndef _MYLIBRARY_H
#define _MYLIBRARY_H

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "fs.h"

/************************* GENERAL METHODS ********************************/

bool fs_mount_init(const char *diskname);
bool fs_is_mounted();
void fs_unmount_procedure();
void fs_print_info();
void fs_print_ls();
void delete_file(const char *filename); /* delete file on fat and root block */

/************************* ROOT BLOCK ********************************/
bool fs_mount_read_root_directory_block();
void set_free_root_entry_cnt();
bool root_block_is_full();
bool filename_already_exists_in_root(const char *filename);
void create_new_file_on_root(const char *filename);

/************************* SUPER BLOCK ********************************/

bool fs_mount_read_superblock();

/************************* FAT BLOCK ********************************/

void set_free_FAT_entry_cnt();
bool fs_mount_read_fat_section();

/************************* HELPER METHODS ***************************/
bool is_filename_valid(const char *filename);

#endif /* _MYLIBRARY_H */