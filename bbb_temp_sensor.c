#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/fs.h>		/* everything... */
#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/regmap.h>
#include "bbb_motor_ctrl.h"

struct mcp_dev {
	struct miscdevice miscdev;
	struct i2c_client *client;
	struct regmap *map;
	u8 data[2];
};


static const struct of_device_id temp_dt_ids[] = {
	{ .compatible = "ldd,temp_sensor"},
	{ /* sentinel */ }
};

static void mcp9808_set_threshold(struct i2c_client *client,
				  unsigned char reg, unsigned char temp)
{
	u8 value[3];
	struct i2c_msg msg;

	value[0] = reg;
	value[1] = temp >> 4;
	value[2] = temp << 4 ;

	pr_err("value %x %x", value[1], value[2]);
	//regmap_write(dev->map, reg, value

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 3;
	msg.buf = value;

	if(i2c_transfer(client->adapter,&msg,1) < 0){
		pr_err("i2c_transfer fail in %s\n",__func__);
	}

}

static irqreturn_t alert_pushed_irq_handler(int irq, void *dev_id)
{
	struct mcp_dev *dev = (struct mcp_dev *)dev_id;
	unsigned int rawtmp;

	regmap_set_bits(dev->map, 0x01, 0x0020); //set bit 5
	regmap_read(dev->map, 0x05, &rawtmp);

	if (rawtmp & 0x4000) {
		pr_err("irq handler ta > tupper");
		motor_control(1);
	}
	else if (rawtmp & 0x2000){
		pr_err("irq handler ta < tlower");
		motor_control(0);
	}
	else
		pr_err("irq hanlder %x", rawtmp);

	return IRQ_HANDLED;
}

ssize_t temp_read(struct file *filp, char __user *buf, size_t count,
		  loff_t *f_pos)
{
	struct mcp_dev *dev;
//	u8 rawtmp[2];
	unsigned int rawtmp;
	u16 decimal = 0;

	if (*f_pos >= 2 )
		return 0;

	dev = container_of(filp->private_data, struct mcp_dev, miscdev);

//	temp_raw(dev, rawtmp);
	regmap_read(dev->map, 0x05, &rawtmp);

	//dev->data[0] = ((rawtmp & 0x0f00) << 4) | ((rawtmp & 0x00f0) >> 4);
	dev->data[0] = (rawtmp & 0x0ff0) >> 4;//((rawtmp & 0x0f00) << 4) | ((rawtmp & 0x00f0) >> 4);
	dev->data[1] = rawtmp & 0x0f;

	if(rawtmp & 0x08)
		decimal = 500;
	if(rawtmp & 0x04)
		decimal += 250;
	if(rawtmp & 0x02)
		decimal += 125;

	pr_info("Temperature value = %d.%d\n",dev->data[0],decimal);

	if(copy_to_user(buf,dev->data,2)) {
		pr_err("copy_to_user fail\n");
		return -EIO;
	}

	*f_pos = 2;

	return 0;
}

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = temp_read,
};

static int temp_probe (struct i2c_client *client,
		       const struct i2c_device_id *id)
{
	struct mcp_dev *dev;
	struct device *pdev = &client->dev;
	struct regmap_config config;
	unsigned int chip_id;
	int ret;
	int limit;


	dev = devm_kzalloc(pdev, sizeof(*dev), GFP_KERNEL);
	if( dev == NULL)
		return -ENOMEM;

	dev->client = client;

	/* setup the regmap configuration */
	memset(&config, 0, sizeof(config));
	config.max_register = 0x09;
	config.reg_bits = 8;
	config.val_bits = 16;

	dev->map = regmap_init_i2c(client, &config);

	regmap_read(dev->map, 0x06, &chip_id);
	pr_info("MCP9808 Manufacture ID = %x\n", chip_id);

	//Resolution - .125/c
	regmap_write(dev->map, 0x08, 0x0202);

	if(!of_property_read_u32(pdev->of_node, "upper_limit", &limit))
		mcp9808_set_threshold(client, 0x02, limit & 0xff); //upper

	if(!of_property_read_u32(pdev->of_node, "lower_limit", &limit))
		mcp9808_set_threshold(client, 0x03, limit & 0xff); //Lower

	if(!of_property_read_u32(pdev->of_node, "critical_limit", &limit))
		mcp9808_set_threshold(client, 0x04, limit & 0xff); //critical

	//Enable interrupt
	regmap_write(dev->map, 0x01, 0x000b);


	//Register the interrupt
	ret = devm_request_threaded_irq(pdev, client->irq, NULL,\
					alert_pushed_irq_handler, \
					IRQF_TRIGGER_HIGH | IRQF_ONESHOT, \
					"mcp9808_alert", dev);
	if (ret < 0)
		return ret;


	dev->miscdev.name = "temp_sensor";
	dev->miscdev.minor = MISC_DYNAMIC_MINOR;
	dev->miscdev.fops = &fops;

	ret = misc_register(&dev->miscdev);
	if (ret < 0)
		return ret;

	i2c_set_clientdata(client, dev);

	pr_info("Hello! temperature sensor probed!\n");

	return 0;
}

static int temp_remove(struct i2c_client *client)
{
	struct mcp_dev *dev = i2c_get_clientdata(client);
	pr_info("good bye reader!\n");
	misc_deregister(&dev->miscdev);
	return 0;
}

static struct i2c_driver mypdrv = {
	.probe = temp_probe,
	.remove = temp_remove,
	.driver = {
			.name = "mcp9808",
			.of_match_table = of_match_ptr(temp_dt_ids),
			.owner = THIS_MODULE,
		},
};
module_i2c_driver(mypdrv);

MODULE_AUTHOR("xxx <xxx@gmail.com>");
MODULE_LICENSE("GPL");

