#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

static struct proc_dir_entry *entry;
static char userArray[128] = "Microchip Technology\n";
int len = 1;


//open
int proc_open(struct inode *node, struct file *flip)
{
	printk(KERN_INFO "Proc Open function\n");
	return 0;
}

//read
ssize_t proc_read(struct file *flip, char *buf, size_t count, loff_t *f_pos)
{
	printk(KERN_INFO "Proc read function\n");

	if(len)
	{
		len = 0;
	}
	else
	{
		len = 1;
		return 0;
	}

	copy_to_user(buf, userArray, 128);
	return count;
}

//write
ssize_t proc_write(struct file *flip, const char *buf, size_t count, loff_t *f_pos)
{
	printk(KERN_INFO "Proc write function\n");
	copy_from_user(userArray, buf, count);
	return count;
}

//release
int proc_release(struct inode *node, struct file *flip)
{
	printk(KERN_INFO "Proc close function\n");
	return 0;
}

static struct file_operations memory_fops = {
	read: proc_read,
	write: proc_write,
	open: proc_open,
	release: proc_release
};

//driver init function
static int driver_init(void)
{
	printk(KERN_INFO "Proc_fs driver init function\n");

	//create the proc fs entry
	entry = proc_create("mydev", 0666, NULL, &memory_fops);

	return 0;
}

//driver exit function
static void driver_exit(void)
{
	printk(KERN_INFO "Proc_fs driver exit function\n");

}

module_init(driver_init);
module_exit(driver_exit);

MODULE_AUTHOR("Arun R");

