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
int find_file_size(int fd);
uint16_t find_first_data_blk_idx_of_a_file(const char *filename);

/************************* SUPER BLOCK ********************************/

bool fs_mount_read_superblock();

/************************* FAT BLOCK ********************************/

void set_free_FAT_entry_cnt();
bool fs_mount_read_fat_section();
uint16_t find_data_blk_idx_by_offset(int fd);
uint16_t find_idx_of_next_data_blk(uint16_t cur_data_blk_idx);

/************************* FILE DESCRIPTOR TABLE ********************************/
int get_new_fd(const char *filename);
bool fd_is_in_use(int fd);
bool file_is_open(const char *filename);
void close_fd(int fd);
void change_fd_offset(int fd, size_t offset);

/************************* FS_READ_AND_WRITE ***************************/
int fs_read_impl(int fd, void *buf, size_t count);
int fs_write_impl(int fd, void *buf, size_t count);

/************************* HELPER METHODS ***************************/
bool is_filename_valid(const char *filename);

#endif /* _MYLIBRARY_H */