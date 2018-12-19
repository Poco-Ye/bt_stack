#include <inet/inet.h>
#include <inet/dev.h>
#include <inet/inet_softirq.h>
#include "wnet_ppp.h"
#ifndef P_WNET // comm.h
#define P_WNET 2
#endif
#ifdef NET_DEBUG
#define wnet_debug  printk
#else
#define wnet_debug
#endif
#define CSQ_RSP_PTR "+CSQ:"
#define REG_RSP_PTR "+CGREG:"
#define ERR_RSP_PTR "+CME ERROR"
#define CGATT_RSP_PTR "+CGATT:" 
#define WNET_BREAK_PTR "NO CARRIER"
#define WNET_H3x0_MODE "+COPS:"
#define WNET_Mux09_MODE "^MONSC:"
#define WNET_H330_MUCELL "+MUCELL:"
#define ATD_TIMEOUT  2000
static char apn_buffer[122];
static char atd_buffer[103];
unsigned char cell_cmd[32] = {0};
unsigned char cell_rsp[32] = {0};
static char pppcfg_buff[250] = {0};
static int flag_cgatt = 0; 
static WNET_DEV_T   wnet_dev={0, };
static struct inet_softirq_s  wnet_ppp_timer;/* PPP Timer */
static struct inet_softirq_s  wnet_timer;/* WNet Timer */
extern volatile int task_lbs_flag;
char PDP_CFG[256] = {0};
void wnet_switch_state(WNET_DEV_T *wnet, WNET_STATE next_state) ;

unsigned char wnet_cell_buf[2048]={0,};
unsigned char wnet_ncell_buf[2048]={0,};
unsigned char wnet_mode_buf[2048]={0,};

static void cmd_rsp_print(char *buf, int count)
{
#ifdef NET_DEBUG
	int i, hex_print;
        hex_print=0;
	for(i=0; i<count; i++)
	{
		if(buf[i]>=0x20&&buf[i]<=0x7E)
		{
			if(buf[i]=='\\')/*display such as \\ */				
				wnet_debug("%C%C",buf[i],buf[i]);
                        else
				wnet_debug("%c", buf[i]);
		}else//display such as \01			
			wnet_debug("\\%02X", (u8)buf[i]);
	}
	if(hex_print)
	{
		wnet_debug("\n[HEX]:\n");
		for(i=0; i<count; i++)
		{
			wnet_debug("%02X,", (u8)buf[i]);
			if(((i+1)%16)==0)	wnet_debug("\n");
		}
		
	}
#endif//#ifdef NET_DEBUG
}
#define wnet_send_cmd(cmd) do{ \
	wnet_debug("Send AT Cmd: "); \
	cmd_rsp_print(cmd, strlen(cmd));\
	wnet_debug("\n");\
	PortReset((u8)(wnet_dev.port));\
	PortTxs((u8)(wnet_dev.port),(unsigned char *)cmd, (u16)strlen((char *)cmd)); \
}while(0)	
static unsigned char *wnet_find_str(unsigned char *str,const unsigned char *fnd,int fl)
{
	int sl,kk,ii;
	sl=strlen((char *)fnd);
	if(sl>fl) return NULL;
	kk=fl-sl;
	for(ii=0;ii<=kk;ii++)
	{
		if(*str==*fnd)
		{
			if(memcmp(str,fnd,sl)==0) return str;
		}
		str++;
	}
	return NULL;
}
/*the response must be ended with \r\n*/
static u8 *wnet_find_rsp(u8 *rsp, const u8 *key, int rsp_len)
{
	int end;
	u8 *nxt;
	u8 *fstr = wnet_find_str(rsp, key, rsp_len);
	if(fstr==NULL)	return NULL;
	nxt = fstr+strlen(key);
	end = 0;
	while(nxt<(rsp+rsp_len))
	{
		if(*nxt==0xD||*nxt==0xA)
		{
			end = 1;
			break;
		}
		nxt++;
	}
	if(end==0)  return NULL;
	return fstr;
}

int wnet_get_rsp(char *buf, int size)
{
	u8 c;
	int count = 0;
	while(1)
	{
		if(PortRx((u8)(wnet_dev.port), &c)!=0) break;        
		if(count == size)   goto out;
		buf[count++] = c;
		if(c==0xA||c==0xD)
		{
			if(c==0xA)  goto out;
		}
	}
out:
	if(count>0)
	{
		wnet_debug("RSP[%d]:", count);
		cmd_rsp_print(buf, count);
		wnet_debug("\n");
	}
	return count;
}

char* wnet_state_str(WNET_STATE state)
{
	static char *name[]=
	{
		"WNET_STATE_DOWN",
		"WNET_STATE_PRE_DIALING",
		"WNET_STATE_DIALING",
		"WNET_STATE_UP",
		"WNET_STATE_PPP",
		"WNET_STATE_PRE_ONHOOK",
		"WNET_STATE_ONHOOK",
	};
	if(state<0||state>=WNET_STATE_MAX)  return "WNET_STATE_N/A";
	return name[state];
}
static int wnet_check_reg(WNET_DEV_T *wnet, char *token)
{
	char *fss;
	int status;
	fss=wnet_find_str(wnet->rsp_buf, token, wnet->rall);
	if(fss) {
		fss+=strlen(token);
		fss=wnet_find_str(fss,",",6);
		if(fss) {
			fss++;
			status= atoi(fss);
			if(status==1||status==5)    return NET_OK;
			else if (status==3) return WL_RET_ERR_SOFT_RESET;
			else    return 1;
		}
	}	
	return WL_RET_ERR_NOREG;
}
static int h3x0_check_mode(WNET_DEV_T *wnet,char *token)
{
	u8 *fstr;
	fstr=wnet_find_str(wnet->rsp_buf, token, wnet->rall);
	fstr= strstr(fstr, token);
	wnet_debug("fstr :%s\n",fstr);
	if(fstr == NULL)
		return 1;
	fstr = strstr(fstr,"\r\n");
	if(fstr ==NULL || *(fstr -1) >'9' || *(fstr -1) < '0')
		return 1;
	wnet->AcT =  *(fstr -1) - '0';
	wnet_debug("wnet->AcT = %d\n",wnet->AcT);
	return 0;
}

static int Mux09_check_mode(WNET_DEV_T *wnet,char *token)
{
	u8 *fstr;
	u8 *ptr,*ptr1;
	u8 mode[16]={0,};
	u8 len;
	fstr=wnet_find_str(wnet->rsp_buf, token, wnet->rall);
	if(fstr == NULL)
		return -1;
	ptr= strstr(fstr, token);
	wnet_debug("fstr :%s\n",ptr);
	ptr = ptr + strlen(token);
	while(' ' == *ptr) ptr++;
	ptr1 = strstr(ptr,",");
	len = ptr1 - ptr;
	memcpy(mode,ptr,len);
	if(memcmp(mode,"WCDMA",strlen("WCDMA")) == 0)
		return 2;
	else if(memcmp(mode,"GSM",strlen("GSM")) == 0)
		return 1;
	else
		return -1;
}

static int Mc509_check_mode(WNET_DEV_T *wnet,char *token)
{
	u8 *fstr;
	fstr=wnet_find_str(wnet->rsp_buf, token, wnet->rall);
	fstr= strstr(fstr, token);
	wnet_debug("fstr :%s\n",fstr);
	if(fstr == NULL)
		return 1;
	else
		memcpy(wnet_cell_buf,wnet->rsp_buf,sizeof(wnet->rsp_buf));
	return 0;	
}

