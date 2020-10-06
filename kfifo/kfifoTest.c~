#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/mutex.h>
#include <linux/kfifo.h>

MODULE_LICENSE("Dual BSD/GPL");

//Declaration
#define FIFO_SIZE 32
struct kfifo test;

static int hello_init(void){
	int ret;
	unsigned char i;
	unsigned char buf[6];

	//Create the kfifo
	ret = kfifo_alloc(&test, FIFO_SIZE, GFP_KERNEL);

	//Put string into the fifo
	kfifo_in(&test, "hello", 5);

	//Show the number of elements used
	printk(KERN_INFO "fifo len: %u\n", kfifo_len(&test));

	//Get the maximum of 5 bytes from fifo 
	i = kfifo_out(&test, buf, 5);
	printk(KERN_INFO "BUF: %.*s\n", i, buf);
	
	return 0;
}

static void hello_exit(void){
	printk("End of the Kfifo Library\n");
	kfifo_free(&test);
}

module_init(hello_init);
module_exit(hello_exit);
