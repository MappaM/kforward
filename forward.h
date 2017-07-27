#define NDEVICE 2

bool promisc = 0;
module_param(promisc, bool, S_IRUSR);
MODULE_PARM_DESC(promisc, "Enable promiscuity");

char* test = "XDPForward";
module_param(test, charp, S_IRUSR);
MODULE_PARM_DESC(test, "Set test name");

/*
 * Hardcoded parameters
 */

//List of devices to use
char* devices[] = {"ens6f0", "ens6f1", "eth4", "eth5"};

//Forwarding map
int forward_devices[] = {0,1,2,3};

struct hwaddr {
    unsigned char addr[6];
};

//Fordwarding addr
struct hwaddr forward_addr[] = { \
        {0x3c,0xfd,0xfe,0x9e,0x5b,0x61},\
        {0x3c,0xfd,0xfe,0x9e,0x5b,0x60}\
    };

/**
 * Global variables
 */
struct mac_pair {
    unsigned char dst[6];
    unsigned char src[6];
} __attribute__((packed));

struct kfstats {
    uint64_t dropped;
    uint64_t count;
    struct timespec first;
    struct timespec last;
};


static struct net_device* _dev[NDEVICE];
//Map input dev->ifindex to the forwarded device
static struct net_device* _dev_fmap[256];
static struct mac_pair _dev_header[256];
static struct kfstats _stats[256];

static struct netdev_hw_addr* get_mac(struct net_device* dev) {
  	struct netdev_hw_addr *ha;

	rcu_read_lock();
	for_each_dev_addr(dev, ha) {
		return ha;
	}
	rcu_read_unlock();
    return 0;
}


inline void print_stats(struct kfstats* stats, struct net_device* dev) {
        uint64_t count = stats->count;
        int64_t diff = 0;
        diff = (stats->last.tv_sec - stats->first.tv_sec) * 1000000000;
        diff += ((int64_t)stats->last.tv_nsec - (int64_t)stats->first.tv_nsec);
        printk("%s count %llu\n",dev->name,count);
        printk("%s dropped %llu\n",dev->name,_stats[dev->ifindex].dropped);

        printk("%s diff %lld\n",dev->name,diff);
        if (diff > 0)
            printk("[%s] %s RESULT-PPS %lld\n",test,dev->name,count/(diff/1000000000));

}

inline void do_count(struct kfstats* kfstats) {
    if ((kfstats->count & 0xff) == 0) {
        if (kfstats->count == 0)
            kfstats->first = current_kernel_time();
        kfstats->last = current_kernel_time();
    }
	kfstats->count++;
}