static int wnet_set_cgatt(WNET_DEV_T *wnet)
{
	char *fss;
	int  i, status;
	fss=wnet_find_str(wnet->rsp_buf, CGATT_RSP_PTR, wnet->rall);
	if(fss){
		fss += strlen(CGATT_RSP_PTR);
		status = atoi(fss);
		wnet_debug("CGATT Status: %d\n", status);
		for(i=wnet->cgatt_do_start; i<=wnet->cgatt_do_end; i++)
		{
			if(i>=wnet->dialing_cmd_rsp.num)    break;
			if(status == 0)/* We should exec: AT+CGATT=1\r */				
				wnet->dialing_cmd_rsp.cmd_rsp[i].flag &= ~CMD_UNDO;
			else/* We should not exec: AT+CGATT=1\r */				
				wnet->dialing_cmd_rsp.cmd_rsp[i].flag |= CMD_UNDO;
		}
	}
	return NET_OK;
}
static void wnet_signal_err(WNET_DEV_T *wnet, int err)
{
	wnet->signal.err = err;
	wnet->signal.level = 0;
	wnet->signal.ber = 0;
	wnet->signal.nexttime = inet_jiffier+SIGNAL_UPDATE_TIME;
}
static int wnet_set_signal(WNET_DEV_T *wnet)
{
	char *fss;
	int  err =0;
	fss=wnet_find_str(wnet->rsp_buf, CSQ_RSP_PTR, wnet->rall);
	if(fss){
		fss+=strlen(CSQ_RSP_PTR);
		wnet->signal.level = (unsigned char )atoi(fss);
		fss=wnet_find_str(fss,",",6);
		if(fss){
			fss++;
			wnet->signal.ber=(unsigned char )atoi(fss);
			wnet->signal.nexttime = inet_jiffier+SIGNAL_UPDATE_TIME;
			wnet->signal.err = 0;
			wnet_debug("GetSignal OK %d,%d\n",wnet->signal.level,wnet->signal.ber);
                        if(wnet->signal.level > 0 && wnet->signal.level < 32)   return NET_OK;
		}else
			err = WL_RET_ERR_RSPERR;
	}else
		err = WL_RET_ERR_NORSP;
	if(err<0)   wnet_signal_err(wnet, err);
        return 1;
}

static int wnet_set_scellinfo(WNET_DEV_T *wnet)
{
	char *fss;
	int ret;
	//wnet_debug("wnet->rsp_buf:%d\n",wnet->rsp_buf);
	fss = wnet_find_str(wnet->rsp_buf, cell_rsp, wnet->rall);
	if(fss){
		memcpy(wnet_cell_buf,wnet->rsp_buf,sizeof(wnet->rsp_buf));
		return 0;
	}
	return 1;
}

static int wnet_set_ncellinfo(WNET_DEV_T *wnet)
{
	char *fss;
	int ret;
	fss = wnet_find_str(wnet->rsp_buf, cell_rsp, wnet->rall);
	if(fss){
		memcpy(wnet_ncell_buf,wnet->rsp_buf,strlen(wnet->rsp_buf));
		return 0;
	}
	return 1;
}

static void wnet_breakdown(WNET_DEV_T *wnet)
{
	wnet_switch_state(wnet, WNET_STATE_PRE_ONHOOK);
	wnet_switch_state(wnet, WNET_STATE_ONHOOK);
	wnet_switch_state(wnet, WNET_STATE_DOWN);
	wnet->err_code = WL_RET_ERR_LINEOFF;
	ppp_logout(&wnet->ppp_dev, WL_RET_ERR_LINEOFF);
}
static void wnet_ppp_timeout(unsigned int mask)
{
	WNET_DEV_T *wnet = &wnet_dev;	
	ppp_timeout(&wnet->ppp_dev, mask);
}
static void wnet_jump_cmd(WNET_DEV_T *wnet, WNET_CMD_RSP_ARR_T *cmd_rsp_arr)
{
	int i=cmd_rsp_arr->idx;
	for(; i<cmd_rsp_arr->num; i++)
		if(cmd_rsp_arr->cmd_rsp[i].flag&CMD_UNDO) 
			continue;
		else
			break;
	cmd_rsp_arr->idx = i;
}
static void wnet_init_cmd(WNET_DEV_T *wnet, WNET_CMD_RSP_ARR_T *cmd_rsp_arr)
{
	memset(wnet->rsp_buf, 0, sizeof(wnet->rsp_buf));
	wnet->rall = 0;
	cmd_rsp_arr->idx = 0;
	cmd_rsp_arr->state = CMD_STATE_INIT;
	cmd_rsp_arr->retry = 0;
	cmd_rsp_arr->flag = 0;   
	wnet_jump_cmd(wnet, cmd_rsp_arr);
	wnet->cmd_jiffier = inet_jiffier;
}
static void wnet_next_cmd(WNET_DEV_T *wnet, WNET_CMD_RSP_ARR_T *cmd_rsp_arr)
{
	#ifdef NET_DEBUG
	{
		char *cmd = cmd_rsp_arr->cmd_rsp[cmd_rsp_arr->idx].cmd;
		wnet_debug("Exec cmd[delta=%d]: ", inet_jiffier-wnet->cmd_jiffier);
		cmd_rsp_print(cmd , strlen(cmd));
		wnet_debug("\n");
		wnet->cmd_jiffier = inet_jiffier;
	}
	#endif
	cmd_rsp_arr->state = CMD_STATE_INIT;
	cmd_rsp_arr->retry = 0;
	cmd_rsp_arr->idx++;
	wnet->rall = 0;
	wnet_jump_cmd(wnet, cmd_rsp_arr);
}

