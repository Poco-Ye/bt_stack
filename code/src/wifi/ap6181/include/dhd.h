/*
 * Header file describing the internal (inter-module) DHD interfaces.
 *
 * Provides type definitions and function prototypes used to link the
 * DHD OS, bus, and protocol modules.
 *
 * Copyright (C) 2011, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: dhd.h,v 1.60.4.17 2011-01-09 08:11:56 Exp $
 */

/****************
 * Common types *
 */

#ifndef _dhd_h_
#define _dhd_h_

#ifdef USE_LWIP
#define ENOMEM		12
#define EFAULT      14
#define EINVAL		22
#define EIO			5
#define ETIMEDOUT	110
#define ERESTARTSYS 7
#else
#define ENOMEM		1
#define EFAULT      2
#define EINVAL		3
#define EIO			4
#define ETIMEDOUT	5
#define ERESTARTSYS 6
#endif /* __LWIP_ARCH_H__ */

#define ALL_INTERFACES	0xff
#define DHD_SDALIGN	    32

#include <wlioctl.h>

#if defined(NDIS60) && !defined(UNDER_CE)
#include <wdf.h>
#include <WdfMiniport.h>
#endif /* NDIS60 && ! UNDER_CE */

/* Forward decls */
struct dhd_bus;
struct dhd_prot;
struct dhd_info;
struct dhd_cmn;

/* The level of bus communication with the dongle */
enum dhd_bus_state {
	DHD_BUS_DOWN,		/* Not ready for frame transfers */
	DHD_BUS_LOAD,		/* Download access only (CPU reset) */
	DHD_BUS_DATA		/* Ready for frame transfers */
};

enum dhd_bus_wake_state {
	WAKE_LOCK_OFF,
	WAKE_LOCK_PRIV,
	WAKE_LOCK_DPC,
	WAKE_LOCK_IOCTL,
	WAKE_LOCK_DOWNLOAD,
	WAKE_LOCK_TMOUT,
	WAKE_LOCK_WATCHDOG,
	WAKE_LOCK_LINK_DOWN_TMOUT,
	WAKE_LOCK_PNO_FIND_TMOUT,
	WAKE_LOCK_SOFTAP_SET,
	WAKE_LOCK_SOFTAP_STOP,
	WAKE_LOCK_SOFTAP_START,
	WAKE_LOCK_SOFTAP_THREAD,
	WAKE_LOCK_MAX
};
enum dhd_prealloc_index {
	DHD_PREALLOC_PROT = 0,
	DHD_PREALLOC_RXBUF,
	DHD_PREALLOC_DATABUF,
	DHD_PREALLOC_OSL_BUF
};
/* Common structure for module and instance linkage */
typedef struct dhd_pub {
	/* Linkage ponters */
	osl_t *osh;		/* OSL handle */
	struct dhd_bus *bus;	/* Bus module handle */
	struct dhd_prot *prot;	/* Protocol module handle */
	struct dhd_info  *info; /* Info module handle */
	struct dhd_cmn	*cmn;	/* dhd_common module handle */

	/* Internal dhd items */
	bool up;		/* Driver up/down (to OS) */
	bool txoff;		/* Transmit flow-controlled */
	bool dongle_reset;  /* TRUE = DEVRESET put dongle into reset */
	enum dhd_bus_state busstate;
	uint hdrlen;		/* Total DHD header length (proto + bus) */
	uint maxctl;		/* Max size rxctl request from proto to bus */
	uint rxsz;		/* Rx buffer size bus module should use */
	uint8 wme_dp;	/* wme discard priority */

	/* Dongle media info */
	bool iswl;		/* Dongle-resident driver is wl */
	ulong drv_version;	/* Version of dongle-resident driver */
	struct ether_addr mac;	/* MAC address obtained from dongle */
	dngl_stats_t dstats;	/* Stats for dongle-based data */

	/* Additional stats for the bus level */
	ulong tx_packets;	/* Data packets sent to dongle */
	ulong tx_multicast;	/* Multicast data packets sent to dongle */
	ulong tx_errors;	/* Errors in sending data to dongle */
	ulong tx_ctlpkts;	/* Control packets sent to dongle */
	ulong tx_ctlerrs;	/* Errors sending control frames to dongle */
	ulong rx_packets;	/* Packets sent up the network interface */
	ulong rx_multicast;	/* Multicast packets sent up the network interface */
	ulong rx_errors;	/* Errors processing rx data packets */
	ulong rx_ctlpkts;	/* Control frames processed from dongle */
	ulong rx_ctlerrs;	/* Errors in processing rx control frames */
	ulong rx_dropped;	/* Packets dropped locally (no memory) */
	ulong rx_flushed;  /* Packets flushed due to unscheduled sendup thread */
	ulong wd_dpc_sched;   /* Number of times dhd dpc scheduled by watchdog timer */

	ulong rx_readahead_cnt;	/* Number of packets where header read-ahead was used. */
	ulong tx_realloc;	/* Number of tx packets we had to realloc for headroom */
	ulong fc_packets;       /* Number of flow control pkts recvd */

	/* Last error return */
	int bcmerror;
	uint tickcnt;

	/* Last error from dongle */
	int dongle_error;

	/* Suspend disable flag and "in suspend" flag */
	int suspend_disable_flag; /* "1" to disable all extra powersaving during suspend */
	int in_suspend;			/* flag set to 1 when early suspend called */
	int dtim_skip;         /* dtim skip , default 0 means wake each dtim */

	/* Pkt filter defination */
	char * pktfilter[100];
	int pktfilter_count;

	wl_country_t dhd_cspec;		/* Current Locale info */
	char eventmask[WL_EVENTING_MASK_LEN];
	bool	dongle_isolation;
} dhd_pub_t;

