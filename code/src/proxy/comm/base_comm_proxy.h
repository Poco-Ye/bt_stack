#ifndef _COMM_PROXY_H_
#define _COMM_PROXY_H_

#define COMM_TYPE 0x01
#define ERR_IRDA_SYS	0xf2	// system not support
#define ERR_IRDA_SEND	0xf3//failed to send REQUEST
#define ERR_IRDA_RECV	0xf4//failed to receive REPLY

#define USB_ERR_DEV_QUERY     19

enum  COM_FUNC{
	F_PORTCLOSE=0,F_PORTOPEN,
	F_PORTSENDS,F_PORTRECV,F_PORTRESET,
	F_PORTPEEP,F_TXPOOLCHECK,F_SET_HEARTBEAT=10,F_OPEN_CHECK
};

#endif