static int wnet_exec_cmd(WNET_DEV_T *wnet, WNET_CMD_RSP_ARR_T *cmd_rsp_arr)
{
	PPP_DEV_T *ppp = &wnet->ppp_dev;
	WNET_CMD_RSP_T *cmd_rsp;
	int ret;

	if(cmd_rsp_arr->idx >= cmd_rsp_arr->num||cmd_rsp_arr->idx< 0)
		return NET_ERR_BUF;
        else
		cmd_rsp = cmd_rsp_arr->cmd_rsp+cmd_rsp_arr->idx;

	if(cmd_rsp->flag == CMD_CELL_INFO && LBSTaskGet() == 0)
		return 0;
retry:	
	if(cmd_rsp_arr->state == CMD_STATE_INIT)
	{
		if(cmd_rsp->flag&CMD_TOATMODE)
		{			
			if(ppp->last_tx_timestamp+PRE_TOATMODE_WAIT>inet_jiffier)
			{
				wnet_debug("cmd:%s Waiting [%d];",cmd_rsp->cmd,ppp->last_tx_timestamp+PRE_TOATMODE_WAIT-inet_jiffier);
				return 1;
			}
		}		
		wnet_send_cmd(cmd_rsp->cmd);
		cmd_rsp_arr->state = CMD_STATE_WAIT;
		cmd_rsp_arr->timestamp = inet_jiffier;
	}
        else{ 
		if(cmd_rsp_arr->timestamp+cmd_rsp->timeout < inet_jiffier)
		{			
			if(cmd_rsp_arr->retry >= cmd_rsp->retry)//NO Respones
			{
				if(cmd_rsp->rsp == NULL) //this cmd not require response
				{
					wnet_debug("[%d/%d]CMD(Ignore RSP):\n\t", cmd_rsp_arr->idx, cmd_rsp_arr->num);
					cmd_rsp_print(cmd_rsp->cmd, strlen(cmd_rsp->cmd));
					wnet_debug("\nRSP:\n\t");
					cmd_rsp_print(wnet->rsp_buf, wnet->rall);
					wnet_debug("\n");
					return 0;
				}
				wnet_debug("[%d/%d]Exec CMD Timeout[%s]:",cmd_rsp_arr->idx, cmd_rsp_arr->num,cmd_rsp->rsp);
				cmd_rsp_print(cmd_rsp->cmd, strlen(cmd_rsp->cmd));
				wnet_debug("\n");
				if(LBSTaskGet() == 1){
					if(cmd_rsp->flag == CMD_CELL_INFO){
						wnet_set_scellinfo(wnet);
						return 0;
					}
					if(cmd_rsp->flag == CMD_CHK_MODE){
						memcpy(wnet_cell_buf,wnet->rsp_buf,sizeof(wnet->rsp_buf));
						return 0;
					}
				} 
				if(cmd_rsp->flag & CMD_REG)
					return WL_RET_ERR_NOREG;				
				return WL_RET_ERR_NORSP;
			}
			cmd_rsp_arr->state = CMD_STATE_INIT;//re-send cmd
			cmd_rsp_arr->retry++;
			goto retry;
		}
		ret = wnet_get_rsp(wnet->rsp_buf+wnet->rall, sizeof(wnet->rsp_buf)-wnet->rall);
		if(ret > 0)
		{
			wnet->rall += ret;
			wnet->rsp_buf[wnet->rall] = 0;
			if(wnet_find_str(wnet->rsp_buf, cmd_rsp->cmd, wnet->rall))				
				cmd_rsp_arr->flag |= CMD_ECHO;//Echo 
			if(cmd_rsp->rsp!= NULL&&wnet_find_rsp(wnet->rsp_buf, cmd_rsp->rsp, wnet->rall))
			{			
				cmd_rsp_print(cmd_rsp->cmd, strlen(cmd_rsp->cmd));			
				wnet_debug("\nRSP:\n\t");
				cmd_rsp_print(cmd_rsp->rsp, strlen(cmd_rsp->rsp));
				wnet_debug("\n");
				if(cmd_rsp->flag&CMD_SIGNAL)
				{	
                                        ret = wnet_set_signal(wnet);
                                        if(ret==1) {
                                        	wnet->rall = 0;
                                        	return 1;
                                        }
				}else if(cmd_rsp->flag & CMD_CELL_INFO){
					if(wnet->mfr == MFR_HUAWEI_MU709 || wnet->mfr == MFR_HUAWEI_MC509){
						ret = wnet_set_ncellinfo(wnet);
						if(ret == 1){
							wnet->rall = 0;
							return 1;
						}
					}else{
						wnet_set_scellinfo(wnet);
						if(ret == 1){
							wnet->rall = 0;
							return 1;
						}	
					}			
				}
				else if(cmd_rsp->flag&CMD_REG)
				{
					ret = wnet_check_reg(wnet,REG_RSP_PTR);
					if(ret < 0)	return ret;
                                        else if(ret==1){
						wnet->rall = 0;
						return 1;
					}
				}else if(cmd_rsp->flag&CMD_DO_CGATT)
					wnet_set_cgatt(wnet);
				else if(cmd_rsp->flag&CMD_CHK_CREG)
				{
					ret = wnet_check_reg(wnet,"+CREG:");
					if(ret < 0) return ret;
					if(ret==1) {
						wnet->rall = 0;
						return 1;
					}				
				}
				else if(cmd_rsp->flag&CMD_CHK_MODE)
				{	
					if(wnet->mfr == MFR_HUAWEI_MU709){
						ret = Mux09_check_mode(wnet,WNET_Mux09_MODE);
						wnet_set_scellinfo(wnet);
						if(ret == 1){
							memcpy(cell_rsp,  "GSM",strlen("GSM"));
							memcpy(cell_cmd,"AT^MONNC\r",strlen("AT^MONNC\r"));
						}
						else if(ret == 2){
							memcpy(cell_rsp,  "OK",strlen("OK"));
							memcpy(cell_cmd,"AT\r",strlen("AT\r"));
						}else{
							wnet->rall = 0;
							return 1;
						}	
					}else if(wnet->mfr == MFR_HUAWEI_MC509){
						memcpy(cell_rsp,"SIQ:",strlen("SIQ:"));
						ret = Mc509_check_mode(wnet,"BSINFO:");
						if(ret == 1){
							wnet->rall = 0;
							return 1;
						}
					}
				}		
				return 0;
			}
			else if(cmd_rsp->rsp!= NULL && wnet_find_str(wnet->rsp_buf, WNET_BREAK_PTR, wnet->rall) && wnet->state==WNET_STATE_DIALING)
			{
				wnet_debug("Find No Carrier in signaling!\n");
				wnet_breakdown(wnet);
				return WL_RET_ERR_LINEOFF;		
			}			
		}
	}
	return 1;
}

void wnet_switch_state(WNET_DEV_T *wnet, WNET_STATE next_state) 
{
	PPP_DEV_T *ppp = (PPP_DEV_T *)&wnet->ppp_dev;
    
	wnet_debug("WNet State '%s' to '%s'[Delta=%d]\n", wnet_state_str(wnet->state), wnet_state_str(next_state),inet_jiffier-(wnet)->timestamp);

    switch(wnet->state)
	{
	case WNET_STATE_DOWN:
		wnet->err_code = 0;
		break;
	case WNET_STATE_PRE_DIALING:
		ppp_prepare(ppp);
		break;
	case WNET_STATE_DIALING:
		break;
	case WNET_STATE_UP:
		break;
	case WNET_STATE_PPP:
		break;
	case WNET_STATE_PRE_ONHOOK:
		break;
	case WNET_STATE_ONHOOK:
		ppp_stop_atmode(ppp);
		break;
	default:
		return;
	}
    
	switch(next_state)
	{
	case WNET_STATE_DOWN:
		if(wnet->logout_jiffier)
		{
			wnet_debug("WNet Logout OK, Delta=%d\n", inet_jiffier-(wnet)->logout_jiffier); 
			wnet->logout_jiffier = 0;
		}else if(wnet->login_jiffier)
		{
			wnet_debug("WNet Login Fail err=%d, Delta=%d\n",wnet->err_code,inet_jiffier-(wnet)->login_jiffier); 
			wnet->login_jiffier = 0;
		}
		if(wnet->signal.err==0)
		{
			wnet->signal.err = WL_RET_ERR_LINEOFF;
			wnet->signal.level = 0;
			wnet->signal.ber = 0;
		}
		ppp->inet_dev->echo_count = 0;
		if(ppp->at_cmd_mode==1)
			ppp_stop_atmode(ppp);
		break;
	case WNET_STATE_PRE_DIALING:
		break;
	case WNET_STATE_DIALING:
		#ifdef WL_CDMA_DTR_ENABLE
		CDMA_DTR_ON();
		wnet_debug("CDMA DTR On...\n");
		#endif		
		wnet_init_cmd(wnet, &wnet->dialing_cmd_rsp);
		break;
	case WNET_STATE_UP:
		ppp_login(ppp, ppp->userid, ppp->passwd, wnet->auth, 0);
		break;
	case WNET_STATE_PPP:
		wnet_debug("WNet Login OK, Delta=%d\n",inet_jiffier-(wnet)->login_jiffier); 
		wnet->login_jiffier = 0;
		wnet->signal.state = SIGNAL_STATE_IDLE;
		wnet->signal.nexttime = inet_jiffier+SIGNAL_UPDATE_TIME;
		wnet->signal.starttime = inet_jiffier+PRE_TOATMODE_WAIT;
		break;
	case WNET_STATE_PRE_ONHOOK:
		ppp_logout(&(wnet)->ppp_dev, wnet->err_code);
		break;
	case WNET_STATE_ONHOOK:
		#ifdef WL_CDMA_DTR_ENABLE
		CDMA_DTR_OFF();
		wnet_debug("CDMA DTR Off...\n");
		#endif		
		wnet_init_cmd(wnet, &wnet->onhook_cmd_rsp);
		ppp_start_atmode(ppp);
		break;
	default:
		return;
	}
	wnet->state = next_state;
	wnet->timestamp = inet_jiffier; 
}
static int wnet_check_carrier(WNET_DEV_T *wnet)
{
	char rsp[512];
	int cnt;
	memset(rsp, 0, sizeof(rsp));
	cnt = wnet_get_rsp(rsp, sizeof(rsp));
	if(cnt > 0)	{
		if(wnet_find_rsp(rsp, WNET_BREAK_PTR, cnt)){
			wnet_debug("!!Find No carrier!\n");
			wnet->no_carrier = 1;
			wnet->no_carrier_time = inet_jiffier;			
			if(wnet->signal.nexttime<=inet_jiffier +4000)//delay 4s to get signal
				wnet->signal.nexttime = inet_jiffier+4000;
			return 0;
		}else if(wnet->no_carrier == 1)
			wnet->no_carrier = 0;
	}
	if(wnet->no_carrier &&  inet_jiffier-wnet->no_carrier_time>3000){
		wnet->no_carrier = 0;
		wnet_debug("No Carrier keep 3s..........\n");
		return 1;
	}
	return 0;
}