typedef struct dhd_cmn {
	osl_t *osh;		/* OSL handle */
	dhd_pub_t *dhd;
} dhd_cmn_t;

#define SPINWAIT_SLEEP(a, exp, us)  do { \
    uint countdown = (us) + 9; \
    while ((exp) && (countdown >= 10)) { \
        OSL_DELAY(10);  \
        countdown -= 10;  \
    } \
} while (0)

typedef struct dhd_if_event {
	uint8 ifidx;
	uint8 action;
	uint8 flags;
	uint8 bssidx;
	uint8 is_AP;
} dhd_if_event_t;

typedef enum dhd_attach_states
{
	DHD_ATTACH_STATE_INIT = 0x0,
	DHD_ATTACH_STATE_NET_ALLOC = 0x1,
	DHD_ATTACH_STATE_DHD_ALLOC = 0x2,
	DHD_ATTACH_STATE_ADD_IF = 0x4,
	DHD_ATTACH_STATE_PROT_ATTACH = 0x8,
	DHD_ATTACH_STATE_WL_ATTACH = 0x10,
	DHD_ATTACH_STATE_THREADS_CREATED = 0x20,
	DHD_ATTACH_STATE_WAKELOCKS_INIT = 0x40,
	DHD_ATTACH_STATE_CFG80211 = 0x80,
	DHD_ATTACH_STATE_EARLYSUSPEND_DONE = 0x100,
	DHD_ATTACH_STATE_DONE = 0x200
} dhd_attach_states_t;

/* Value -1 means we are unsuccessful in creating the kthread. */
#define DHD_PID_KT_INVALID 	-1
/* Value -2 means we are unsuccessful in both creating the kthread and tasklet */
#define DHD_PID_KT_TL_INVALID	-2

/*
 * Exported from dhd OS modules (dhd_linux/dhd_ndis)
 */

/* Indication from bus module regarding presence/insertion of dongle.
 * Return dhd_pub_t pointer, used as handle to OS module in later calls.
 * Returned structure should have bus and prot pointers filled in.
 * bus_hdrlen specifies required headroom for bus module header.
 */
extern dhd_pub_t *dhd_attach(osl_t *osh, struct dhd_bus *bus, uint bus_hdrlen);
extern int dhd_net_attach(dhd_pub_t *dhdp, int idx);

/* Indication from bus module regarding removal/absence of dongle */
extern void dhd_detach(dhd_pub_t *dhdp);

extern bool dhd_prec_enq(dhd_pub_t *dhdp, struct pktq *q, void *pkt, int prec);

/* Receive frame for delivery to OS.  Callee disposes of rxp. */
extern void dhd_rx_frame(dhd_pub_t *dhdp, int ifidx, void *rxp, int numpkt, uint8 chan);

/* Notify tx completion */
extern void dhd_txcomplete(dhd_pub_t *dhdp, void *txp);

