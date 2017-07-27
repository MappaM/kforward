#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/rtnetlink.h>
#include <linux/kthread.h>
#include "forward.h"

int mode = 0;
module_param(mode, int, S_IRUSR);
MODULE_PARM_DESC(mode, "0 for forward, 1 for count");

void do_transmit(struct net_device* fdev, struct sk_buff* skb) {
    struct netdev_queue *txq;

    txq = netdev_get_tx_queue(fdev, 0);
    if (netif_queue_stopped(fdev) || netif_xmit_frozen_or_stopped(txq)) {
        _stats[skb->dev->ifindex].dropped ++;
        kfree_skb(skb);
        return;
    }

    skb_push(skb, skb->data - skb_mac_header(skb));
    memcpy(skb->data, (unsigned char*)&_dev_header[skb->dev->ifindex], 12);
    skb->dev = fdev;
    if (!skb) {
        printk("[KFORWARD] Bad skb!\n");
        return;
    }

    dev_queue_xmit(skb);
}

rx_handler_result_t
rx_handler(struct sk_buff **pskb)
{

    struct sk_buff *skb = *pskb;
    int ret;
    struct net_device* fdev;
    skb = skb_share_check(skb, GFP_ATOMIC);
    if (!skb) {
        printk("[KFORWARD] Shared skb !?\n");
        return RX_HANDLER_CONSUMED;
    }

    fdev = _dev_fmap[skb->dev->ifindex];

    if (mode == 0)
        do_transmit(fdev, skb);
    else {
        do_count(&_stats[skb->dev->ifindex]);
        consume_skb(skb);
    }

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
    }

    for (i = 0; i < NDEVICE; i++) {
        unsigned ifindex = _dev[i]->ifindex;
        if (ifindex >= 256) {
            printk("[KFORWARD] ifindex out of bound ! Aborting !");
            return 1;
        }
        _dev_fmap[ifindex] = _dev[forward_devices[i]];

        memcpy(&_dev_header[ifindex].dst,&forward_addr[i].addr,6);
        memcpy(&_dev_header[ifindex].src,&get_mac(_dev[forward_devices[i]])->addr,6);

        _stats[ifindex].dropped = 0;
    }


    //Launch devices
    for (i = 0; i < NDEVICE; i++) {
        rtnl_lock();
        dev_set_promiscuity(_dev[i], promisc);
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
        struct kfstats* stats = &_stats[_dev[i]->ifindex];
        print_stats(stats,_dev[i]);

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
