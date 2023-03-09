obj-m += my_module.o
my_module-objs += main.o ./file-system/file.o ./file-system/dir.o ./file-system/file_system.o lib/scth.o

A = $(shell cat /sys/module/the_usctm/parameters/sys_call_table_address)

all:
	# gcc singlefilemakefs.c -o singlefilemakefs
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

create-fs:
	dd bs=4096 count=100 if=/dev/zero of=image
	./singlefilemakefs image

mount-module:
	insmod my_module.ko the_syscall_table=$(A)

mount-fs:
	mkdir mount
	sudo mount -o loop -t soafs image ./mount/

umount-fs:
	sudo umount ./mount/
	rm -d ./mount

umount-module:
	sudo rmmod my_module
