obj-m += full_file_system.o
full_file_system-objs += file_system.o file.o dir.o

all:
	gcc singlefilemakefs.c -o singlefilemakefs
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm singlefilemakefs
	rm image

create-fs:
	dd bs=4096 count=100 if=/dev/zero of=image
	./singlefilemakefs image

mount-module:
	sudo insmod full_file_system.ko

mount-fs:
	mkdir mount
	sudo mount -o loop -t soafs image ./mount/

umount-fs:
	sudo umount ./mount/
	rm -d ./mount

umount-module:
	sudo rmmod full_file_system
