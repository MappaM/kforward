#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/rtnetlink.h>
#include <linux/kthread.h>
#include <linux/filter.h>
#include "forward.h"

struct kfprog {
	struct netlink_ext_ack extack;
	struct net_device* _dev;
	struct bpf_prog* prog;
	u64 count;
};

static struct kfprog _prog[256];

unsigned int xdp_handler(const void* ctx, const struct bpf_insn *insn) {
	struct xdp_buff *xdp = (struct xdp_buff*)ctx;
    struct kfprog *kfprog = (struct kfprog*)(*(void**)insn);
	struct net_device* dev = kfprog->_dev;
    struct kfstats *kfstats = &_stats[dev->ifindex];
    memcpy((void*)xdp->data, (unsigned char*)&_dev_header[dev->ifindex], 12);
    do_count(kfstats);
	return XDP_DROP;
}

static int __init xdpforward_init(void)
{
    int i;

    printk("Loading xdpforward\n");

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
            printk("[xdpforward] ifindex out of bound ! Aborting !");
            return 1;
        }
        _dev_fmap[ifindex] = _dev[forward_devices[i]];

        memcpy(&_dev_header[ifindex].dst,&forward_addr[i].addr,6);
        memcpy(&_dev_header[ifindex].src,&get_mac(_dev[forward_devices[i]])->addr,6);

        _stats[ifindex].dropped = 0;
    }


    //Launch devices
    for (i = 0; i < NDEVICE; i++) {
		struct net_device* dev = _dev[i];
        const struct net_device_ops *ops = dev->netdev_ops;
        struct kfprog* kfprog =  &_prog[dev->ifindex];
        struct bpf_prog *prog;
        xdp_op_t xdp_op;
        int err;
        struct netdev_xdp xdp;

        kfprog->_dev = dev; 
        prog = bpf_prog_alloc(sizeof(struct bpf_prog) + sizeof(void*),0);
        if (!prog)
            return -ENOMEM;
        prog->bpf_func = &xdp_handler;
        *((void**)&prog->insns) = (void*)kfprog;
        rtnl_lock();
        dev_set_promiscuity(dev, promisc);
        xdp_op = ops->ndo_xdp;

	    memset(&xdp, 0, sizeof(xdp));
    	xdp.command = XDP_SETUP_PROG;
	    xdp.extack = &_prog[dev->ifindex].extack;
    	xdp.flags = 0;
	    xdp.prog = prog;

    	err = xdp_op(dev, &xdp);
        if (err) {
            printk("Could not XDP op : %d !\n", err);
        }
        rtnl_unlock();
    }


    return 0;
}

static void xdpforward_exit(void)
{
    int i;

    xdp_op_t xdp_op;
    for (i = 0; i < NDEVICE; i++) {
        int err;
        struct netdev_xdp xdp;
        int64_t diff = 0;
        if (!_dev[i])
            continue;
        const struct net_device_ops *ops = _dev[i]->netdev_ops;

        printk("Release device %d\n",i);
        struct kfstats* stats = &_stats[_dev[i]->ifindex];
        print_stats(stats,_dev[i]);
        rtnl_lock();
        xdp_op = ops->ndo_xdp;

	    memset(&xdp, 0, sizeof(xdp));
    	xdp.command = XDP_SETUP_PROG;
	    xdp.extack = 0;
    	xdp.flags = 0;
	    xdp.prog = 0;

    	err = xdp_op(_dev[i], &xdp);
        if (err) {
            printk("Could not XDP op : %d !\n", err);
        }

//TODO :uninstall
        rtnl_unlock();
        dev_put(_dev[i]);
    }

    printk("Unloading xdpforward\n");
}


module_init(xdpforward_init);
module_exit(xdpforward_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("xdpforward forwards packet between interfaces");
