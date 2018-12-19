/*
**   Inet Configure File
**Author: sunJH
**Date: 2007-08-13
修改历史：
080624 sunJH
1.增加TCP_RCV_QUEUELEN、TCP_SND_QUEUELEN宏定义
2.取消MAX_1024_COUNT定义
3.MAX_512_COUNT由200个变为90个（通过宏定义）
4.MAX_1518_COUNT由50个变为120个
5.报文全部占用的内存为222KB
**/
#ifndef _INET_CONFIG_H_
#define _INET_CONFIG_H_

/*
** 协议栈连接配置参数
**
**/

/* TCP Receiver buffer space
*/
#ifndef TCP_RCV_QUEUELEN
#define TCP_RCV_QUEUELEN 10
#endif
	
/* TCP sender buffer space (pbufs). This must be at least = 2 *
   TCP_SND_BUF/TCP_MSS for things to work. */
#ifndef TCP_SND_QUEUELEN
#define TCP_SND_QUEUELEN                10
#endif

/*
 TCP Max connect control block
*/
#ifndef MAX_TCP_PCB_COUNT
#define MAX_TCP_PCB_COUNT 10
#endif

/*
TCP Max Listen control block
*/
#ifndef MAX_TCP_LISTEN_PCB_COUNT
#define MAX_TCP_LISTEN_PCB_COUNT 2
#endif

/*
UDP MAX control block
*/
#ifndef MAX_UDP_COUNT
#define MAX_UDP_COUNT 2
#endif

/*
Socket MAX control block
*/
#ifndef MAX_SOCK_COUNT
#define MAX_SOCK_COUNT (MAX_TCP_PCB_COUNT+MAX_TCP_LISTEN_PCB_COUNT+MAX_UDP_COUNT)
#endif

/*
** Skbuff 数目配置参数
**
**/
#define MAX_512_COUNT  (TCP_RCV_QUEUELEN+TCP_SND_QUEUELEN+1)*MAX_TCP_PCB_COUNT /* packet size = 512 */
#define MAX_1518_COUNT (TCP_RCV_QUEUELEN+TCP_SND_QUEUELEN+4)*MAX_TCP_PCB_COUNT /* packet size = 1518 */

#endif/* _INET_CONFIG_H_ */
