#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/rtnetlink.h>
#include <linux/kthread.h>

const static int NDEVICE = 4;

static struct net_device* _dev[4];

char* devices[] = {"eth2","eth3","eth4","eth5"};


rx_handler_result_t
rx_handler(struct sk_buff **pskb)
{

    struct netdev_queue *txq;
    struct sk_buff *skb = *pskb;
    int ret;
    skb = skb_share_check(skb, GFP_ATOMIC);
    if (!skb)
        return RX_HANDLER_CONSUMED;

    txq = netdev_get_tx_queue(skb->dev, 0);
    if (netif_queue_stopped(skb->dev) ||  netif_xmit_frozen_or_stopped(txq)) {
        kfree_skb(skb);
        return RX_HANDLER_CONSUMED;
    }

    skb_push(skb, skb->data - skb_mac_header(skb));

    if (!skb) {
        printk("Bad skb!\n");
        return 0;
    }

    ret =  dev_queue_xmit(skb);
	return RX_HANDLER_CONSUMED;
}



static int __init kforward_init(void)
{
    int i;
    
    printk("Loading KFORWARD\n");
    
    for (i = 0; i < NDEVICE; i++) {
        _dev[i] = dev_get_by_name(&init_net, devices[i]);
        if (!_dev[i]) {
            printk("Could not init %s\n",devices[i]);
            continue;
        }
        dev_hold(_dev[i]);
        rtnl_lock();
        dev_set_promiscuity(_dev[i], 1);
     
    	netdev_rx_handler_register(_dev[i], rx_handler, 0);
    
        rtnl_unlock();
    }
    return 0;
}

static void kforward_exit(void)
{
    int i;
    for (i = 0; i < NDEVICE; i++) {
        if (!_dev[i])
            continue;
        printk("Release device %d\n",i);
        rtnl_lock();
        netdev_rx_handler_unregister(_dev[i]);
        rtnl_unlock();
        dev_put(_dev[i]);
    }

    printk("Unloading KFORWARD\n");
    
}


module_init(kforward_init);
module_exit(kforward_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("KForward forwards packet between interfaces");
