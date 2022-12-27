obj-m = pen_ctrl_string.o pen_driver.o pen_enum.o pen_ep_addr.o pen_info.o pen_string.o pen_urb_string.o

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