static void wnet_do_signal(WNET_DEV_T *wnet, PPP_DEV_T *ppp)
{
	int err;
	WNET_CMD_RSP_ARR_T *cmd_rsp_arr = &wnet->signal.cmd_rsp_arr;
	WNET_CMD_RSP_T *cmd_rsp = cmd_rsp_arr->cmd_rsp;
	if(wnet->signal.state == SIGNAL_STATE_IDLE) return;
	if(cmd_rsp_arr->idx >= cmd_rsp_arr->num||cmd_rsp_arr->idx<0)    goto cancel;
	cmd_rsp += cmd_rsp_arr->idx;
	err = wnet_exec_cmd(wnet, cmd_rsp_arr);
	if(err < 0)	{			
		wnet_signal_err(wnet, err);
		if(cmd_rsp_arr->idx==0)//the first command has not finished*/			
			goto cancel;
	}else if(err==1)    return;
	wnet_next_cmd(wnet, &wnet->signal.cmd_rsp_arr);
	return;	
cancel:	
	wnet->signal.state = SIGNAL_STATE_IDLE;
	ppp_stop_atmode(ppp);
	return; 	
}

static void wnet_do_dialing(WNET_DEV_T *wnet, PPP_DEV_T *ppp)
{
	int err, i;
	WNET_CMD_RSP_ARR_T *cmd_rsp_arr = &wnet->dialing_cmd_rsp;
	i = cmd_rsp_arr->idx;
	if(i>=cmd_rsp_arr->num)
		wnet_switch_state(wnet, WNET_STATE_UP);
	else {
		err = wnet_exec_cmd(wnet, cmd_rsp_arr);
		if(err<0)
		{
			if (WL_RET_ERR_SOFT_RESET == err){
				wnet_debug("ERROR: WL_RET_ERR_SOFT_RESET!!!\r\n");
				if (wnet->reset_cnt++ < 2) {
					wnet_debug("wnet->reset_cnt=%d\r\n", wnet->reset_cnt);
					wnet_send_cmd("at+cfun=1,1\r");
					DelayMs(6000);		
					wnet_switch_state(wnet, WNET_STATE_PRE_DIALING);
				}
				else {
					wnet_debug("wnet->reset_cnt=%d, err=%d\r\n", wnet->reset_cnt, err);
					wnet_switch_state(wnet, WNET_STATE_DOWN);
					wnet->err_code = err;
				}				
				return;	
			}			
			wnet_debug("wnet_do_dialing cmd err=%d\n", err);
			goto fail;
 		}else if(err==0)
			wnet_next_cmd(wnet, cmd_rsp_arr);
	}	
	return;
fail:
	wnet_switch_state(wnet, WNET_STATE_ONHOOK);
	wnet->err_code = err;
	return ;
}

static void wnet_do_onhook(WNET_DEV_T *wnet, PPP_DEV_T *ppp)
{
	int err, i;
	WNET_CMD_RSP_ARR_T *cmd_rsp_arr = &wnet->onhook_cmd_rsp;

	if(wnet->timestamp+120*1000<inet_jiffier){
		err=0;
		goto cancel;
	}
	i = cmd_rsp_arr->idx;
	if(i>=cmd_rsp_arr->num){
		err = 0;
		goto cancel;
	}else{
		err = wnet_exec_cmd(wnet, cmd_rsp_arr);
		if(wnet->rall>0){
			if(wnet_find_str(wnet->rsp_buf, WNET_BREAK_PTR, wnet->rall) ||
                            wnet_find_str(wnet->rsp_buf, "ERROR", wnet->rall))
				goto cancel;
		}
		if(err<0)	goto cancel;
 		else if(err==0)
			wnet_next_cmd(wnet, cmd_rsp_arr);
	}	
	return;
cancel:
	wnet_switch_state(wnet, WNET_STATE_DOWN);
	return ;
}
static void wnet_timeout(unsigned int mask)
{
	WNET_DEV_T *wnet = &wnet_dev;
	PPP_DEV_T *ppp = &wnet->ppp_dev;
	static u32 last_print=0;
	if(wnet->type == 0) 	return;

        switch(wnet->user_event) //handle the user event firstly
        {
        case USER_EVENT_LOGIN:
            if(wnet->state == WNET_STATE_DOWN)
            {
                    wnet->timestamp = wnet->login_jiffier; 
                    wnet_switch_state(wnet, WNET_STATE_PRE_DIALING);               
            }            
            wnet->reset_cnt = 0;
            wnet->user_event = USER_EVENT_NONE;  
            break;
        case USER_EVENT_LOGOUT:
            if(wnet->state == WNET_STATE_PRE_DIALING)
            {
                    wnet->timestamp = wnet->logout_jiffier;
                    wnet_switch_state(wnet, WNET_STATE_DOWN);
            }
            else  if(wnet->state == WNET_STATE_DIALING)
            {
			ppp->last_tx_timestamp = inet_jiffier;/* Must be wait 1sec for TX Freed */
			wnet->err_code = NET_ERR_LOGOUT;
			wnet_switch_state(wnet, WNET_STATE_PRE_ONHOOK);         
            }
            else  if(wnet->state == WNET_STATE_UP)
            {
                    wnet->err_code = NET_ERR_LOGOUT;
                    wnet_switch_state(wnet, WNET_STATE_PRE_ONHOOK);
            }
            else  if(wnet->state == WNET_STATE_PPP)
            {
                    wnet->err_code = NET_ERR_LOGOUT;
                    wnet_switch_state(wnet, WNET_STATE_PRE_ONHOOK);
                    wnet->timestamp = wnet->logout_jiffier;            
            } 
            wnet->user_event = USER_EVENT_NONE;
            break;   
        default:
            break;
        }
        
	switch(wnet->state) //handle the timeout event according to the state
	{
	case WNET_STATE_DOWN:
		wnet->reset_cnt = 0;
		break;
	case WNET_STATE_PRE_DIALING: 
                wnet_switch_state(wnet, WNET_STATE_DIALING);  
		break;
	case WNET_STATE_DIALING:
                wnet_do_dialing(wnet, ppp);
		break;
	case WNET_STATE_UP:		
		if(ppp->state == PPPclsd||(ppp->state!=PPPopen&&wnet->timestamp+60*1000<inet_jiffier))
		{	
		        wnet->err_code = NET_ERR_PPP;
			wnet_switch_state(wnet, WNET_STATE_PRE_ONHOOK);
		}else if(ppp->state == PPPopen)
			wnet_switch_state(wnet, WNET_STATE_PPP);
		wnet->reset_cnt = 0;
		break;
	case WNET_STATE_PPP:
		/*the change of the state must  after handling the signal*/
		if(wnet->signal.state > SIGNAL_STATE_IDLE)
                {        	
                        wnet_do_signal(wnet, ppp);
                        break;
                }
		if(ppp->inet_dev->echo_count > 0)
		{			
			wnet->err_code = WL_RET_ERR_RST;/*packet echo */
			wnet_switch_state(wnet, WNET_STATE_PRE_ONHOOK);
			wnet->timestamp = inet_jiffier;		
		}else if(wnet->signal.cmd_rsp_arr.flag&CMD_ECHO)/*cmd echo*/
		{			
			wnet->err_code = WL_RET_ERR_RST;
			wnet_switch_state(wnet, WNET_STATE_PRE_ONHOOK);
		}else if(ppp->state==PPPopen)
		{
			if(wnet->signal.cmd_rsp_arr.num<= 0) break;
			if(wnet->signal.nexttime >=inet_jiffier)
			{	
                                if(wnet_check_carrier(wnet)==1)
                                {
                                	wnet_breakdown(wnet);
                                	break;
                                }
                                wnet->signal.starttime = inet_jiffier+PRE_TOATMODE_WAIT;         
			}else if(!tcp_conn_exist(ppp->inet_dev))
                        {
                        	if(wnet->signal.starttime<=inet_jiffier&&
                        		ppp->last_tx_timestamp+PRE_TOATMODE_WAIT<inet_jiffier)
                        	{
                        		ppp_start_atmode(ppp);
                        		wnet_init_cmd(wnet, &wnet->signal.cmd_rsp_arr);
                        		wnet->signal.state = SIGNAL_STATE_WAIT;
                        	}/*waiting */

                        }else/*waiting*/
                        {							
                        	wnet->signal.starttime = inet_jiffier+PRE_TOATMODE_WAIT;
                        	if(wnet->signal.nexttime+60*1000<inet_jiffier)
                        	{
                        		//wnet_signal_err(wnet, NET_ERR_TIMEOUT);
                        		wnet->signal.nexttime = inet_jiffier+SIGNAL_UPDATE_TIME;
                        	}
                        }      
		}else
		{
			wnet->err_code = NET_ERR_PPP;
			wnet_switch_state(wnet, WNET_STATE_PRE_ONHOOK);
		}
		break;
	case WNET_STATE_PRE_ONHOOK:
		if(ppp->state==PPPclsd||wnet->timestamp+5*1000<=inet_jiffier)
		{
			wnet_switch_state(wnet, WNET_STATE_ONHOOK);
		}
		break;
	case WNET_STATE_ONHOOK:
                wnet_do_onhook(wnet, ppp);
		break;
	default:
		wnet->user_event = USER_EVENT_NONE;
		break;
	}

#ifdef NET_DEBUG
		if(last_print+3000<inet_jiffier)
		{
			wnet_debug("WNet Do in '%s'\n",wnet_state_str(wnet->state));
			last_print = inet_jiffier;
		}
#endif		
}
static void wnet_irq_disable(PPP_DEV_T *ppp)
{
	ppp_com_irq_disable(ppp->comm);
}

