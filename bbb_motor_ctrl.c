#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/fs.h>		/* everything... */
#include <linux/miscdevice.h>
#include "bbb_motor_ctrl.h"


static struct gpio_desc *motorpos, *motorneg;
struct miscdevice miscdev;

static const struct of_device_id motor_dt_ids[] = {
	{ .compatible = "ldd,motor_ctrl", },
	{ /* sentinel */ }
};

void motor_control(unsigned char value)
{
	if (value == 0) {
		gpiod_set_value(motorpos, 0);
	}
	else if (value == 1) {
		gpiod_set_value(motorpos, 1);
	}
	else {
		pr_err("only 0 and 1 are accepted");
	}
}
EXPORT_SYMBOL(motor_control);

ssize_t motor_write(struct file *filp,
		    const char __user *buf,
		    size_t count, loff_t *f_pos)
{
	char value;
	int retval;

	if (copy_from_user(&value, buf, 1)) {
		retval = -EFAULT;
		goto out;
	}

	if (value == '0') {
		retval = count;
		gpiod_set_value(motorpos, 0);
	}
	else if (value == '1') {
		retval = count;
		gpiod_set_value(motorpos, 1);
	}
	else {
		retval = -EINVAL;
		pr_err("only 0 and 1 are accepted");
	}

out:
	return retval;
}

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.write = motor_write,
};

static int motor_probe (struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;

	motorpos = devm_gpiod_get(dev, "motorpos", GPIOD_OUT_LOW);
	if (IS_ERR(motorpos))
		return PTR_ERR(motorpos);

	motorneg = devm_gpiod_get(dev, "motorneg", GPIOD_OUT_LOW);
	if (IS_ERR(motorneg))
		return PTR_ERR(motorneg);

	gpiod_set_value(motorneg, 0);

	miscdev.name = "motor_ctrl";
	miscdev.minor = MISC_DYNAMIC_MINOR;
	miscdev.fops = &fops;

	ret = misc_register(&miscdev);
	if (ret < 0)
		return ret;

	pr_info("Hello! motor control device probed!\n");

	return 0;
}

static int motor_remove(struct platform_device *pdev)
{
	pr_info("good bye reader!\n");
	misc_deregister(&miscdev);
	return 0;
}

static struct platform_driver mypdrv = {
	.probe = motor_probe,
	.remove = motor_remove,
	.driver = {
			.name = "motor_control",
			.of_match_table = of_match_ptr(motor_dt_ids),
			.owner = THIS_MODULE,
		},
};

module_platform_driver(mypdrv);
MODULE_AUTHOR("xxx <xxx@gmail.com>");
MODULE_LICENSE("GPL");

