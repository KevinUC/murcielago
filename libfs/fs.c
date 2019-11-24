#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "disk.h"
#include "fs.h"
#include "mylibrary.h"

int fs_mount(const char *diskname)
{

	if (block_disk_open(diskname) != 0)
	{
		printf("fs_mount: block_disk_open failure \n");
		return -1;
	}

	if (!fs_mount_init(diskname)) /* init super,fat, root directory blocks */
	{
		return -1;
	}

	return 0;
}

int fs_umount(void)
{
	if (block_disk_close() != 0)
	{
		printf("fs_umount: block_disk_close faied\n");
		return -1;
	}

	fs_unmount_procedure();

	return 0;
}

int fs_info(void)
{
	if (!fs_is_mounted())
	{
		return -1; /* no underlying virtual disk was opened */
	}

	fs_print_info();

	return 0;
}

int fs_create(const char *filename)
{
	/* check if root block is full */
	if (root_block_is_full())
	{
		return -1;
	}

	/* check if filename is NULL terminated and has valid length */
	if (!is_filename_valid(filename))
	{
		return -1;
	}

	/* check if filename already exists */
	if (filename_already_exists_in_root(filename))
	{
		return -1;
	}

	/* add the new file to system and disk */
	create_new_file_on_root(filename);

	return 0;
}

int fs_delete(const char *filename)
{
	/* TODO: check if the file is currently open */

	/* check if filename is NULL terminated and has valid length */
	if (!is_filename_valid(filename))
	{
		return -1;
	}

	/* check if filename exists */
	if (!filename_already_exists_in_root(filename))
	{
		return -1;
	}

	delete_file(filename);

	return 0;
}

int fs_ls(void)
{
	if (!fs_is_mounted())
	{
		return -1; /* no underlying virtual disk was opened */
	}

	fs_print_ls();

	return 0;
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
	return 0;
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
	return 0;
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
	return 0;
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
	return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
	return 0;
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
	return 0;
}
