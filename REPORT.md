# data structure

We defined several structures in mylibrary.c to facilitate the synchronization
between the program and the virtual disk. Each structure corresponds to a
block or a section specified by the ECS150-FS. To handle superblock, we
defined a superblock struct that contains all the fields included in the real
superblock. To deal with fat blocks, we defined a fat block struct that
contains 2048 uint16_t entries, each of which maps to a real fat block entry.
Since the number of fat blocks is unknown until runtime, we used a fat block
pointer to hold an array of fat blocks that will be dynamically allocated upon
mounting. We handled the root block in a similar way as for super block since
their size is always fixed. We also stored the “ECS150FS” signature string for
comparison in a later stage. Lastly, we have an array of 32 file descriptor
structures. Each fd struct contains a Boolean value inUse that indicates
whether the fd is in use. The offset field indicates the offset position in
the file associated with the fd. Each struct definition is attached with the
packed attribute to avoid complier interference. The filename field stores the
name of the file. All these data structures are defined in mylibrary.c.

We also defined some global variables. The Boolean mounted indicates whether a
valid disk has been mounted. freeRootEntryCnt stored the number of free root
entries on the root block. Similarly, freeFATEntryCnt stores the number of
free FAT entries in the system.

# phase 1

**fs_mount()**: it starts by calling the provided block_disk_open() methods to
open the disk. If the reading is successful, it will read superblock, FAT
blocks, root blocks sequentially into the corresponding data structures. Each
reading process is handled by a customized function. For example,
fsMountReadRootBlock() is responsible for reading the root block from the
disk. It calls the provided block_read() interface to fetch the data bytes,
then counts how many root entries are free by iterating over the entire root
block struct. fsMountReadFATSection() is responsible for reading all the FAT
entries. It is called after superblock is read into the program. Since
superblock contains the number of FAT blocks in the disk, fsMountReadFATSection
() can then use malloc to allocate required space for the fat block array. It
will then loop through all the fat blocks in the disk and read them to the
array by calling block_read(). Morever, this function also verifies whether
the first entry of the first FAT block is set to FAT_EOC, as required by the
filesystem specification. fsMountReadSuperblock() is similar to
fsMountReadRootBlock(), except that it also verifies the filesystem signature
and that if the return value of block_disk_count equals to the total amount of
blocks of virtual disk stored in the superblock. If any of these read methods
fail (return -1), then fs_mount() will also return -1. Otherwise, it will
initialize the 32 file descriptors by setting their in_use fields to false and
the offset to zero. Finally, fs_mount() sets the mounted global varibel to
true, indicating that a valid disk has been mounted.

**fs_unmount()**: it first calls fsIsMounted() to verify if a valid disk has
been mounted. If not, it will return -1. Then it will call the provided
interface block_disk_close() to close the virtual disk file. Finally, it will
free the memory occupied by the fat block array, set mounted to false, and
reset other global variables to default values.

# phase 2

**fs_create()**: it first calls fsIsMounted() to verify if a valid disk has
been mounted. If not, it will return -1. Then it checks if the root block is
full. This is done by simply checking if freeRootEntryCount is zero. It will
then verify if the given filename is valid. We did this by looping
through the first sixteen characters of the filename until a NULL terminator
is captured. If no capture occurs during the loop, then the filename is
invalid. We then check if a file with the same name has already been added to
the disk. We implemented this procedure by going over each non-empty root
entry and use strcmp() to find a duplicate. If all verifications are
successful, we decrement freeRootEntryCount by one, retrieve the first
available entry in the root block, modify it accordingly, and finally use
block_write() to update the root block in the disk.

**fs_delete()**: it calls fsIsMounted() to verify if a valid disk has been
mounted. If not, it will return -1. It will then verify if the given filename
is valid and if the file exists in the disk. It will also check if this file
is currently open. This is done by looping through the file descriptor array
and see if any file descriptor associated with the file is currently in use.
Lastly, it will perform the delete procedure, which consists of incrementing
freeRootEntryCount by one, removing the corresponding entry in the root block
as well as freeing relevant fat entries. The FAT freeing procedure is the most
complicated. It calculates the position of the first FAT entry associated with
the file before iterating through the FAT entry chain, marking entries to zero,
incrementing freeFATEntryCnt until FAT_EOC is encountered. All changes made to
the data structures will be written to the disk to maintain consistency.