static void wnet_irq_enable(PPP_DEV_T *ppp)
{
	ppp_com_irq_enable(ppp->comm);
}

static void wnet_irq_tx(PPP_DEV_T *ppp)
{
	ppp_com_irq_tx(ppp->comm);
}
static int  wnet_check(PPP_DEV_T *ppp, long ppp_state)
{
	int ret = -1;
	if(wnet_dev.state == WNET_STATE_UP ||wnet_dev.state == WNET_STATE_PPP ||
           wnet_dev.state == WNET_STATE_PRE_ONHOOK)
		ret = 0;
	return ret;
}
static int  wnet_ready(PPP_DEV_T *ppp)
{
	return 0;
}
static PPP_LINK_OPS  wnet_link_ops=
{
	wnet_irq_disable,
	wnet_irq_enable,
	wnet_irq_tx,
	wnet_ready,
	wnet_check,
	NULL,
	NULL,
};
void wnet_init_cmdrsp(WNET_CMD_RSP_ARR_T *cmd_rsp_arr, WNET_CMD_RSP_T *cmd_rsp)
{
	int i;
	if(cmd_rsp==NULL)
	{
		cmd_rsp_arr->cmd_rsp = NULL;
		cmd_rsp_arr->num = 0;
		return;
	}
	for(i=0; cmd_rsp[i].cmd!=NULL; i++);
	cmd_rsp_arr->cmd_rsp = cmd_rsp;
	cmd_rsp_arr->num = i;
}

int wnet_ppp_used( void)
{
	WNET_DEV_T *wnet=&wnet_dev;
	if(wnet->user_event != 0) return 1;
	if(wnet->state != WNET_STATE_DOWN)	return 1;
	return 0;
}

int wnet_get_scellinfo(char *rsp,int len)
{
	WNET_DEV_T *wnet=&wnet_dev;
	memcpy(rsp,wnet_cell_buf,len);
	return  0;
}

int wnet_get_ncellinfo(char *rsp,int len)
{
	WNET_DEV_T *wnet=&wnet_dev;
	memcpy(rsp,wnet_ncell_buf,len);
	return  0;
}

int wnet_get_mode(char *rsp,int len)
{
	WNET_DEV_T *wnet=&wnet_dev;
	memcpy(rsp,wnet_mode_buf,len);
	return  0;	
}

int wnet_get_signal(unsigned char *signal_level)
{
	WNET_DEV_T *wnet=&wnet_dev;
	if(wnet->signal.err==0)
	{
		if(signal_level)
			*signal_level = wnet->signal.level;
		return 0;
	}
	if(signal_level)    *signal_level = 0;
	return wnet->signal.err;
}
int wnet_type_set(int wtype, int mfr)
{
	WNET_DEV_T *wnet=&wnet_dev;
	if(mfr >= MFR_MAX_ID||mfr<0)	return NET_ERR_ARG;
	wnet->type = wtype;
	wnet->mfr = mfr;
	if(mfr == 0)
		strcpy(cell_rsp,"MCC:");
	else if(mfr == 1)
		strcpy(cell_rsp,"^SMOND:");
	return 0;
}
void s_InitWxNetPPP(void)
{
	WNET_DEV_T *wnet = &wnet_dev;
	PPP_DEV_T *ppp = &wnet->ppp_dev;
	INET_DEV_T *dev = &wnet->inet_dev;
	if(net_has_init())	return ;
	memset(&wnet_dev, 0, sizeof(wnet_dev));
	wnet_dev.signal.err = WL_RET_ERR_LINEOFF;
	ppp_dev_init(ppp);
	ppp_inet_init(ppp, dev, "GPRS_PPP", WNET_IFINDEX);    
        wnet->port = P_WNET;            
	ppp->link_ops = &wnet_link_ops;
        ppp->init = 1;
	wnet_ppp_timer.mask = INET_SOFTIRQ_TIMER;
	wnet_ppp_timer.h = (void (*)(unsigned long))wnet_ppp_timeout;
	inet_softirq_register(&wnet_ppp_timer);
	wnet_timer.mask = INET_SOFTIRQ_TIMER;
	wnet_timer.h = (void (*)(unsigned long))wnet_timeout;
	inet_softirq_register(&wnet_timer);
}

