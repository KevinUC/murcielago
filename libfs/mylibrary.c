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

/************************* FILE DESCRIPTOR TABLE *******************/

typedef struct fd
{
    bool _in_use; /* indicate whether this fd is currently in use */
    size_t _offset;
    uint8_t _filename[16];
} fd;

fd _fd_table[FS_OPEN_MAX_COUNT]; /* fd ranges from 0 to 31 */

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

int get_new_fd(const char *filename)
{
    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++)
    {
        if (!_fd_table[i]._in_use)
        {
            _fd_table[i]._in_use = true; /* mark this fd as used */
            memcpy(_fd_table[i]._filename, filename, strlen(filename) + 1);

            return i;
        }
    }

    return -1;
}

bool file_is_open(const char *filename)
{
    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++)
    {
        if (_fd_table[i]._in_use && strcmp((const char *)_fd_table[i]._filename, filename) == 0)
        {
            return true;
        }
    }
    return false;
}

void change_fd_offset(int fd, size_t offset)
{
    _fd_table[fd]._offset = offset;
}

void close_fd(int fd)
{
    _fd_table[fd]._in_use = false;
    _fd_table[fd]._offset = 0;
}

bool fd_is_in_use(int fd)
{
    return _fd_table[fd]._in_use;
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
            if (i == 0) /* filename is empty */
            {
                break;
            }
            else
            {
                return true;
            }
        }
        i++;
    }
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

int find_file_size(int fd)
{

    for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
    {
        bool root_entry_is_valid = _rootdirectory._entrys[i]._first_data_blk_idx != 0;

        if (root_entry_is_valid && strcmp((const char *)_fd_table[fd]._filename, (const char *)_rootdirectory._entrys[i]._filename) == 0)
        {
            return _rootdirectory._entrys[i]._file_size_in_bytes;
        }
    }

    return -1;
}

