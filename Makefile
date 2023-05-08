obj-m += my_module.o
my_module-objs += main.o ./file-system/file.o ./file-system/dir.o ./file-system/file_system.o lib/scth.o ./data-structure-core/data_structure_core.o


A = $(shell cat /sys/module/the_usctm/parameters/sys_call_table_address)

# Numero totale dei blocchi esclusi i blocchi di stato
NBLOCKS_FS = 100002

UPDATE_LIST_SIZE = 2

ACTUAL_SIZE = 5

all:
	gcc ./file-system/singlefilemakefs.c -o ./file-system/singlefilemakefs
	gcc ./file-system/parametri.c -o ./file-system/parametri
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm ./file-system/singlefilemakefs
	rm ./file-system/parametri
	rm ./file-system/image
	rm ./file-system/image2

get-param:
	./file-system/parametri $(NBLOCKS_FS)

create-fs:
	dd bs=4096 count=$(PARAM) if=/dev/zero of=./file-system/image
	./file-system/singlefilemakefs ./file-system/image $(NBLOCKS_FS) $(UPDATE_LIST_SIZE) $(ACTUAL_SIZE)

create-fs-2:
	dd bs=4096 count=$(PARAM) if=/dev/zero of=./file-system/image2
	./file-system/singlefilemakefs ./file-system/image2 $(NBLOCKS_FS) $(UPDATE_LIST_SIZE) $(ACTUAL_SIZE)

mount-module:
	insmod my_module.ko the_syscall_table=$(A)

mount-fs:
	mkdir ./file-system/mount
	sudo mount -o loop,"./file-system/mount/" -t soafs ./file-system/image ./file-system/mount/

mount-fs-2:
	sudo mount -o loop,"./file-system/mount/" -t soafs ./file-system/image2 ./file-system/mount/

umount-fs:
	sudo umount ./file-system/mount/
	rm -d ./file-system/mount/

umount-module:
	sudo rmmod my_module