static WNET_CMD_RSP_T signal_cmd_rsps_mg323[]=/* GetSignal AT Cmds */
{
	{"+++", 		  NULL,   1500, 0, CMD_TOATMODE},
	{"AT+CSQ\r",	  "OK", 500,  2, CMD_SIGNAL},
	{"AT^SMOND\r", "OK",1000,1,CMD_CELL_INFO},
	{"ATO\r",		  NULL,   500,	0, 0},
	{NULL,			  NULL,   0,	0, 0},
};

static WNET_CMD_RSP_T signal_cmd_rsps_G6x0[]=/* GetSignal AT Cmds */
{
	{"+++", 		  NULL,   1500, 0, CMD_TOATMODE},
	{"AT+CSQ\r",	  "OK", 500,  2, CMD_SIGNAL},
	{cell_cmd, "OK",1000,2,CMD_CELL_INFO},
	{"ATO\r",		  NULL,   500,	0, 0},
	{NULL,			  NULL,   0,	0, 0},
};

static WNET_CMD_RSP_T signal_cmd_rsps_mu509[]=/* GetSignal AT Cmds */
{
	{"+++", 		         NULL,   1500, 0, CMD_TOATMODE},
	{"AT+CSQ\r",	  "OK", 500,  2, CMD_SIGNAL},
	//{cell_cmd,               "OK",1000,2,CMD_CELL_INFO},
	{"ATO\r",		  NULL,   1000,	0, 0},
	{NULL,			  NULL,   0,	0, 0},
};

static WNET_CMD_RSP_T signal_cmd_rsps_mu709[]=/* GetSignal AT Cmds */
{
	{"+++", 		         NULL,   1500, 0, CMD_TOATMODE},
	{"AT+CSQ\r",	  "OK", 1500,  1, CMD_SIGNAL},	
	{"AT^MONSC\r",	  "OK", 1500,  1, CMD_CHK_MODE},
	{cell_cmd, 	  "OK",3000,1,CMD_CELL_INFO},
	{"ATO\r",		  NULL,   1000,	0, 0},
	{NULL,			  NULL,   0,	0, 0},
};

static WNET_CMD_RSP_T signal_cmd_rsps_h330[]=/* GetSignal AT Cmds */
{
	{"+++", 		  NULL,   1500, 0, CMD_TOATMODE},
	{"AT+CSQ\r",	  "OK", 500,  2, CMD_SIGNAL},
	{cell_cmd, 	  "OK",3000,2,CMD_CELL_INFO},
	{"ATO\r",		  NULL,   500,	0, 0},
	{NULL,			  NULL,   0,	0, 0},
};

static WNET_CMD_RSP_T onhook_cmd_rsp_G6x0[]=
{
	{"",		     NULL, 10000 , 0, 0},
	{"AT\r",		 "OK", 500 ,  4, CMD_RST},
	{NULL, NULL, 0, 0},
};
static WNET_CMD_RSP_T dialing_cmd_rsps_G6x0[]=/* Dialing AT Cmds */
{
	{"AT\r",		"OK",	 500,	4, CMD_RST},
	{"ATE0V1\r",	"OK",	 1000,	1,	 0},
	{"ATS0=0\r",	"OK",	 1000,	0,	 0},
	{"AT+CSQ\r",	"OK", 2000,	4, CMD_SIGNAL},
	{apn_buffer,	"OK",	 2000, 15, 0},
	{"AT+CGATT?\r", CGATT_RSP_PTR,	  1000, 0,	CMD_CGATT},
	{"",			 NULL,			  1000, 0, CMD_CGATT},
	{"AT+CGATT=1\r","OK",			  3000, 15, CMD_CGATT},
	{"",			 NULL,			  1000, 0, CMD_CGATT},
	{"AT+CGREG?\r",	REG_RSP_PTR, 2000,	30, CMD_REG},
	{"",			 NULL,			  1000, 0, CMD_REG},
	{cell_cmd, "OK",1000,2,CMD_CELL_INFO},
	{atd_buffer,	"CONNECT",15000,	2, 0},
	{NULL,NULL, 0, 0},
};
static WNET_CMD_RSP_T onhook_cmd_rsp_mu509[]=
{
	{"",				NULL,	1000,			0,			0},
	{"AT\r",		"OK",	1000,			20,			0},
	{"ATH0\r",		"OK",	500,			3,			0},
	{NULL,			NULL,	0,				0,			0},
};
static WNET_CMD_RSP_T dialing_cmd_rsps_mu509[]= 
{
	{"AT\r",			"OK",				1000,		60,		0},
	{"ATE0v1\r",		"OK",				1000,		1,		0},
	{"ATS0=0\r",		"OK",				1000,		0,		0},
	{"AT+CSQ\r",	"OK",	2000,		4,		CMD_SIGNAL},
	{apn_buffer,	"OK",				2000,		15,		0},
	{atd_buffer,  "CONNECT",		1000,		15,		0},
	{NULL,				NULL,				0,			0},
};

static WNET_CMD_RSP_T dialing_cmd_rsps_mu709[]= 
{
	{"AT\r",			"OK",				1000,		60,		0},
	{"ATE0v1\r",		"OK",				1000,		1,		0},
	{"ATS0=0\r",		"OK",				1000,		0,		0},
	{"AT+CSQ\r",	"OK",	2000,		4,		CMD_SIGNAL},
	{"AT^MONSC\r",	"OK",	2000,		4,		CMD_CHK_MODE},
	{cell_cmd,	"OK",			  2000,		15,		CMD_CELL_INFO},
	{"",			 NULL,			  1000, 0, CMD_CELL_INFO},
	{apn_buffer,	"OK",				2000,		15,		0},
	{atd_buffer,  "CONNECT",		1000,		15,		0},
	{NULL,				NULL,				0,			0},
};

static WNET_CMD_RSP_T onhook_cmd_rsp_mc509[]=
{
	{"",		     NULL, 1000 , 0, 0},
	{"AT\r",		 "OK", 500 , 12, 0},
	{NULL, NULL, 0, 0},
};
static WNET_CMD_RSP_T dialing_cmd_rsps_mc509[]=/* Dialing AT Cmds */
{
	{"AT\r",		"OK",	 1000,	 60, 0},
	{"ATE0V1\r",	"OK",	 1000,	1,	 0},
	{"ATS0=0\r",	"OK",	 1000,	0,	 0},
	{"AT+CSQ?\r",	"OK", 2000,	4, CMD_SIGNAL},
	{"AT^BSINFO\r",	"OK", 2000,	2, CMD_CHK_MODE},
	{"AT^SIQ\r",		"OK",2000,2,CMD_CELL_INFO},
	{pppcfg_buff,	"OK", 3000,	4, 0},
	{atd_buffer,	"CONNECT",10000,  15, 0},
	{NULL, NULL, 0, 0},
};
static WNET_CMD_RSP_T onhook_cmd_rsp_mg323[]=
{
	{"",		     NULL, 10000 , 0, 0},
	{"+++", 		 NULL, 1000, 0, CMD_TOATMODE},
	{"ATH0\r",		 "OK", 500 , 0, 0},
	{"AT\r",		 "OK", 500 , 12, 0},
	{NULL, NULL, 0, 0},
};
static WNET_CMD_RSP_T dialing_cmd_rsps_mg323[]=/* Dialing AT Cmds */
{
	{"AT\r",		"OK",	 500,	4, CMD_RST},
	{"ATE0V1\r",	"OK",	 1000,	1,	 0},
	{"ATS0=0\r",	"OK",	 1000,	0,	 0},
	{"AT+CSQ\r",	"OK", 2000,	10, CMD_SIGNAL},
	{PDP_CFG,	    NULL,	 3000,	0,	 0},
	{apn_buffer,	"OK",	 2000, 15, 0},
	{"AT+CREG?\r",	"+CREG:", 5000, 15, CMD_CHK_CREG},
	{"AT+CGATT?\r", CGATT_RSP_PTR,	  1000, 0,	CMD_CGATT},
	{"",			 NULL,			  1000, 0, CMD_CGATT},
	{"AT+CGATT=1\r","OK",			  1000, 30, CMD_CGATT},
	{"",			 NULL,			  1000, 0, CMD_CGATT},
	{"AT+CGREG?\r",	REG_RSP_PTR, 2000,	30, CMD_REG},
	{"",			 NULL,			  1000, 0, CMD_CGATT},
	{"AT^SMOND\r", "OK",1000,1,CMD_CELL_INFO},
	{"",			 "^SMOND",			  1000, 0, CMD_CELL_INFO},
	{atd_buffer,	"CONNECT",ATD_TIMEOUT,	15, 0},
	{NULL,NULL, 0, 0},
};