**fs_ls()**: it calls fsIsMounted() to verify if a valid disk has been
mounted. If not, it will return -1. It will then iterate through non-empty
entries of the root block data structure and print relevant information of
each file accordingly.

# phase 3

**fs_open()**: It verifies if a valid disk has been mounted, if the filename
provided is valid and if the file exists in the disk. Then, it will go through
the file descriptor array and locate the first unused file descriptor fd. If
no file descriptor is available, fs_open() will fail. Otherwise it will use
memcpy to copy the filename to fd’s filename field, so that fd is now
associated with the file. fs_open() will return fd (range from 0 to 31).

**fs_close()**: It verifies if a valid disk has been mounted, if the given
file descriptor is valid. Then it will find the corresponding file descriptor
entry in the array and reset its fields.

**fs_lseek()**: It verifies if a valid disk has been mounted, if the given
file descriptor is valid and if the given offset lies between zero and the
file size. Then, it will locate the corresponding file descriptor entry in the
array and update the offset field.

**fs_stat()**: It verifies if a valid disk has been mounted and if the given
file descriptor is valid. Then, it will iterate through the root block entries,
locate the entry corresponding to this file, and return the file size in bytes.

# phase 4

**fs_read()**: It verifies if a valid disk has been mounted and if the given
file descriptor is valid. Then, it will retrieve the size of the corresponding
file and compare with the offset associated with the file descriptor. If the
offset is larger, then fs_read() returns zero between no byte can be read. If
the sum of offset and the number of bytes to read (count) exceeds file size,
then we will truncate the value of count. Before reading the contents, we also
need to find the index of the first data block of the file, and then use that
calculate the index of the data block in which the offset is currently
pointing to. In high level, we iteratively read data blocks until count drops
to zero. For the current data block that we are looking at, we first determine
if this is the last block we need to read. This is done by comparing the sum
of remaining number of bytes to read and the beginning read index of the
current block to 4096. If the sum is not greater, then we read the remaining
data and quit the loop. Otherwise, we read from the beginning index all the
way to the end of the current block, and move to the next block by reading the
corresponding FAT entry. The beginning read index is initially set by fd’s
offset mod 4096. When we iterate to the next block, it will be permanently set
to zero because we will then always read from the start of the data block.
When the reading process is finished, we will increment fd’s offset by the
number of bytes read.

**fs_write()**: The error handling is exactly the same as fs_read(). In
fact, the logic behind the writing actual writing procedure also matches
fs_read(). The only noticeable difference is that fs_write() might need to
allocate additional data blocks. To decide whether allocation of extra memory
is required, we compare the sum of fd’s offset and numer of bytes to write
(count) to the current number of bytes allocated to the file (number of data
blocks occupied by the file multiplies by 4096). If the sum is not greater,
then we proceed to the writing procedure. Otherwise, we calculate the number
of extra data blocks needed, and then traverse the FAT entries to find out if
the request can be completed. If we have enough free blocks, then they will be
assigned to this file. Otherwise, we need to truncate the value of count to
fit the available free memory. If the file is currently empty, we also need to
add the index of its first data block to the corresponding root entry.
Otherwise, we will edit the last FAT entry of the file from FAT_EOC to the
index of the first allocated block. Then, we will move to the writing
procedure, which is almost the same as fs_read(), except that we now write
contents to the disk.

# testing:

Besides passing the given student shell script tester, we also added
fsMyTest.c and fsMyTest.sh. The newly added shell script verifies the output
of fs_ls and checks if the library is able to read and write large files
properly. fsMyTest.c complements the shell script by checking fs_seek, fs_open
and other interfaces that cannot be directly tested with a script.

# external source referenced

https://stackoverflow.com/questions/8568432/is-gccs-attribute-packed
-pragma-pack-unsafe

https://stackoverflow.com/questions/53112875/convert-uint16-t-hexadecimal
-number-to-decimal-in-c

https://stackoverflow.com/questions/35329617/checking-validity-of-non-null
-terminated-string?rq=1

In addition, we also viewed lots of Piazza posts, as well as the discusion
slide.
