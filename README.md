Tag Virtual File System Tag Virtual File System

Allows you to organize files (e.g. movies) as a tag structure: e.g. select all the movies of sci-fi movies about hackers.

The peculiarity of this implementation is that the organization takes place natively for the user, i.e. it is represented in folders, files.

The file system does not save the files themselves in their entirety, but stores symbolic links to them. So you can, on the one hand, use files both with tagvfs and in "normal" mode. In addition, no disk space is wasted on storing duplicates.

Implementation: tagvfs is implemented as a kernel module in Linux. To use tagvfs you need to build it under the current kernel (Linux modules are required) Run/Install the module Mount the required folders.

The code was tested on Debian GNU/Linux 11 (bullseye), kernel 5.10.

Installation: Since tagvfs is a kernel module, it is built for the current kernel version. To build it, you will need the gcc package and the kernel header files. These are the packages ???? gcc linux????

Running/Inserting the module

sudo insmod tagvfs.ko

Mounting individual file systems:

sudo mount -t tagvfs path-to-file path-mount-point

For example: sudo mount -t tagvfs /tagvfs/tag.raw /tagvfs/tag/
