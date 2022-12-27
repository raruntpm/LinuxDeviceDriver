/*
 * snull.c --  the Simple Network Utility
 *
 * Copyright (C) 2001 Alessandro Rubini and Jonathan Corbet
 * Copyright (C) 2001 O'Reilly & Associates
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.  The citation
 * should list that the code comes from the book "Linux Device
 * Drivers" by Alessandro Rubini and Jonathan Corbet, published
 * by O'Reilly & Associates.   No warranty is attached;
 * we cannot take responsibility for errors or fitness for use.
 *
 * $Id: snull.c,v 1.21 2004/11/05 02:36:03 rubini Exp $
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/moduleparam.h>

#include <linux/sched.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h> /* kmalloc() */
#include <linux/errno.h>  /* error codes */
#include <linux/types.h>  /* size_t */
#include <linux/interrupt.h> /* mark_bh */

#include <linux/in.h>
#include <linux/netdevice.h>   /* struct device, and other headers */
#include <linux/etherdevice.h> /* eth_type_trans */
#include <linux/ip.h>          /* struct iphdr */
#include <linux/tcp.h>         /* struct tcphdr */
#include <linux/skbuff.h>
#include <linux/version.h> 	/* LINUX_VERSION_CODE  */

#include "snull.h"

#include <linux/in6.h>
#include <asm/checksum.h>

MODULE_AUTHOR("Alessandro Rubini, Jonathan Corbet");
MODULE_LICENSE("Dual BSD/GPL");


/*
 * Transmitter lockup simulation, normally disabled.
 */
static int lockup = 0;
module_param(lockup, int, 0);

static int timeout = SNULL_TIMEOUT;
module_param(timeout, int, 0);

/*
 * A structure representing an in-flight packet.
 */
struct snull_packet {
	struct snull_packet *next;
	struct net_device *dev;
	int	datalen;
	u8 data[ETH_DATA_LEN];
};

int pool_size = 8;
module_param(pool_size, int, 0);

/*
 * This structure is private to each device. It is used to pass
 * packets in and out, so there is place for a packet
 */

struct snull_priv {
	struct net_device_stats stats;
	int status;
	struct snull_packet *ppool;
	struct snull_packet *rx_queue;  /* List of incoming packets */
	int rx_int_enabled;
	int tx_packetlen;
	struct sk_buff *skb;
	spinlock_t lock;
	struct net_device *dev;
	bool wol;
};

/*
 * Set up a device's packet pool.
 */
void snull_setup_pool(struct net_device *dev)
{
	struct snull_priv *priv = netdev_priv(dev);
	int i;
	struct snull_packet *pkt;

	priv->ppool = NULL;
	for (i = 0; i < pool_size; i++) {
		pkt = kmalloc (sizeof (struct snull_packet), GFP_KERNEL);
		if (pkt == NULL) {
			printk (KERN_NOTICE "Ran out of memory allocating packet pool\n");
			return;
		}
		pkt->dev = dev;
		pkt->next = priv->ppool;
		priv->ppool = pkt;
	}
}

void snull_teardown_pool(struct net_device *dev)
{
	struct snull_priv *priv = netdev_priv(dev);
	struct snull_packet *pkt;
    
	while ((pkt = priv->ppool)) {
		priv->ppool = pkt->next;
		kfree (pkt);
		/* FIXME - in-flight packets ? */
	}
}    

/*
 * Buffer/pool management.
 */
struct snull_packet *snull_get_tx_buffer(struct net_device *dev)
{
	struct snull_priv *priv = netdev_priv(dev);
	unsigned long flags;
	struct snull_packet *pkt;
    
	spin_lock_irqsave(&priv->lock, flags);
	pkt = priv->ppool;
	if(!pkt) {
		PDEBUG("Out of Pool\n");
		return pkt;
	}
	priv->ppool = pkt->next;
	if (priv->ppool == NULL) {
		printk (KERN_INFO "Pool empty\n");
		netif_stop_queue(dev);
	}
	spin_unlock_irqrestore(&priv->lock, flags);
	return pkt;
}


