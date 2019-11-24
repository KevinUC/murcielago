#include "mylibrary.h"
#include "disk.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include "mylibrary.h"

/************************* GLOBAL VARS AND CONSTS ********************************/
uint16_t FAT_EOC = 0xFFFF;

bool _mounted = false;

/************************* ROOT BLOCK ********************************/

typedef struct rootentry
{
    /* Filename (including NULL character) */
    uint8_t _filename[FS_FILENAME_LEN];
    uint32_t _file_size_in_bytes; /* Size of the file (in bytes) */
    uint16_t _first_data_blk_idx; /* Index of the first data block */
    uint8_t _padding[10];         /* Unused/Padding */
} __attribute__((__packed__)) rootentry;

typedef struct rootdirectory
{
    rootentry _entrys[FS_FILE_MAX_COUNT];
} __attribute__((__packed__)) rootdirectory;

rootdirectory _rootdirectory;
int _free_root_entry_cnt = -1;

/************************* SUPER BLOCK ********************************/

typedef struct superblock
{
    uint8_t _signature[8];        /* Signature (must be equal to “ECS150FS”) */
    uint16_t _total_blk_cnt;      /* Total amount of blocks of virtual disk */
    uint16_t _root_blk_strt_idx;  /* Root directory block index */
    uint16_t _data_blk_strt_idx;  /* Data block start index */
    uint16_t _total_data_blk_cnt; /* Amount of data blocks */
    uint8_t _total_FAT_blk_cnt;   /* Number of blocks for FAT */
    uint8_t _padding[4079];       /* Unused/Padding */
} __attribute__((__packed__)) superblock;

superblock _superblock;
const char *_signature = "ECS150FS";

/************************* FAT BLOCK ********************************/

typedef struct fatblock
{
    uint16_t _entry[2048];              /* big array of 16 bit entry */
} __attribute__((__packed__)) fatblock; /* a single fat block*/

fatblock *_fat_section; /* array of fat blocks */
int _free_FAT_entry_cnt = -1;

int _fat_block_strt_idx = 1;
int _num_of_fat_entries_per_block = 2048;

/************************* FUNCTION IMPLEMENTATION *******************/

void fs_print_info()
{
    printf("FS Info:\n");
    printf("total_blk_count=%d\n", _superblock._total_blk_cnt);
    printf("fat_blk_count=%d\n", _superblock._total_FAT_blk_cnt);
    printf("rdir_blk=%d\n", _superblock._root_blk_strt_idx);
    printf("data_blk=%d\n", _superblock._data_blk_strt_idx);
    printf("data_blk_count=%d\n", _superblock._total_data_blk_cnt);
    printf("fat_free_ratio=%d/%d\n", _free_FAT_entry_cnt, _superblock._total_data_blk_cnt);
    printf("rdir_free_ratio=%d/%d\n", _free_root_entry_cnt, FS_FILE_MAX_COUNT);
}

void fs_print_ls()
{
    printf("FS Ls:\n");

    for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
    {

        if (_rootdirectory._entrys[i]._first_data_blk_idx != 0)
        {

            printf("file: %s, size: %d, data_blk: %d\n", _rootdirectory._entrys[i]._filename, _rootdirectory._entrys[i]._file_size_in_bytes, _rootdirectory._entrys[i]._first_data_blk_idx);
        }
    }
}

bool is_filename_valid(const char *filename)
{
    int i = 0;

    while (i < FS_FILENAME_LEN)
    {
        if (filename[i] == '\0')
        {
            if (i == 0)
            {
                break;
            }
            else
            {
                printf("filename is valid\n");
                return true;
            }
        }
        i++;
    }
    printf("filename is NOT valid\n");
    return false;
}

bool root_block_is_full()
{
    return _free_root_entry_cnt == 0;
}

bool fs_is_mounted()
{
    return _mounted;
}

void fs_unmount_procedure()
{
    free(_fat_section);
    _mounted = false;
    _free_FAT_entry_cnt = _free_root_entry_cnt = -1;
}

void set_free_root_entry_cnt()
{
    int cnt = 0;

    for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
    {
        if (_rootdirectory._entrys[i]._first_data_blk_idx == 0)
        {
            cnt++;
        }
    }

    _free_root_entry_cnt = cnt;
}

void create_new_file_on_root(const char *filename)
{

    _free_root_entry_cnt--;

    /* modify program root entry */
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
    {
        if (_rootdirectory._entrys[i]._first_data_blk_idx == 0)
        {
            _rootdirectory._entrys[i]._file_size_in_bytes = 0;
            _rootdirectory._entrys[i]._first_data_blk_idx = FAT_EOC;
            memcpy(_rootdirectory._entrys[i]._filename, filename, strlen(filename) + 1);

            /* write change to root block in the disk */
            block_write(_superblock._root_blk_strt_idx, (void *)&_rootdirectory);

            return;
        }
    }
}

