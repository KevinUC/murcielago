#!/bin/sh

# make fresh virtual disks, create test files  
./fs_make.x disk_ref_100.fs 100 > ref.stdout
cp disk_ref_100.fs disk_lib_100.fs

./fs_make.x disk_ref_1.fs 1 > ref.stdout
cp disk_ref_1.fs disk_lib_1.fs

python3 -c "for i in range(12293): print('a', end='')" > super_large_file

# add files to disk of size 100, test fs_(un)mount, fs_write
./fs_ref.x add disk_ref_100 super_large_file >ref.stdout 2>ref.stderr
./test_fs.x add disk_lib_100 super_large_file >lib.stdout 2>lib.stderr

diff -u ref.stdout lib.stdout
diff -u ref.stderr lib.stderr

./fs_ref.x add disk_ref_100 Makefile >ref.stdout 2>ref.stderr
./test_fs.x add disk_lib_100 Makefile >lib.stdout 2>lib.stderr

diff -u ref.stdout lib.stdout
diff -u ref.stderr lib.stderr

./fs_ref.x add disk_ref_1 super_large_file >ref.stdout 2>ref.stderr
./test_fs.x add disk_lib_1 super_large_file >lib.stdout 2>lib.stderr

diff -u ref.stdout lib.stdout
diff -u ref.stderr lib.stderr

# test fs_info
./fs_ref.x info disk_ref_100.fs >ref.stdout 2>ref.stderr
./test_fs.x info disk_lib_100.fs >lib.stdout 2>lib.stderr

diff -u ref.stdout lib.stdout
diff -u ref.stderr lib.stderr

# test fs_ls
./fs_ref.x ls disk_ref_100.fs >ref.stdout 2>ref.stderr
./test_fs.x ls disk_lib_100.fs >lib.stdout 2>lib.stderr

diff -u ref.stdout lib.stdout
diff -u ref.stderr lib.stderr

# test fs_read
./fs_ref.x cat disk_ref_100 super_large_file >ref.stdout 2>ref.stderr
./test_fs.x cat disk_lib_100 super_large_file >lib.stdout 2>lib.stderr

diff -u ref.stdout lib.stdout
diff -u ref.stderr lib.stderr

./fs_ref.x cat disk_ref_100 Makefile >ref.stdout 2>ref.stderr
./test_fs.x cat disk_lib_100 Makefile >lib.stdout 2>lib.stderr

diff -u ref.stdout lib.stdout
diff -u ref.stderr lib.stderr

./fs_ref.x cat disk_ref_1 super_large_file >ref.stdout 2>ref.stderr
./test_fs.x cat disk_lib_1 super_large_file >lib.stdout 2>lib.stderr

diff -u ref.stdout lib.stdout
diff -u ref.stderr lib.stderr

# test fs_delete
./fs_ref.x rm disk_ref_100 super_large_file >ref.stdout 2>ref.stderr
./test_fs.x rm disk_lib_100 super_large_file >lib.stdout 2>lib.stderr

diff -u ref.stdout lib.stdout
diff -u ref.stderr lib.stderr

./fs_ref.x rm disk_ref_100 Makefile >ref.stdout 2>ref.stderr
./test_fs.x rm disk_lib_100 Makefile >lib.stdout 2>lib.stderr

diff -u ref.stdout lib.stdout
diff -u ref.stderr lib.stderr


# test fs_info
./fs_ref.x info disk_ref_100.fs >ref.stdout 2>ref.stderr
./test_fs.x info disk_lib_100.fs >lib.stdout 2>lib.stderr

diff -u ref.stdout lib.stdout
diff -u ref.stderr lib.stderr

# test fs_ls
./fs_ref.x ls disk_ref_100.fs >ref.stdout 2>ref.stderr
./test_fs.x ls disk_lib_100.fs >lib.stdout 2>lib.stderr

diff -u ref.stdout lib.stdout
diff -u ref.stderr lib.stderr

# clean up
rm disk_ref_100.fs disk_ref_1.fs disk_lib_100.fs disk_lib_1.fs
rm super_large_file
rm ref.stdout ref.stderr
rm lib.stdout lib.stderr