void snull_release_buffer(struct snull_packet *pkt)
{
	unsigned long flags;
	struct snull_priv *priv = netdev_priv(pkt->dev);
	
	spin_lock_irqsave(&priv->lock, flags);
	pkt->next = priv->ppool;
	priv->ppool = pkt;
	spin_unlock_irqrestore(&priv->lock, flags);
	if (netif_queue_stopped(pkt->dev) && pkt->next == NULL)
		netif_wake_queue(pkt->dev);
}

void snull_enqueue_buf(struct net_device *dev, struct snull_packet *pkt)
{
	unsigned long flags;
	struct snull_priv *priv = netdev_priv(dev);

	spin_lock_irqsave(&priv->lock, flags);
	pkt->next = priv->rx_queue;  /* FIXME - misorders packets */
	priv->rx_queue = pkt;
	spin_unlock_irqrestore(&priv->lock, flags);
}

struct snull_packet *snull_dequeue_buf(struct net_device *dev)
{
	struct snull_priv *priv = netdev_priv(dev);
	struct snull_packet *pkt;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);
	pkt = priv->rx_queue;
	if (pkt != NULL)
		priv->rx_queue = pkt->next;
	spin_unlock_irqrestore(&priv->lock, flags);
	return pkt;
}

/*
 * Enable and disable receive interrupts.
 */
static void snull_rx_ints(struct net_device *dev, int enable)
{
	struct snull_priv *priv = netdev_priv(dev);
	priv->rx_int_enabled = enable;
}

    
/*
 * Open and close
 */

int snull_open(struct net_device *dev)
{
	/* request_region(), request_irq(), ....  (like fops->open) */

	netif_start_queue(dev);
	return 0;
}

int snull_release(struct net_device *dev)
{
    /* release ports, irq and such -- like fops->close */

	netif_stop_queue(dev); /* can't transmit any more */
	return 0;
}

/*
 * Configuration changes (passed on by ifconfig)
 */
int snull_config(struct net_device *dev, struct ifmap *map)
{
	if (dev->flags & IFF_UP) /* can't act on a running interface */
		return -EBUSY;

	/* Don't allow changing the I/O address */
	if (map->base_addr != dev->base_addr) {
		printk(KERN_WARNING "snull: Can't change I/O address\n");
		return -EOPNOTSUPP;
	}

	/* Allow changing the IRQ */
	if (map->irq != dev->irq) {
		dev->irq = map->irq;
        	/* request_irq() is delayed to open-time */
	}

	/* ignore other fields */
	return 0;
}

/*
 * Receive a packet: retrieve, encapsulate and pass over to upper levels
 */
void snull_rx(struct net_device *dev, struct snull_packet *pkt)
{
	struct sk_buff *skb;
	struct snull_priv *priv = netdev_priv(dev);

	/*
	 * The packet has been retrieved from the transmission
	 * medium. Build an skb around it, so upper layers can handle it
	 */
	skb = dev_alloc_skb(pkt->datalen + 2);
	if (!skb) {
		if (printk_ratelimit())
			printk(KERN_NOTICE "low mem - packet dropped\n");
		priv->stats.rx_dropped++;
		goto out;
	}
	skb_reserve(skb, 2); /* align IP on 16B boundary */
	memcpy(skb_put(skb, pkt->datalen), pkt->data, pkt->datalen);

	/* Write metadata, and then pass to the receive level */
	skb->dev = dev;
	skb->protocol = eth_type_trans(skb, dev);
	skb->ip_summed = CHECKSUM_UNNECESSARY; /* don't check it */
	priv->stats.rx_packets++;
	priv->stats.rx_bytes += pkt->datalen;
	netif_rx(skb); //Non-NAPI driver
  out:
	return;
}

/*
 * The typical interrupt entry point
 */
static void
snull_regular_interrupt(int irq, void *dev_id,
				    struct pt_regs *regs)
{
	int statusword;
	struct snull_priv *priv;
	struct snull_packet *pkt = NULL;
	struct net_device *dev = (struct net_device *)dev_id;
	if (!dev)
		return;

