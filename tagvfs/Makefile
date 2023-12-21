obj-m := tagvfs.o
tagvfs-y := common.o tag_allfiles_dir.o tag_dir.o tag_file.o tag_fs.o tag_inode.o tag_module.o tag_onlytags_dir.o tag_storage.o tag_storage_cache.o tag_tag_dir.o tag_tag_mask.o

PWD := $(CURDIR)

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