static WNET_CMD_RSP_T onhook_cmd_rsp_h330[]=
{
	{"",		     NULL, 10000 , 0, 0},
	{"AT\r",		 "OK", 500 ,  4, CMD_RST},
	{NULL, NULL, 0, 0},
};

static WNET_CMD_RSP_T dialing_cmd_rsps_h330[]=/* Dialing AT Cmds */
{
	{"AT\r",		"OK",	 500,	4, CMD_RST},
	{"ATE0V1\r",	"OK",	 1000,	1,	 0},
	{"ATS0=0\r",	"OK",	 1000,	0,	 0},
	{"AT+CSQ\r",	"OK", 2000,	4, CMD_SIGNAL},

	{apn_buffer,	"OK",	 2000, 15, 0},

	{"AT+CGATT?\r", CGATT_RSP_PTR,	  1000, 0,	CMD_CGATT},
	{"",			 NULL,			  1000, 0, CMD_CGATT},
	{"AT+CGATT=1\r","OK",			  3000, 15, CMD_CGATT},
	{"",			 NULL,			  1000, 0, CMD_CGATT},
	{"AT+CGREG?\r",	REG_RSP_PTR, 2000,	30, CMD_REG},
	{"",			 NULL,			  1000, 0, CMD_REG},
	{cell_cmd,	"OK", 2000,	3, CMD_CELL_INFO},
	{atd_buffer,	"CONNECT",15000,	2, 0},
	{NULL,NULL, 0, 0},
};

static WNET_CMD_RSP_T* cmd_rsp_arr_set[MFR_MAX_ID][3]=
{
	{signal_cmd_rsps_G6x0, onhook_cmd_rsp_G6x0, dialing_cmd_rsps_G6x0},	
	{signal_cmd_rsps_mg323, onhook_cmd_rsp_mg323, dialing_cmd_rsps_mg323},
	{signal_cmd_rsps_mu509, onhook_cmd_rsp_mu509, dialing_cmd_rsps_mu509},
	{NULL, onhook_cmd_rsp_mc509, dialing_cmd_rsps_mc509},	
	{signal_cmd_rsps_h330, onhook_cmd_rsp_h330, dialing_cmd_rsps_h330},
	{signal_cmd_rsps_mu709, onhook_cmd_rsp_mu509, dialing_cmd_rsps_mu709},
};

void wnet_init_cgatt(WNET_DEV_T *wnet)
{
	int i, start, end;	
        start = end = -1;    
	for(i=0; i<wnet->dialing_cmd_rsp.num; i++)
	{
		if(wnet->dialing_cmd_rsp.cmd_rsp[i].flag&CMD_CGATT)
		{
			if(start < 0)   start = i;
			end = i;
                        if(wnet->mfr == MFR_HUAWEI_MG323 )
                        {
                            if(flag_cgatt == 0)
                                wnet->dialing_cmd_rsp.cmd_rsp[i].flag |= CMD_UNDO;                                
                            else
                                wnet->dialing_cmd_rsp.cmd_rsp[i].flag &= ~CMD_UNDO;
                        }
		}
	}
        flag_cgatt = 1;
	if(end>start)
	{
		wnet->dialing_cmd_rsp.cmd_rsp[start].flag |= CMD_DO_CGATT;
		start++;
		wnet->cgatt_do_start = start;
		wnet->cgatt_do_end = end;
		wnet_debug("CGATT: %d~%d\n", start, end);	
	}
}
int wnet_init_atcmd(WNET_DEV_T *wnet, const char *dial_num, const char *apn, const char *Uid, const char *Pwd)
{
	WNET_CMD_RSP_T *dialing_cmd_rsp_arr = NULL;   
        wnet_init_cmdrsp(&wnet_dev.signal.cmd_rsp_arr, cmd_rsp_arr_set[wnet->mfr][0]);
        wnet_init_cmdrsp(&wnet_dev.onhook_cmd_rsp, cmd_rsp_arr_set[wnet->mfr][1]);  
        wnet_init_cmdrsp(&wnet_dev.dialing_cmd_rsp, cmd_rsp_arr_set[wnet->mfr][2]);  
        if(NULL == apn) 
        {
            wnet->dialing_cmd_rsp.num = 0;
            return NET_OK;
        }    
	memset(atd_buffer, 0, sizeof(atd_buffer));
        memset(apn_buffer, 0, sizeof(apn_buffer));
        memset(pppcfg_buff, 0, sizeof(pppcfg_buff)); 
        dialing_cmd_rsp_arr = cmd_rsp_arr_set[wnet->mfr][2];            
        if(NULL == Uid) Uid="";
        if(NULL == Pwd) Pwd="";
        sprintf(pppcfg_buff, "AT^PPPCFG=\"%s\",\"%s\"\r", Uid, Pwd);	    
        if(dial_num==NULL || *dial_num==0)
	 {
                if(WTYPE_CDMA == wnet->type)
                    strcpy(atd_buffer, "ATD#777\r");
                else
                    strcpy(atd_buffer, "ATD*99***1#\r");		
	}else {		
		if(memcmp("ATD", dial_num, 3) == 0)
			sprintf(atd_buffer, "%s\r", dial_num);
		else 
			sprintf(atd_buffer, "ATD%s\r", dial_num);
	}	
	if(apn)
	{     
                sprintf(apn_buffer,"at+cgdcont=1,\"IP\",\"%s\"\r",apn);   
		wnet_init_cgatt(wnet);
	}
	return NET_OK;
}

int wnet_login(const char *DialNum, const char* ApnNum,char * Uid,char * Pwd,  long Auth, int timeout,int  AliveInterval)
{
	WNET_DEV_T *wnet = &wnet_dev;
	PPP_DEV_T *ppp = &wnet->ppp_dev;
	int ret;    

    if(wnet->mfr < 0 || wnet->mfr >= MFR_MAX_ID || wnet->type == 0) return NET_ERR_SYS;            
    if(WlGetInitStatus()) return NET_ERR_INIT;
	if(str_check_max(DialNum, 99) ||str_check_max(ApnNum, 99) ||
            str_check_max(Uid, sizeof(ppp->userid)-1)!=NET_OK||
            str_check_max(Pwd, sizeof(ppp->passwd)-1)!=NET_OK)
            return NET_ERR_STR;
    
	if(wnet->state == WNET_STATE_PPP)
	{
		wnet->user_event = USER_EVENT_NONE;//Clear Any User Request!
		return NET_OK;
	}
	if(wnet->user_event == USER_EVENT_LOGIN||wnet->state == WNET_STATE_DIALING||
            wnet->state == WNET_STATE_UP)
		return WL_RET_ERR_DIALING;
	    
	ret = wnet_init_atcmd(wnet, DialNum, ApnNum,Uid, Pwd);
	if(ret<0)   return ret;	
	ret = ppp_set_authinfo(ppp, Uid, Pwd);
	if(ret < 0) return ret;        	
	
	memset(PDP_CFG, 0, sizeof(PDP_CFG));
	if(Auth != PPP_ALG_MSCHAPV1 && Auth != PPP_ALG_MSCHAPV2)
		sprintf(PDP_CFG, "AT+CGAUTH=1,%d,\"%s\",\"%s\"\r",Auth&0x3,ppp->userid, ppp->passwd);   
	else
		strcpy(PDP_CFG,"AT\r");
	wnet->auth = Auth;
	if(wnet->type == WTYPE_CDMA) ppp_set_keepalive(ppp, AliveInterval);
    
        PortOpen((u8)wnet->port, "115200,8,n,1");//shics add
        ppp->comm = ppp_get_comm(wnet->port);//shics add      
       
	wnet->user_event = USER_EVENT_LOGIN;
	wnet->login_jiffier = inet_jiffier;
	if(wnet->login_jiffier == 0)wnet->login_jiffier = 1;
	return 1;
}