	/* Lock the device */
	priv = netdev_priv(dev);
	spin_lock(&priv->lock);

	statusword = priv->status;
	priv->status = 0;
	if (statusword & SNULL_RX_INTR) {
		/* send it to snull_rx for handling */
		pkt = priv->rx_queue;
		if (pkt) {
			priv->rx_queue = pkt->next;
			snull_rx(dev, pkt);
		}
	}
	if (statusword & SNULL_TX_INTR) {
		/* a transmission is over: free the skb */
		priv->stats.tx_packets++;
		priv->stats.tx_bytes += priv->tx_packetlen;
		dev_kfree_skb(priv->skb);
	}

	/* Unlock the device and we are done */
	spin_unlock(&priv->lock);
	if (pkt) snull_release_buffer(pkt);
	return;
}

/*
 * Transmit a packet (low level interface)
 */
static void snull_hw_tx(char *buf, int len, struct net_device *dev)
{
	struct iphdr *ih;
	struct net_device *dest;
	struct snull_priv *priv;
	u32 *saddr, *daddr;
	struct snull_packet *tx_buffer;

	/* I am paranoid. Ain't I? */
	if (len < sizeof(struct ethhdr) + sizeof(struct iphdr)) {
		printk("snull: Hmm... packet too short (%i octets)\n",
				len);
		return;
	}

	/*
	 * Ethhdr is 14 bytes, but the kernel arranges for iphdr
	 * to be aligned (i.e., ethhdr is unaligned)
	 */
	ih = (struct iphdr *)(buf+sizeof(struct ethhdr));
	saddr = &ih->saddr;
	daddr = &ih->daddr;

	((u8 *)saddr)[2] ^= 1; /* change the third octet (class C) */
	((u8 *)daddr)[2] ^= 1;

	ih->check = 0;         /* and rebuild the checksum (ip needs it) */
	ih->check = ip_fast_csum((unsigned char *)ih,ih->ihl);

	/*
	 * Ok, now the packet is ready for transmission: first simulate a
	 * receive interrupt on the twin device, then  a
	 * transmission-done on the transmitting device
	 */
	dest = snull_devs[dev == snull_devs[0] ? 1 : 0];
	priv = netdev_priv(dest);
	tx_buffer = snull_get_tx_buffer(dev);

	if(!tx_buffer) {
		PDEBUG("Out of tx buffer, len is %i\n",len);
		return;
	}

	tx_buffer->datalen = len;
	memcpy(tx_buffer->data, buf, len);
	snull_enqueue_buf(dest, tx_buffer);
	if (priv->rx_int_enabled) {
		priv->status |= SNULL_RX_INTR;
		snull_regular_interrupt(0, dest, NULL);
	}

	priv = netdev_priv(dev);
	priv->tx_packetlen = len;
	priv->status |= SNULL_TX_INTR;
	if (lockup && ((priv->stats.tx_packets + 1) % lockup) == 0) {
        	/* Simulate a dropped transmit interrupt */
		netif_stop_queue(dev);
		PDEBUG("Simulate lockup at %ld, txp %ld\n", jiffies,
				(unsigned long) priv->stats.tx_packets);
	}
	else
		snull_regular_interrupt(0, dev, NULL);
}

/*
 * Transmit a packet (called by the kernel)
 */
int snull_tx(struct sk_buff *skb, struct net_device *dev)
{
	int len;
	char *data;
	struct snull_priv *priv = netdev_priv(dev);

	data = skb->data;
	len = skb->len;
	netif_trans_update(dev);

	/* Remember the skb, so we can free it at
	 * interrupt time */
	priv->skb = skb;

	/* actual deliver of data is device-specific,
	 * and not shown here */
	snull_hw_tx(data, len, dev);

	return 0; /* Our simple device can not fail */
}