/* OS independent layer functions */
extern int dhd_os_proto_block(dhd_pub_t * pub);
extern int dhd_os_proto_unblock(dhd_pub_t * pub);
extern int dhd_os_ioctl_resp_wait(dhd_pub_t * pub);
extern int dhd_os_ioctl_resp_wake(dhd_pub_t * pub);
extern unsigned int dhd_os_get_ioctl_resp_timeout(void);
extern void dhd_os_set_ioctl_resp_timeout(unsigned int timeout_msec);
extern int dhd_os_sdlock(dhd_pub_t * pub);
extern void dhd_os_sdunlock(dhd_pub_t * pub);
extern void dhd_os_sdlock_txq(dhd_pub_t * pub);
extern void dhd_os_sdunlock_txq(dhd_pub_t * pub);
extern void dhd_os_sdlock_sndup_rxq(dhd_pub_t * pub);
extern void dhd_customer_gpio_wlan_ctrl(int onoff);
extern int	   dhd_custom_get_mac_address(unsigned char *buf);
extern void dhd_os_sdunlock_sndup_rxq(dhd_pub_t * pub);
extern void dhd_os_sdlock_eventq(dhd_pub_t * pub);
extern void dhd_os_sdunlock_eventq(dhd_pub_t * pub);

#if defined(DHDTHREAD)
struct task_struct;
struct sched_param;
int setScheduler(struct task_struct *p, int policy, struct sched_param *param);
#endif /* DHDTHREAD && DHD_GPL */

typedef struct {
	uint32 limit;		/* Expiration time (usec) */
	uint32 increment;	/* Current expiration increment (usec) */
	uint32 elapsed;		/* Current elapsed time (usec) */
	uint32 tick;		/* O/S tick time (usec) */
} dhd_timeout_t;

extern void dhd_timeout_start(dhd_timeout_t *tmo, uint usec);
extern int dhd_timeout_expired(dhd_timeout_t *tmo);
extern struct net_device * dhd_idx2net(struct dhd_pub *dhd_pub, int ifidx);
extern int wl_host_event(dhd_pub_t *dhd_pub, int *idx, void *pktdata, wl_event_msg_t *, void **data_ptr);
extern void wl_event_to_host_order(wl_event_msg_t * evt);

extern int dhd_wl_ioctl(dhd_pub_t *dhd_pub, int ifindex, wl_ioctl_t *ioc, void *buf, int len);
extern int dhd_wl_ioctl_cmd(dhd_pub_t *dhd_pub, int cmd, void *arg, int len, uint8 set,
                            int ifindex);

extern struct dhd_cmn *dhd_common_init(osl_t *osh);
extern void dhd_common_deinit(dhd_pub_t *dhd_pub, dhd_cmn_t *sa_cmn);

/* Send packet to dongle via data channel */
extern int dhd_sendpkt(dhd_pub_t *dhdp, int ifidx, void *pkt);
extern uint dhd_bus_status(dhd_pub_t *dhdp);

typedef enum cust_gpio_modes {
	WLAN_RESET_ON,
	WLAN_RESET_OFF,
	WLAN_POWER_ON,
	WLAN_POWER_OFF
} cust_gpio_modes_t;

extern uint dhd_intr; /* Use interrupts */
extern uint dhd_poll; /* Use polling */

#define DHD_IDLETIME_TICKS 1

/* Override to force tx queueing all the time */
extern uint dhd_force_tx_queueing;
/* Default KEEP_ALIVE Period is 55 sec to prevent AP from sending Keep Alive probe frame */
#define KEEP_ALIVE_PERIOD 55000
#define NULL_PKT_STR	"null_pkt"

/* optionally set by a module_param_string() */
#define MOD_PARAM_PATHLEN	2048

/* For supporting multiple interfaces */
#define DHD_MAX_IFS	16
#define DHD_DEL_IF	-0xe
#define DHD_BAD_IF	-0xf

