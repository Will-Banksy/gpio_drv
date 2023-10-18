# KERNEL := ~/rpisrc/linux
# PWD := $(shell pwd)
# obj-m += $(shell cat target/objects.txt)

# all:
# 	@mkdir -p target/src
# 	@echo $(obj-m)

# 	make ARCH=arm CROSS_COMPILE=$(CROSS) -C $(KERNEL) M=$(PWD) modules

# clean:
# 	make -C $(KERNEL) M=$(PWD) clean

TARGET := $(shell pwd | xargs basename)

all:
	make -C target KERNEL=$(KERNEL) CROSS=$(CROSS)
	cp target/$(TARGET).ko $(TARGET).ko

clean:
	rm -rf target
	rm *.ko