void delete_file(const char *filename)
{
    _free_root_entry_cnt++;

    uint16_t idx_of_next_data_blk = 0;
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

uint16_t find_data_blk_idx_by_offset(int fd)
{
    size_t offset = _fd_table[fd]._offset;

    uint16_t data_blk_idx = find_first_data_blk_idx_of_a_file((const char *)_fd_table[fd]._filename);
    int relative_blk_idx = offset / 4096;

    for (int i = 0; i < relative_blk_idx; i++)
    {
        data_blk_idx = find_idx_of_next_data_blk(data_blk_idx); /* go to next data blk */
    }

    return data_blk_idx;
}

void fat_allocate_extra_entry(int num, int *actual_amount_allocated, uint16_t *idx_of_1st_new_entry)
{
    bool find_first = true;
    bool quit = false;
    int prev_section_idx = -1;
    int prev_entry_idx = -1;

    *actual_amount_allocated = 0; /* at worst case, no new entry is allocated */

    if (num == 0)
    {
        return; /* no block needed to allocate */
    }

    /* write change to superblock */

    for (int i = 0; i < _superblock._total_FAT_blk_cnt; i++)
    {
        for (int j = 0; j < _num_of_fat_entries_per_block; j++)
        {
            /* we have iterated thru all fat entries, but cannot find sufficent space */
            if (i * _num_of_fat_entries_per_block + j + 1 > _superblock._total_data_blk_cnt)
            {
                quit = true;
                break;
            }

            if (_fat_section[i]._entry[j] == 0) /* 0 corresponds to free data block */
            {
                *actual_amount_allocated = *actual_amount_allocated + 1;

                if (find_first)
                {
                    /* find the first availble block */
                    *idx_of_1st_new_entry = i * _num_of_fat_entries_per_block + j;
                    find_first = false;
                }
                else
                {
                    /* point prev entry to this entry */
                    _fat_section[prev_section_idx]._entry[prev_entry_idx] = i * _num_of_fat_entries_per_block + j;
                }

                /* set cur entry as prev */
                prev_section_idx = i;
                prev_entry_idx = j;

                /* temporarily mark cur entry as eof */
                _fat_section[i]._entry[j] = FAT_EOC;

                if (*actual_amount_allocated == num)
                {
                    /* all blocks have been allocated */
                    quit = true;
                    break;
                }
            }
        } // inner for

        if (quit)
        {
            break;
        }
    } // outer for

    /* write change to fat section */
    for (int i = 0; i < _superblock._total_FAT_blk_cnt; i++)
    {
        assert(block_write(_fat_block_strt_idx + i, (void *)&_fat_section[i]) == 0);
    }
}

void update_idx_of_1st_data_blk_in_root(int fd, uint16_t idx)
{
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
    {
        bool root_entry_is_valid = _rootdirectory._entrys[i]._first_data_blk_idx != 0;

        if (root_entry_is_valid && strcmp((const char *)_fd_table[fd]._filename, (const char *)_rootdirectory._entrys[i]._filename) == 0)
        {
            /* write change to root block */
            _rootdirectory._entrys[i]._first_data_blk_idx = idx;
            block_write(_superblock._root_blk_strt_idx, (void *)&_rootdirectory);
            return;
        }
    }
}

void update_last_fat_entry_of_a_file(int fd, uint16_t new_entry)
{
    /* find idx of first fat entry of the file */
    uint16_t data_blk_idx = find_first_data_blk_idx_of_a_file((const char *)_fd_table[fd]._filename);

    while (data_blk_idx != FAT_EOC)
    {
        data_blk_idx = find_idx_of_next_data_blk(data_blk_idx);
    }

    /* write change to fat block */
    _fat_section[data_blk_idx / 2048]._entry[data_blk_idx % 2048] = new_entry;
    assert(block_write(_fat_block_strt_idx + (data_blk_idx / 2048), &_fat_section[data_blk_idx / 2048]) == 0);
}

void update_file_size(int fd, uint32_t size)
{
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
    {
        bool root_entry_is_valid = _rootdirectory._entrys[i]._first_data_blk_idx != 0;

        if (root_entry_is_valid && strcmp((const char *)_fd_table[fd]._filename, (const char *)_rootdirectory._entrys[i]._filename) == 0)
        {
            /* write change to root block */
            _rootdirectory._entrys[i]._file_size_in_bytes = size;
            block_write(_superblock._root_blk_strt_idx, (void *)&_rootdirectory);
            return;
        }
    }
}

int fs_write_impl(int fd, void *buf, size_t count)
{
    if (count <= 0)
    {
        return 0; /* nothing to write to the disk */
    }

    /* 1. allocate additonal fat block if needed */
    int cur_file_size = find_file_size(fd);
    int cur_total_byte_allocated = fat_ceil(cur_file_size);
    int cur_file_offset = _fd_table[fd]._offset;

    if (cur_file_offset + count > cur_total_byte_allocated)
    {
        /* start allocating extra fat entry */
        int num_of_extra_entry_needed = (fat_ceil(cur_file_offset + count) - cur_total_byte_allocated) / 4096;
        int actual_amount_allocated = 10000;       /* could be less than amount required if disk runs out of space */
        uint16_t idx_of_1st_new_fat_entry = 10000; /* we need this to concatnate the last entry of the original file */

        fat_allocate_extra_entry(num_of_extra_entry_needed, &actual_amount_allocated, &idx_of_1st_new_fat_entry);

        if (actual_amount_allocated < num_of_extra_entry_needed)
        {
            /* truncate number of bytes to write */
            count = (fat_ceil(cur_file_size) - cur_file_offset) + 4096 * actual_amount_allocated;
        }

        if (cur_file_size == 0)
        {
            /* if file is currently empty, need to update index of the first data block on root */
            update_idx_of_1st_data_blk_in_root(fd, idx_of_1st_new_fat_entry);
        }
        else
        {
            /* file is currently not empty, need to update the last fat entry */
            update_last_fat_entry_of_a_file(fd, idx_of_1st_new_fat_entry);
        }
    }

    /* 2. write contents to the disk, the logic is mostly identical to fs_read_impl() */
    uint16_t data_blk_idx = find_data_blk_idx_by_offset(fd); /* find index of first data block */
    int buf_offset = 0;
    int remaining_bytes_to_write = count;
    int in_blk_offset = cur_file_offset % 4096; /* in_blk_offset lies in between 0 and 4095 */
    uint8_t data_blk[4096];

    assert(block_read(_superblock._data_blk_strt_idx + data_blk_idx, (void *)data_blk) == 0);

    while (remaining_bytes_to_write != 0)
    {
        if (in_blk_offset + remaining_bytes_to_write <= 4096) /* last blk to write */
        {
            memcpy(data_blk + in_blk_offset, buf + buf_offset, remaining_bytes_to_write);              /* input_buf => data_buf */
            assert(block_write(_superblock._data_blk_strt_idx + data_blk_idx, (void *)data_blk) == 0); /* data_buf => disk */
            break;
        }
        else
        {
            int num_of_bytes_to_write_to_this_blk = 4096 - in_blk_offset;
            remaining_bytes_to_write -= num_of_bytes_to_write_to_this_blk;

            memcpy(data_blk + in_blk_offset, buf + buf_offset, num_of_bytes_to_write_to_this_blk);     /* input_buf => data_buf */
            assert(block_write(_superblock._data_blk_strt_idx + data_blk_idx, (void *)data_blk) == 0); /* data_buf => disk */

            in_blk_offset = 0; /* for next blk, we will write from start */
            buf_offset += num_of_bytes_to_write_to_this_blk;
            data_blk_idx = find_idx_of_next_data_blk(data_blk_idx);

            assert(block_read(_superblock._data_blk_strt_idx + data_blk_idx, (void *)data_blk) == 0); /* fetch next data block */
        }
    }

    _fd_table[fd]._offset += count; /* increment fd offset by the amount we write */

    /* update file size if necessary */
    if (cur_file_offset + count > cur_file_size)
    {
        update_file_size(fd, (uint32_t)(cur_file_offset + count));
    }

    return count;
}

int fs_read_impl(int fd, void *buf, size_t count)
{
    int file_size = find_file_size(fd);

    if (_fd_table[fd]._offset == file_size || count <= 0)
    {
        return 0; /* already at eof or non-positive value of count  */
    }

    if (_fd_table[fd]._offset + count > file_size)
    {
        count = file_size - _fd_table[fd]._offset; /* truncate number of bytes to read */
    }

    uint16_t data_blk_idx = find_data_blk_idx_by_offset(fd); /* find index of first data block */
    int buf_offset = 0;
    int remaining_bytes_to_read = count;
    int in_blk_offset = _fd_table[fd]._offset % 4096; /* in_blk_offset lies in between 0 and 4095 */
    uint8_t data_blk[4096];

    assert(block_read(_superblock._data_blk_strt_idx + data_blk_idx, (void *)data_blk) == 0); /* read first data block */

    while (remaining_bytes_to_read != 0)
    {
        if (in_blk_offset + remaining_bytes_to_read <= 4096) /* last blk to read */
        {
            memcpy(buf + buf_offset, data_blk + in_blk_offset, remaining_bytes_to_read);
            break;
        }
        else
        {
            int num_of_bytes_to_read_from_this_blk = 4096 - in_blk_offset;

            remaining_bytes_to_read -= num_of_bytes_to_read_from_this_blk;
            memcpy(buf + buf_offset, data_blk + in_blk_offset, num_of_bytes_to_read_from_this_blk);
            in_blk_offset = 0; /* for next blk, we will read from start */
            buf_offset += num_of_bytes_to_read_from_this_blk;
            data_blk_idx = find_idx_of_next_data_blk(data_blk_idx);
            assert(block_read(_superblock._data_blk_strt_idx + data_blk_idx, (void *)data_blk) == 0); /* update data blk */
        }
    }

    _fd_table[fd]._offset += count; /* increment fd offset by the amount we read */

    return count;
}

uint16_t find_idx_of_next_data_blk(uint16_t cur_data_blk_idx)
{

    int fat_blk_idx = cur_data_blk_idx / 2048;
    int fat_entry_idx = cur_data_blk_idx % 2048;

    return _fat_section[fat_blk_idx]._entry[fat_entry_idx];
}

uint16_t find_first_data_blk_idx_of_a_file(const char *filename)
{
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
    {
        bool root_entry_is_valid = _rootdirectory._entrys[i]._first_data_blk_idx != 0;

        if (root_entry_is_valid && strcmp(filename, (const char *)_rootdirectory._entrys[i]._filename) == 0)
        {
            return _rootdirectory._entrys[i]._first_data_blk_idx;
        }
    }

    return 1000;
}

bool filename_already_exists_in_root(const char *filename)
{
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
    {
        bool root_entry_is_valid = _rootdirectory._entrys[i]._first_data_blk_idx != 0;

        if (root_entry_is_valid && strcmp(filename, (const char *)_rootdirectory._entrys[i]._filename) == 0)
        {
            return true;
        }
    }

    return false;
}

bool fs_mount_read_fat_section()
{
    _fat_section = malloc(_superblock._total_FAT_blk_cnt * sizeof(fatblock));

    for (int i = 0; i < _superblock._total_FAT_blk_cnt; i++)
    {
        if (block_read(_fat_block_strt_idx + i, (void *)&_fat_section[i]) != 0)
        {
            return false;
        }
    }

    if (_fat_section[0]._entry[0] != FAT_EOC) /* check if first entry of FAT is unused */
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

    /* init fd table */
    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++)
    {
        _fd_table[i]._in_use = false;
        _fd_table[i]._offset = 0;
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
        return false;
    }

    /* verify superblock */

    if (strncmp((const char *)_superblock._signature, _signature, 8) != 0)
    {
        return false;
    }

    if (block_disk_count() != _superblock._total_blk_cnt)
    {
        return false;
    }

    return true;
}

int fat_ceil(int file_size_in_bytes)
{
    /* e.g. 8193 => 4096*3 = 12288, 12 => 4096 */
    if (file_size_in_bytes % 4096 == 0)
    {
        return file_size_in_bytes;
    }

    return (file_size_in_bytes / 4096 + 1) * 4096;
}