/* Please be mindful that total pkttag space is 32 octets only */
typedef struct dhd_pkttag {
	/*
	b[11 ] - 1 = this packet was sent in response to one time packet request,
	do not increment credit on status for this one. [WLFC_CTL_TYPE_MAC_REQUEST_PACKET].
	b[10 ] - 1 = signal-only-packet to firmware [i.e. nothing to piggyback on]
	b[9  ] - 1 = packet is host->firmware (transmit direction)
	       - 0 = packet received from firmware (firmware->host)
	b[8  ] - 1 = packet was sent due to credit_request (pspoll),
	             packet does not count against FIFO credit.
	       - 0 = normal transaction, packet counts against FIFO credit
	b[7  ] - 1 = AP, 0 = STA
	b[6:4] - AC FIFO number
	b[3:0] - interface index
	*/
	uint16	if_flags;
	/* destination MAC address for this packet so that not every
	module needs to open the packet to find this
	*/
	uint8	dstn_ether[ETHER_ADDR_LEN];
	/*
	This 32-bit goes from host to device for every packet.
	*/
	uint32	htod_tag;
	/* bus specific stuff */
	union {
		struct {
			void* stuff;
			uint32 thing1;
			uint32 thing2;
		} sd;
		struct {
			void* bus;
			void* urb;
		} usb;
	} bus_specific;
} dhd_pkttag_t;

#define DHD_PKTTAG_SET_H2DTAG(tag, h2dvalue)	((dhd_pkttag_t*)(tag))->htod_tag = (h2dvalue)
#define DHD_PKTTAG_H2DTAG(tag)					(((dhd_pkttag_t*)(tag))->htod_tag)

#define DHD_PKTTAG_IFMASK		0xf
#define DHD_PKTTAG_IFTYPE_MASK	0x1
#define DHD_PKTTAG_IFTYPE_SHIFT	7
#define DHD_PKTTAG_FIFO_MASK	0x7
#define DHD_PKTTAG_FIFO_SHIFT	4

#define DHD_PKTTAG_SIGNALONLY_MASK			0x1
#define DHD_PKTTAG_SIGNALONLY_SHIFT			10

#define DHD_PKTTAG_ONETIMEPKTRQST_MASK		0x1
#define DHD_PKTTAG_ONETIMEPKTRQST_SHIFT		11

#define DHD_PKTTAG_PKTDIR_MASK			0x1
#define DHD_PKTTAG_PKTDIR_SHIFT			9

#define DHD_PKTTAG_CREDITCHECK_MASK		0x1
#define DHD_PKTTAG_CREDITCHECK_SHIFT	8

#define DHD_PKTTAG_INVALID_FIFOID 0x7

#define DHD_PKTTAG_SETFIFO(tag, fifo)	((dhd_pkttag_t*)(tag))->if_flags = \
	(((dhd_pkttag_t*)(tag))->if_flags & ~(DHD_PKTTAG_FIFO_MASK << DHD_PKTTAG_FIFO_SHIFT)) | \
	(((fifo) & DHD_PKTTAG_FIFO_MASK) << DHD_PKTTAG_FIFO_SHIFT)
#define DHD_PKTTAG_FIFO(tag)			((((dhd_pkttag_t*)(tag))->if_flags >> \
	DHD_PKTTAG_FIFO_SHIFT) & DHD_PKTTAG_FIFO_MASK)

#define DHD_PKTTAG_SETIF(tag, if)	((dhd_pkttag_t*)(tag))->if_flags = \
	(((dhd_pkttag_t*)(tag))->if_flags & ~DHD_PKTTAG_IFMASK) | ((if) & DHD_PKTTAG_IFMASK)
#define DHD_PKTTAG_IF(tag)	(((dhd_pkttag_t*)(tag))->if_flags & DHD_PKTTAG_IFMASK)

#define DHD_PKTTAG_SETIFTYPE(tag, isAP)	((dhd_pkttag_t*)(tag))->if_flags = \
	(((dhd_pkttag_t*)(tag))->if_flags & \
	~(DHD_PKTTAG_IFTYPE_MASK << DHD_PKTTAG_IFTYPE_SHIFT)) | \
	(((isAP) & DHD_PKTTAG_IFTYPE_MASK) << DHD_PKTTAG_IFTYPE_SHIFT)
#define DHD_PKTTAG_IFTYPE(tag)	((((dhd_pkttag_t*)(tag))->if_flags >> \
	DHD_PKTTAG_IFTYPE_SHIFT) & DHD_PKTTAG_IFTYPE_MASK)

#define DHD_PKTTAG_SETCREDITCHECK(tag, check)	((dhd_pkttag_t*)(tag))->if_flags = \
	(((dhd_pkttag_t*)(tag))->if_flags & \
	~(DHD_PKTTAG_CREDITCHECK_MASK << DHD_PKTTAG_CREDITCHECK_SHIFT)) | \
	(((check) & DHD_PKTTAG_CREDITCHECK_MASK) << DHD_PKTTAG_CREDITCHECK_SHIFT)
