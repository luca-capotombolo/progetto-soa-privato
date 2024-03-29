obj-m += my_module.o
my_module-objs += main.o ./file-system/file.o ./file-system/dir.o ./file-system/file_system.o lib/scth.o ./data-structure-core/data_structure_core.o


A = $(shell cat /sys/module/the_usctm/parameters/sys_call_table_address)

# Numero totale dei blocchi esclusi i blocchi di stato
NBLOCKS_FS = 50002

UPDATE_LIST_SIZE = 6

ACTUAL_SIZE = 3

all:
	gcc ./file-system/singlefilemakefs.c -o ./file-system/singlefilemakefs
	gcc ./file-system/parametri.c -o ./file-system/parametri
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm ./file-system/singlefilemakefs
	rm ./file-system/parametri
	rm ./file-system/image

get-param:
	./file-system/parametri $(NBLOCKS_FS)

create-fs:
	dd bs=4096 count=$(PARAM) if=/dev/zero of=./file-system/image
	./file-system/singlefilemakefs ./file-system/image $(NBLOCKS_FS) $(UPDATE_LIST_SIZE) $(ACTUAL_SIZE)

mount-module:
	insmod my_module.ko the_syscall_table=$(A)

mount-fs:
	mkdir ./file-system/mount
	sudo mount -o loop,"./file-system/mount/" -t soafs ./file-system/image ./file-system/mount/

umount-fs:
	sudo umount ./file-system/mount/
	rm -d ./file-system/mount/

umount-module:
	sudo rmmod my_module

create-test:
	gcc ./test/full-test-conc.c -o full-test
	gcc ./test/test-get.c -o get
	gcc ./test/test-put.c -o put
	gcc ./test/test-invalidate.c -o invalidate
	gcc ./test/test.c -o simple-test

rm-test:
	rm full-test
	rm get
	rm put
	rm invalidate
	rm simple-test

compile-user:
	gcc ./user/user.c -o app-user

rm-user:
	rm app-user
