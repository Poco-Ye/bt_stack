/*
修改历史:
080624 sunJH
inet_addr增加字符串长度检查
*/
#include <inet/inet.h>
#include <inet/dev.h>

#define IP_ADDR_ANY_VALUE 0x00000000UL
#define IP_ADDR_BROADCAST_VALUE 0xffffffffUL

/* used by IP_ADDR_ANY and IP_ADDR_BROADCAST in ip_addr.h */
const struct ip_addr ip_addr_any = { IP_ADDR_ANY_VALUE };
const struct ip_addr ip_addr_broadcast = { IP_ADDR_BROADCAST_VALUE };
int myisdigit(char a)
{
	if(a>='0'&&a<='9')
		return 1;
	return 0;
}
int inet_aton(const char *cp, long *inp)
{
	unsigned long addr;
	int value;
	int part;

	if (!inp)
		return 0;
  
	addr = 0;
	for (part=1;part<=4;part++) 
	{
		if (!myisdigit(*cp))
			return 0;
        
		value = 0;
		while (myisdigit(*cp)) 
		{
			value *= 10;
			value += *cp++ - '0';
			if (value > 255)
				return 0;
		}
    
		if (*cp++ != ((part == 4) ? '\0' : '.'))
			return 0;
    
		addr <<= 8;
		addr |= value;
	}
  
	*inp = htonl(addr);

	return 1;
}

unsigned long inet_addr(const char *cp)
{
	long a;
	if(str_check_max(cp, MAX_IPV4_ADDR_STR_LEN)!=NET_OK)
		return -1;
	if (!inet_aton(cp, &a))
		return -1;
	else
		return a;
}

char *inet_ntoa2(long addr/*net order */, char *buf/*out*/)
{
	static char t[20];
	u8 *p = (u8*)&addr;
	if(!buf)
		buf = t;
	sprintf(buf,"%d.%d.%d.%d",
		p[0],
		p[1],
		p[2],
		p[3]
		);
	return buf;
}

char *inet_ntoa(long addr/*net order */)
{
	return inet_ntoa2(addr, NULL);
}