#define DHD_PKTTAG_CREDITCHECK(tag)	((((dhd_pkttag_t*)(tag))->if_flags >> \
	DHD_PKTTAG_CREDITCHECK_SHIFT) & DHD_PKTTAG_CREDITCHECK_MASK)

#define DHD_PKTTAG_SETPKTDIR(tag, dir)	((dhd_pkttag_t*)(tag))->if_flags = \
	(((dhd_pkttag_t*)(tag))->if_flags & \
	~(DHD_PKTTAG_PKTDIR_MASK << DHD_PKTTAG_PKTDIR_SHIFT)) | \
	(((dir) & DHD_PKTTAG_PKTDIR_MASK) << DHD_PKTTAG_PKTDIR_SHIFT)
#define DHD_PKTTAG_PKTDIR(tag)	((((dhd_pkttag_t*)(tag))->if_flags >> \
	DHD_PKTTAG_PKTDIR_SHIFT) & DHD_PKTTAG_PKTDIR_MASK)

#define DHD_PKTTAG_SETSIGNALONLY(tag, signalonly)	((dhd_pkttag_t*)(tag))->if_flags = \
	(((dhd_pkttag_t*)(tag))->if_flags & \
	~(DHD_PKTTAG_SIGNALONLY_MASK << DHD_PKTTAG_SIGNALONLY_SHIFT)) | \
	(((signalonly) & DHD_PKTTAG_SIGNALONLY_MASK) << DHD_PKTTAG_SIGNALONLY_SHIFT)
#define DHD_PKTTAG_SIGNALONLY(tag)	((((dhd_pkttag_t*)(tag))->if_flags >> \
	DHD_PKTTAG_SIGNALONLY_SHIFT) & DHD_PKTTAG_SIGNALONLY_MASK)

#define DHD_PKTTAG_SETONETIMEPKTRQST(tag)	((dhd_pkttag_t*)(tag))->if_flags = \
	(((dhd_pkttag_t*)(tag))->if_flags & \
	~(DHD_PKTTAG_ONETIMEPKTRQST_MASK << DHD_PKTTAG_ONETIMEPKTRQST_SHIFT)) | \
	(1 << DHD_PKTTAG_ONETIMEPKTRQST_SHIFT)
#define DHD_PKTTAG_ONETIMEPKTRQST(tag)	((((dhd_pkttag_t*)(tag))->if_flags >> \
	DHD_PKTTAG_ONETIMEPKTRQST_SHIFT) & DHD_PKTTAG_ONETIMEPKTRQST_MASK)

#define DHD_PKTTAG_SETDSTN(tag, dstn_MAC_ea)	memcpy(((dhd_pkttag_t*)((tag)))->dstn_ether, \
	(dstn_MAC_ea), ETHER_ADDR_LEN)
#define DHD_PKTTAG_DSTN(tag)	((dhd_pkttag_t*)(tag))->dstn_ether

typedef int (*f_commitpkt_t)(void* ctx, void* p);
int dhd_wlfc_enable(dhd_pub_t *dhd);
int dhd_wlfc_interface_event(struct dhd_info *, uint8 action, uint8 ifid, uint8 iftype, uint8* ea);
int dhd_wlfc_FIFOcreditmap_event(struct dhd_info *dhd, uint8* event_data);
int dhd_wlfc_event(struct dhd_info *dhd);

#define DHD_WLFC_CTRINC_MAC_CLOSE(entry)	do {} while (0)
#define DHD_WLFC_CTRINC_MAC_OPEN(entry)		do {} while (0)

extern void dhd_wait_for_event(dhd_pub_t *dhd);
extern void dhd_wait_event_wakeup(dhd_pub_t*dhd);

#ifdef ARP_OFFLOAD_SUPPORT
/* dhd_commn arp offload wrapers */
void dhd_aoe_hostip_clr(dhd_pub_t *dhd);
void dhd_aoe_arp_clr(dhd_pub_t *dhd);
int dhd_arp_get_arp_hostip_table(dhd_pub_t *dhd, void *buf, int buflen);
void dhd_arp_offload_add_ip(dhd_pub_t *dhd, uint32 ipaddr);
#endif /* ARP_OFFLOAD_SUPPORT */
#endif /* _dhd_h_ */