void delete_file(const char *filename)
{
    _free_root_entry_cnt++;

    int idx_of_next_data_blk = -100;
    bool file_is_empty = false;

    /* 1. delete file on root block */
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
    {
        bool root_entry_is_valid = _rootdirectory._entrys[i]._first_data_blk_idx != 0;

        if (root_entry_is_valid && strcmp(filename, (const char *)_rootdirectory._entrys[i]._filename) == 0)
        {
            idx_of_next_data_blk = _rootdirectory._entrys[i]._first_data_blk_idx;
            file_is_empty = _rootdirectory._entrys[i]._file_size_in_bytes == 0;

            /* reset root entry */
            _rootdirectory._entrys[i]._first_data_blk_idx = 0;
            _rootdirectory._entrys[i]._file_size_in_bytes = 0;
            memset(_rootdirectory._entrys[i]._filename, 0, 16);

            /* write change to root block in the disk */
            block_write(_superblock._root_blk_strt_idx, (void *)&_rootdirectory);

            break;
        }
    }

    /* 2. delete file on fat block */
    assert(idx_of_next_data_blk != -100);

    if (file_is_empty)
    {
        return;
    }

    /* calculate fat block index corresponding to the first data block */
    int fat_blk_idx = idx_of_next_data_blk / _num_of_fat_entries_per_block;
    int fat_entry_idx = idx_of_next_data_blk % _num_of_fat_entries_per_block;

    /* modify fat block data structure */
    while (_fat_section[fat_blk_idx]._entry[fat_entry_idx] != FAT_EOC)
    {
        _free_FAT_entry_cnt++;

        idx_of_next_data_blk = _fat_section[fat_blk_idx]._entry[fat_entry_idx];
        _fat_section[fat_blk_idx]._entry[fat_entry_idx] = 0;

        fat_blk_idx = idx_of_next_data_blk / _num_of_fat_entries_per_block;
        fat_entry_idx = idx_of_next_data_blk % _num_of_fat_entries_per_block;
    }

    /* handle last data block */
    _fat_section[fat_blk_idx]._entry[fat_entry_idx] = 0;
    _free_FAT_entry_cnt++;

    /* write changes to fat blocks in the disk */
    for (int i = 0; i < _superblock._total_FAT_blk_cnt; i++)
    {
        block_write(_fat_block_strt_idx + i, (void *)&_fat_section[i]);
    }
}

bool filename_already_exists_in_root(const char *filename)
{
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
    {
        bool root_entry_is_valid = _rootdirectory._entrys[i]._first_data_blk_idx != 0;

        if (root_entry_is_valid && strcmp(filename, (const char *)_rootdirectory._entrys[i]._filename) == 0)
        {
            printf("filename exists in the root dir\n");
            return true;
        }
    }

    return false;
}

bool fs_mount_read_fat_section()
{
    _fat_section = malloc(_superblock._total_FAT_blk_cnt * sizeof(fatblock));
    assert(_fat_section != NULL);

    for (int i = 0; i < _superblock._total_FAT_blk_cnt; i++)
    {
        if (block_read(_fat_block_strt_idx + i, (void *)&_fat_section[i]) != 0)
        {
            printf("block_read %d th fat block fails\n", i);
            return false;
        }
    }

    if (_fat_section[0]._entry[0] != FAT_EOC) /* first entry of FAT is unused */
    {
        return false;
    }

    set_free_FAT_entry_cnt();

    return true;
}

void set_free_FAT_entry_cnt()
{
    int cnt = 0;
    bool quit = false;

    for (int i = 0; i < _superblock._total_FAT_blk_cnt; i++)
    {
        for (int j = 0; j < _num_of_fat_entries_per_block; j++)
        {
            /* check for out of bound */
            if (i * _num_of_fat_entries_per_block + j + 1 > _superblock._total_data_blk_cnt)
            {
                quit = true;
                break;
            }

            if (_fat_section[i]._entry[j] == 0) /* 0 corresponds to free data block */
            {
                cnt++;
            }
        }

        if (quit)
        {
            break;
        }
    }

    _free_FAT_entry_cnt = cnt;
}

bool fs_mount_init(const char *diskname)
{

    FAT_EOC = 0xFFFF;
    _mounted = false;
    _free_root_entry_cnt = -1;
    _signature = "ECS150FS";
    _free_FAT_entry_cnt = -1;
    _fat_block_strt_idx = 1;
    _num_of_fat_entries_per_block = 2048;

    if (!fs_mount_read_superblock())
    {
        return false;
    }

    if (!fs_mount_read_root_directory_block())
    {
        return false;
    }

    if (!fs_mount_read_fat_section())
    {
        return false;
    }

    _mounted = true;

    return true;
}

bool fs_mount_read_root_directory_block()
{
    memset(&_rootdirectory, 0, sizeof(rootdirectory));

    if (block_read(_superblock._root_blk_strt_idx,
                   (void *)&_rootdirectory) != 0)
    {
        printf("block_read root dir fails\n");
        return false;
    }

    set_free_root_entry_cnt();

    return true;
}

bool fs_mount_read_superblock()
{
    memset(&_superblock, 0, sizeof(superblock));

    /* read superblock */

    if (block_read(0, (void *)&_superblock) != 0)
    {
        printf("block_read superblock fails\n");
        return false;
    }

    /* verify superblock */

    if (strncmp((const char *)_superblock._signature, _signature, 8) != 0)
    {
        printf("signature doesn't match\n");
        return false;
    }

    if (block_disk_count() != _superblock._total_blk_cnt)
    {
        printf("block_disk_count doesn't match\n");
        return false;
    }

    return true;
}