/**
* Deal with a transmit timeout.
* See https://github.com/torvalds/linux/commit/0290bd291cc0e0488e35e66bf39efcd7d9d9122b
* for signature change which occurred on kernel 5.6
*/
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,6,0)
void snull_tx_timeout (struct net_device *dev)
#else
void snull_tx_timeout (struct net_device *dev, unsigned int txqueue)
#endif
{
	struct snull_priv *priv = netdev_priv(dev);
        struct netdev_queue *txq = netdev_get_tx_queue(dev, 0);

	PDEBUG("Transmit timeout at %ld, latency %ld\n", jiffies,
			jiffies - txq->trans_start);
        /* Simulate a transmission interrupt to get things moving */
	priv->status |= SNULL_TX_INTR;
	snull_regular_interrupt(0, dev, NULL);
	priv->stats.tx_errors++;

	/* Reset packet pool */
	spin_lock(&priv->lock);
	snull_teardown_pool(dev);
	snull_setup_pool(dev);
	spin_unlock(&priv->lock);

	netif_wake_queue(dev);
	return;
}



/*
 * Ioctl commands 
 */
int snull_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	PDEBUG("ioctl\n");
	return 0;
}

/*
 * Return statistics to the caller
 */
struct net_device_stats *snull_stats(struct net_device *dev)
{
	struct snull_priv *priv = netdev_priv(dev);
	return &priv->stats;
}

int snull_header(struct sk_buff *skb, struct net_device *dev,
                unsigned short type, const void *daddr, const void *saddr,
                unsigned len)
{
	struct ethhdr *eth = (struct ethhdr *)skb_push(skb,ETH_HLEN);

	eth->h_proto = htons(type);
	memcpy(eth->h_source, saddr ? saddr : dev->dev_addr, dev->addr_len);
	memcpy(eth->h_dest,   daddr ? daddr : dev->dev_addr, dev->addr_len);
	eth->h_dest[ETH_ALEN-1]   ^= 0x01;   /* dest is us xor 1 */
	return (dev->hard_header_len);
}


static int snull_set_mac_addr(struct net_device *dev, void *addr)
{
	int err;

	err = eth_mac_addr(dev, addr);
        if (err < 0)
                return err;

        return 0;
}

static netdev_features_t snull_features_check(struct sk_buff *skb,
                                             struct net_device *dev,
                                             netdev_features_t features)
{
	pr_info("features check");
	return features;
}

static int snull_set_features(struct net_device *netdev,
                             netdev_features_t features)
{
        netdev_features_t changed = features ^ netdev->features;

        /* TX checksum offload */
        if (changed & NETIF_F_HW_CSUM)
		pr_info("tx checksum offload");

        /* RX checksum offload */
        if (changed & NETIF_F_RXCSUM)
		pr_info("rx checksum offload");

        return 0;
}

/*
 * The "change_mtu" method is usually not needed.
 * If you need it, it must be like this.
 */
int snull_change_mtu(struct net_device *dev, int new_mtu)
{
	unsigned long flags;
	struct snull_priv *priv = netdev_priv(dev);
	spinlock_t *lock = &priv->lock;

	/* check ranges */
	if ((new_mtu < 68) || (new_mtu > 1500))
		return -EINVAL;
	/*
	 * Do anything you need, and the accept the value
	 */
	spin_lock_irqsave(lock, flags);
	dev->mtu = new_mtu;
	spin_unlock_irqrestore(lock, flags);
	return 0; /* success */
}

static int snull_get_regs_len(struct net_device *netdev)
{
	pr_err("get regs len");
	pr_err("get regs len");
        return 10 * sizeof(u32);
}

static void snull_get_regs(struct net_device *dev, struct ethtool_regs *regs,
                          void *p)
{
        u32 *regs_buff = p;
	int i;

	pr_err("get regs ");
	pr_err("get regs ");
        regs->version = 0x01;

	for( i=0; i<10; i++)
		regs_buff[i]  = i;
}

static int
snull_get_eeprom_len(struct net_device *dev)
{
	return 10 * sizeof(u32);
}

static int
snull_get_eeprom(struct net_device *dev, struct ethtool_eeprom *eeprom,
		 u8 *eebuf)
{
        u8 *regs_buff = eebuf;
	int i;

	pr_err("get regs ");
	pr_err("get regs ");

	for( i=0; i<10; i++)
		regs_buff[i]  = i;

	return 0;
}

