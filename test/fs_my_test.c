#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <fs.h>

/* Comprehensive tests for fs_150 */

void usage()
{
    fprintf(stderr, "Usage: fs_my_test.x <diskname>\n");
    exit(1);
}

int main(int argc, char **argv)
{
    char *diskname;

    if (argc != 2)
        usage();

    diskname = argv[1];

    /* test fs_info */
    assert(fs_info() == -1); /* no disk mounted as this point */

    /* test fs_create */
    if (fs_mount(diskname) != 0)
    {
        fprintf(stderr, "please provide the name of a valid disk\n");
        exit(1);
    }

    char buf[16] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p'}; /* non NULL terminated array */
    assert(fs_create(buf) == -1);                                                                    /* invalid filename */
    assert(fs_create("qwertyuioplkjhgfd") == -1);                                                    /* filename is too long */
    assert(fs_create("file") == 0);
    assert(fs_create("file") == -1); /* file already exists */

    /* test open, stat, write and read, lseek */
    assert(fs_open(buf) == -1);
    assert(fs_open("qwertyuioplkjhgfd") == -1);

    int fd0, fd1, fd2;
    uint8_t MSG[20] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T'};
    uint8_t msg[20] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't'};
    uint8_t read_buf[20];

    fd0 = fs_open("file");
    fd1 = fs_open("file");
    fd2 = fs_open("file");

    assert(fd0 == 0 && fd1 == 1 && fd2 == 2);
    assert(fs_stat(0) == 0 && fs_stat(1) == 0 && fs_stat(2) == 0 && fs_stat(5) == -1 && fs_stat(100) == -1);

    assert(fs_write(-1, (void *)msg, 10) == -1 && fs_write(5, (void *)msg, 10) == -1);
    assert(fs_write(fd0, (void *)msg, 5) == 5); /* file: abcde */

    assert(fs_stat(fd0) == 5 && fs_stat(fd1) == 5 && fs_stat(fd2) == 5);

    assert(fs_read(5, (void *)read_buf, 5) == -1);
    assert(fs_read(fd1, (void *)read_buf, 5) == 5);
    assert(memcmp(read_buf, msg, 5) == 0);

    assert(fs_lseek(fd0, -1) == -1 && fs_lseek(fd0, 6) == -1 && fs_lseek(fd0, 100) == -1);
    assert(fs_lseek(fd1, 5) == 0); /* now fd0, fd1 both have offset = 5 */

    assert(fs_write(fd0, (void *)(msg + 5), 15) == 15); /* file: abcdefghijklmnopqrst */
    assert(fs_stat(fd2) == 20);
    assert(fs_read(fd2, (void *)read_buf, 20) == 20);
    assert(memcmp(read_buf, msg, 20) == 0);

    assert(fs_lseek(fd2, 5) == 0);
    assert(fs_write(fd2, (void *)MSG, 20) == 20); /* file: abcdeABCDEFGHIJKLMNOPQRST */
    assert(fs_lseek(fd0, 5) == 0);
    assert(fs_read(fd0, (void *)read_buf, 20) == 20);
    assert(fs_read(fd0, (void *)read_buf, 20) == 0); /* offset = 25, nothing to read */
    assert(memcmp(read_buf, MSG, 20) == 0);

    assert(fs_stat(fd2) == 25 && fs_stat(fd0) == 25 && fs_stat(fd1) == 25);

    /* additional tests */
    assert(fs_create("file2") == 0);
    int fd3 = fs_open("file2");
    assert(fd3 == 3);
    assert(fs_write(fd3, (void *)MSG, 20) == 20); /* file: abcdeABCDEFGHIJKLMNOPQRST */
    assert(fs_lseek(fd3, 12) == 0);
    assert(fs_read(fd3, (void *)read_buf, 100) == 8);
    assert(memcmp(read_buf, MSG + 12, 8) == 0);

    /* test fs_delete and fs_close, fs_ls, fs_unmount, fs_info */
    assert(fs_delete("file") == -1); /* currently open */
    assert(fs_close(fd0) == 0 && fs_close(fd1) == 0 && fs_close(fd2) == 0 && fs_close(fd3) == 0 && fs_close(100) == -1);
    assert(fs_delete("file") == 0);
    assert(fs_delete("file2") == 0);

    assert(fs_umount() == 0);
    assert(fs_umount() == -1); /* no underlying disk is open */
    assert(fs_ls() == -1);     /* no underlying disk is open */
    assert(fs_info() == -1);   /* no underlying disk is open */
}
