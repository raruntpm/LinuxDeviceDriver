#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/cdev.h>

static struct cdev c_dev;
static struct class *led_class;
static dev_t dev_number;
#define DEVICE_NAME "led"


static int my_open(struct inode *i, struct file *f)
{
	printk(KERN_INFO "Driver: Open()\n");
	return 0;
}

static int my_close(struct inode *i, struct file *f)
{
	printk(KERN_INFO "Driver: close()\n");
	return 0;
}

static ssize_t my_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{
	printk(KERN_INFO "Driver: read()\n");
	return 0;
}

static ssize_t my_write(struct file *f, const char __user *buf, size_t len, loff_t *off)
{
	printk(KERN_INFO "Driver: write()\n");
	return len; 
}

static struct file_operations pugs_fops = 
{
	.owner = THIS_MODULE,
	.open = my_open, 
	.release = my_close,
	.read = my_read, 
	.write = my_write
};
//Driver Initialization
int led_init(void){

	//Dynamic allocation of the device number
	if(alloc_chrdev_region(&dev_number, 0,1,DEVICE_NAME) < 0){
		printk("can't register new device\n");
		return -1;
	}
	
	printk(KERN_INFO "Major = %d Minor = %d\n", MAJOR(dev_number), MINOR(dev_number));

	//Creates class file in /sys/class/Testingsysfsclass
	led_class=class_create(THIS_MODULE, "TestingSysfsclass");
	printk("the sysfs class is created\n");

	//Creates Device file in /sys/class/Testingsysfsclass/led
	device_create(led_class, NULL,dev_number, NULL, DEVICE_NAME);

	//map the file operations to the device
	cdev_init(&c_dev, &pugs_fops);
	cdev_add(&c_dev, dev_number, 1);

	return 0;
}

//Driver exit function
void led_cleanup(void)
{
	device_destroy(led_class,dev_number);
	class_destroy(led_class);
	unregister_chrdev_region(dev_number, 1);
	printk("the sysfs class is destroyed\n");
	return;
}

module_init(led_init);
module_exit(led_cleanup);

MODULE_LICENSE("GPL v2");

