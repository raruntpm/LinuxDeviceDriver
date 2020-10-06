//This Function create the two files in the user space.
// /proc/wait  and /dev/myled.
// when you read the device /dev/myled, it waits for the wait queue.
// open the another terminal, write to the /proc device... echo "hello" > /proc/wait.
// when we write to proc device, it will send the wait queue, which will wake up /dev/myled read function. 


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h> // required for various structures related to files liked fops.
#include <linux/uaccess.h> // required for copy_from and copy_to user functions
#include <linux/semaphore.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/wait.h> // Required for the wait queues
#include <linux/sched.h> // Required for task states (TASK_INTERRUPTIBLE etc )


static int Major;
struct proc_dir_entry *Our_Proc_File;
wait_queue_head_t queue;
char flag = 'n';
static struct class *led_class;


struct deviceTest {
    char array[100];
    struct semaphore sem;
}char_arr;

struct cdev *kernel_cdev;


int open(struct inode *inode, struct file *filp)
{
   
    printk(KERN_INFO "Inside open \n");
    if(down_interruptible(&char_arr.sem)) {
        printk(KERN_INFO " could not hold semaphore");
        return -1;
    }
    return 0;
}


int release(struct inode *inode, struct file *filp) {
    printk (KERN_INFO "Inside close \n");
    printk(KERN_INFO "Releasing semaphore");
    up(&char_arr.sem);
    return 0;
}

ssize_t read(struct file *filp, char *buff, size_t count, loff_t *offp) {
    unsigned long ret;
    printk("Inside read \n");
    wait_event_interruptible(queue, flag != 'n');
    printk(KERN_INFO "Woken Up");
//    ret = copy_to_user(buff, char_arr.array, count);
    flag = 'n';
    return 0;
}

ssize_t write(struct file *filp, const char *buff, size_t count, loff_t *offp) {   
    unsigned long ret;
    printk(KERN_INFO "Inside write \n");
    ret =    copy_from_user(char_arr.array, buff, count);
    return ret;
}

/**
 * Declaration of the File operation for the device
 */
struct file_operations fops = {
    read:        read,
    write:        write,
    open:         open,
    release:    release
};

/**
 * Procfs Declartion
 */
ssize_t write_proc(struct file *file,const char *buffer,size_t count,loff_t *data);
ssize_t read_proc(struct file *file,char *buffer,size_t count,loff_t *data);

struct file_operations fifo_fops = {
       read:    read_proc,
       write:  write_proc
};

ssize_t read_proc(struct file *file,char *buffer,size_t count,loff_t *data)
{
	return 0;
}


ssize_t write_proc(struct file *file,const char *buffer,size_t count,loff_t *data)
{
	int ret=0;
	printk(KERN_INFO "procfile_write /proc/wait called\n");
	ret = copy_from_user(char_arr.array, buffer, count);
	printk(KERN_INFO "%s",buffer);
	flag = 'y';
	wake_up_interruptible(&queue);
	return count;
}


int create_new_proc_entry(void )
{
	//creation of procfs
    Our_Proc_File = proc_create("wait", 0, NULL, &fifo_fops);
   
    if(Our_Proc_File == NULL) {
        proc_remove(Our_Proc_File);
        printk(KERN_ALERT "Error: could not initialize /proc/wait\n");
        return -ENOMEM;
    }
    printk(KERN_INFO "/proc/wait created \n");
	return 0;   
}


/**
 * Module Entry Routine
 */
int char_arr_init (void) {
    int ret;
    dev_t dev_no,dev;


    kernel_cdev = cdev_alloc();   	//Allocation 
     kernel_cdev->ops = &fops;		// Allocate fops to cdev
    kernel_cdev->owner = THIS_MODULE;
    printk (" Inside init module\n");

	//Dynamic allocation of the major number
     ret = alloc_chrdev_region( &dev_no , 0, 1,"chr_arr_dev");
    if (ret < 0) {
        printk("Major number allocation is failed\n");
        return ret;   
    }
   
    Major = MAJOR(dev_no);
    dev = MKDEV(Major,0);
    sema_init(&char_arr.sem,1);   	//semaphore initialization
    printk (" The major number for your device is %d\n", Major);

    // Add the cdev structure
    ret = cdev_add( kernel_cdev,dev,1);
    if(ret < 0 )
    {
    printk(KERN_INFO "Unable to allocate cdev");
    return ret;
    }

    led_class = class_create(THIS_MODULE, "TestingSysfsClass");

    device_create(led_class, NULL, dev, NULL, "led");

    init_waitqueue_head(&queue);
    create_new_proc_entry();
    return 0;
}

/**
 * Module Exit Routine
 */
void char_arr_cleanup(void) {
    printk(KERN_INFO " Inside cleanup_module\n");
    proc_remove(Our_Proc_File);		//Remove the proc entry
    cdev_del(kernel_cdev);			//Delete the Cdev structure
    unregister_chrdev_region(Major, 1);		//Unregister the device
}
MODULE_LICENSE("GPL");   
module_init(char_arr_init);
module_exit(char_arr_cleanup);