static void snull_get_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	struct snull_priv *priv = netdev_priv(dev);

	wol->supported |= WAKE_MAGIC;

	if (priv->wol)
		wol->wolopts |= WAKE_MAGIC;
}

static int snull_set_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	struct snull_priv *priv = netdev_priv(dev);

        if (wol->wolopts & WAKE_MAGIC)
		priv->wol = true;
        else
		priv->wol = false;

	return 0;
}

static const struct ethtool_ops snull_ethtool_ops = {
        .get_link               = ethtool_op_get_link,
        .get_ts_info            = ethtool_op_get_ts_info,
	.set_wol		= snull_set_wol,
	.get_wol		= snull_get_wol,
};

static const struct header_ops snull_header_ops = {
        .create  = snull_header,
};

static const struct net_device_ops snull_netdev_ops = {
	.ndo_open            = snull_open,
	.ndo_stop            = snull_release,
	.ndo_start_xmit      = snull_tx,
	.ndo_do_ioctl        = snull_ioctl,
	.ndo_set_config      = snull_config,
	.ndo_get_stats       = snull_stats,
	.ndo_change_mtu      = snull_change_mtu,
	.ndo_tx_timeout      = snull_tx_timeout,
        .ndo_set_mac_address = snull_set_mac_addr,
	.ndo_set_features       = snull_set_features,
	.ndo_features_check     = snull_features_check,
};

/*
 * The init function (sometimes called probe).
 * It is invoked by register_netdev()
 */
void snull_init(struct net_device *dev)
{
	struct snull_priv *priv;

	dev->watchdog_timeo = timeout;
	dev->ethtool_ops = &snull_ethtool_ops;
	dev->netdev_ops = &snull_netdev_ops;
	dev->header_ops = &snull_header_ops;
	/* keep the default flags, just add NOARP */
	dev->flags           |= IFF_NOARP;
	dev->hw_features     |= NETIF_F_HW_CSUM;
	dev->hw_features     |= NETIF_F_RXCSUM;
	dev->features        = dev->hw_features;
	dev->priv_flags	     |= IFF_LIVE_ADDR_CHANGE;

	/*
	 * Then, initialize the priv field.
	 * This encloses the statistics
	 * and a few private fields.
	 */
	priv = netdev_priv(dev);
	memset(priv, 0, sizeof(struct snull_priv));

	spin_lock_init(&priv->lock);
	priv->dev = dev;

	snull_rx_ints(dev, 1);
	snull_setup_pool(dev);
}

/*
 * The devices
 */

struct net_device *snull_devs[2];

void snull_cleanup(void)
{
	int i;

	for (i = 0; i < 2;  i++) {
		if (snull_devs[i]) {
			unregister_netdev(snull_devs[i]);
			snull_teardown_pool(snull_devs[i]);
			free_netdev(snull_devs[i]);
		}
	}
	return;
}

/*
 * Finally, the module stuff
 */

int snull_init_module(void)
{
	int result, i, ret = -ENOMEM;

	/* Allocate the devices */
	snull_devs[0] = alloc_etherdev(sizeof(struct snull_priv));
	snull_devs[1] = alloc_etherdev(sizeof(struct snull_priv));
	if (snull_devs[0] == NULL || snull_devs[1] == NULL)
		goto out;

	for (i = 0; i < 2;  i++) {
		/* Intializes the device capabilities & flag */
		snull_init(snull_devs[i]);
		if ((result = register_netdev(snull_devs[i])))
			printk("snull: error %i registering device \"%s\"\n",
					result, snull_devs[i]->name);
		else
			ret = 0;
	}

	/* Copy two mac address which differ by last bit */
	memcpy(snull_devs[0]->dev_addr, "\0SNUL0", ETH_ALEN);
	memcpy(snull_devs[1]->dev_addr, "\0SNUL1", ETH_ALEN);

   out:
	if (ret)
		snull_cleanup();
	return ret;
}


module_init(snull_init_module);
module_exit(snull_cleanup);
