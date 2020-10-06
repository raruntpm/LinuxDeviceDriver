#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kmod.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <asm/param.h>

MODULE_LICENSE("Dual BSD/GPL");

struct timer_list exp_timer;
int delay = 5;

void do_something(struct timer_list *data)
{
	printk("your timer expired and func has been called\n");

	//comment the following code, if you need one shot timer.
	// If not you will get periodic timer
	mod_timer(&exp_timer, jiffies + msecs_to_jiffies(1000));
	
}

int timerTest_init(void)
{	
	printk("Timer Modules Init Called\n");

	timer_setup(&exp_timer, do_something, 0); //third parameter is parameter passed to function

	mod_timer(&exp_timer, jiffies + msecs_to_jiffies(1000)); //start the timer, if not activated before.

	
	return 0;
}

void timerTest_exit(void)
{
	del_timer(&exp_timer);
	printk("Timer module Exit called\n");
}

module_init(timerTest_init);
module_exit(timerTest_exit);


