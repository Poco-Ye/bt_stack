#ifndef OCHI_HOST_PROXY__H
#define OCHI_HOST_PROXY__H

//#include <Base.h>



/* S60/B210 专用的错误码50~70 */
#define OHCI_ERR_IRDA_SYN   -51 /* 座机重启或放置在不同的座机上*/
#define OHCI_ERR_IRDA_INDEX -52/*Mismatched function_no of baseset reply packet*/
#define OHCI_ERR_IRDA_BUF -53/* */
#define OHCI_ERR_IRDA_SEND -54/*fail to send request to baseset*/
#define OHCI_ERR_IRDA_RECV -55/*fail to receive reply from baseset*/
#define OHCI_ERR_IRDA_OFFBASE -56/* off base while receiving*/

#endif

