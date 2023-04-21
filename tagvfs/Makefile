obj-m := tagvfs.o
tagvfs-y := tag_dir.o tag_file.o tag_fs.o tag_inode.o tag_module.o tag_storage.o

PWD := $(CURDIR)

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