int WlPppLoginEx(const char *DialNum, const char *Apn,char *Uid, char *Pwd,  long Auth, int timeout, int  AliveInterval)
{
        int ret;
	WNET_DEV_T *wnet = &wnet_dev;		
        if (timeout > 0 && timeout<10000) timeout = 10000;
	wnet->no_carrier = 0;	
	if(AliveInterval > 0)
	{
		if(AliveInterval > 36000) return NET_ERR_ARG;
		if(AliveInterval<10) AliveInterval = 10;
		AliveInterval *= 1000;
	}

	if (-1==Mc509MainTaskWaitPppRelink(0)) return NET_OK;
	
	if (-1==TryLockedMc509(10*30)) //30 sec
		return NET_ERR_TIMEOUT;
	
	inet_softirq_disable();
	ret = wnet_login(DialNum, Apn, Uid, Pwd, Auth, timeout, AliveInterval);
	inet_softirq_enable();
	
	UnlockMc509();
	
	if(ret<=0) 	return ret;
	while(1)
	{
		if(wnet->state == WNET_STATE_PPP) return NET_OK;
		else if(wnet->user_event==0)
		{
			if(wnet->state == WNET_STATE_DOWN)
			{
				if(wnet->err_code)	return wnet->err_code;
				return NET_ERR_LINKDOWN;
			}
		}
		if(timeout==0) return NET_ERR_TIMEOUT;
		inet_delay(INET_TIMER_SCALE);
		if(timeout > 0)
		{
			if(timeout > INET_TIMER_SCALE) timeout -= INET_TIMER_SCALE;
			else	timeout = 0;
		}
	}
	return NET_ERR_LINKDOWN;
}

int WlPppLogin(const char * Apn,char * Uid,char * Pwd,  long Auth, int timeout,int  AliveInterval)
{
    return WlPppLoginEx(NULL, Apn,Uid, Pwd, Auth, timeout, AliveInterval);
}

int WlPppCheck(void)
{
	WNET_DEV_T *wnet = &wnet_dev;

	if (-1==Mc509MainTaskWaitPppRelink(0)) return NET_OK;//time in the PPP relinking.

	if(wnet->type == 0) 	return NET_ERR_SYS;    
	if(wnet->user_event != 0)return 1;//doing
	if(wnet->state == WNET_STATE_PPP) return NET_OK;
        if(wnet->state!=WNET_STATE_DOWN ) return 1;//doing
	if(wnet->err_code)	return wnet->err_code;
	return NET_ERR_LINKDOWN;
}

void WlPppLogout(void)
{
	WNET_DEV_T *wnet = &wnet_dev;

	Mc509MainTaskWaitPppRelink(30);
	if (-1==TryLockedMc509(10*30)) //30 sec
		return;

	inet_softirq_disable();
        if(wnet->type == 0||wnet->state == WNET_STATE_DOWN||
            wnet->state == WNET_STATE_PRE_ONHOOK||wnet->state == WNET_STATE_ONHOOK)
		wnet->user_event = USER_EVENT_NONE;
        else if(wnet->user_event != USER_EVENT_LOGOUT){
		wnet->user_event = USER_EVENT_LOGOUT;
		wnet->logout_jiffier = inet_jiffier;
		if(wnet->logout_jiffier == 0) wnet->logout_jiffier = 1;
	}
	inet_softirq_enable();

	UnlockMc509();
}



#define irq_save(x) irq_save_asm(&x)
#define irq_restore(x) irq_restore_asm(&x)

static volatile int mc509_status=0;
static volatile int mc509_lock=0;

int Mc509StatusGet()
{
	int temp;
	unsigned int flag = 0;
	WNET_DEV_T *wnet = &wnet_dev;
	if (wnet->mfr != MFR_HUAWEI_MC509) return -2;//gprs modem type error.	
	irq_save(flag);
	temp=mc509_status;
	irq_restore(flag);
	return temp;

}

int Mc509StatusSet(int status)
{
	int temp;
	unsigned int flag = 0;
	WNET_DEV_T *wnet = &wnet_dev;
	if (wnet->mfr != MFR_HUAWEI_MC509) return -2;//gprs modem type error.	
	irq_save(flag);
	mc509_status=status;
	irq_restore(flag);
	return temp;	
}

void UnlockMc509()
{
	unsigned int flag = 0;
	WNET_DEV_T *wnet = &wnet_dev;
	if (wnet->mfr != MFR_HUAWEI_MC509) return ;//gprs modem type error.	
	irq_save(flag);
	mc509_lock=0;
	irq_restore(flag);
}

int TryLockedMc509(unsigned int timeout)//timeout * 100ms; MCU time slice=2ms
{
	unsigned int flag = 0;
	unsigned int cnt=0;
	int temp=0;
	WNET_DEV_T *wnet = &wnet_dev;
	if (wnet->mfr != MFR_HUAWEI_MC509) return -2;//gprs modem type error.
	while (temp==0) {
		irq_save(flag);
		if (mc509_lock==0){
			mc509_lock=1;
			temp=1;
		}
		else
			temp=0;
		irq_restore(flag);
		if (temp==0) {
			if (cnt>=timeout) break;
			cnt++; 
			OsSleep(50);/*2ms*50=100ms*/
		}
	}
	if (temp==0) return -1; //time out
	return 0;
}

int Mc509MainTaskWaitPppRelink(int time)//uint: 1 sec
{
	WNET_DEV_T *wnet = &wnet_dev;
	if (time<0) time=15;
	int cnt=0;
	if (wnet->mfr == MFR_HUAWEI_MC509 && GetLbsFlag()==1) {
		while (Mc509StatusGet()==1 && IsMainCodeTask()) {
			//see_debug("Mc509MainTaskWaitPppRelink!\r\n");
			if (cnt++ >= time) return -1;//time out
			DelayMs(1000);
		}
	}
	return cnt;
}

int PppRetryLinking()
{
	int ret=0;
	WNET_DEV_T *wnet = &wnet_dev;
	PPP_DEV_T *ppp = &wnet->ppp_dev;
	if (wnet->mfr != MFR_HUAWEI_MC509) return 0;
	if(tcp_conn_exist(ppp->inet_dev)) return 1;
	if (WlPppCheck()!=NET_OK) return 0;
	//PPP linking
	//==========PPP reconneting start=========
	Mc509StatusSet(1);
	WlPppLogout();
	while(WlPppCheck()==1) {OsSleep(500*1);}
	//OsSleep(500*1);
	ret=Mc509ReLoginEx();
	Mc509StatusSet(0);
	//==========PPP reconneting end=========
	return ret;
}
