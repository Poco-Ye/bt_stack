
#include "pmu.h"

volatile unsigned char  gucPmuLdoon=0;
extern volatile int gBatteryCheckBusy;

int s_PmuWrite(unsigned char addr, unsigned char *data, int len)
{
	int iRet;
	iRet = pmu_i2c_write_str_toaddr(PMU_SLAVER, addr, data, len);
	return iRet;
}
int s_PmuRead(unsigned char addr, unsigned char *data, int len)
{
	int iRet;
	iRet = pmu_i2c_read_str_fromaddr(PMU_SLAVER, addr, data, len);
	return iRet;
}

void s_PowerSwitch(unsigned char vout, unsigned char OnOff)
{
	unsigned char data1 = 0,data2 = 0;
	int iRet1,iRet2;
	int i;
	unsigned int flag;
	//asm volatile ("di; nop; nop; nop;");
	//irq_save(flag);
	gBatteryCheckBusy = 1;
	if (OnOff == 0)
 	{	
 		gucPmuLdoon &= (~vout);
	}
	else
	{
		gucPmuLdoon |= vout;
	}
	data1 = gucPmuLdoon;
	for (i = 0;i < 3;i++)
	{
		iRet1 = s_PmuWrite(PMU_ADDR_LDOON, &data1, 1);
		if (iRet1 == 1)
		{
			iRet2 = s_PmuRead(PMU_ADDR_LDOON, &data2, 1);
			if ((iRet2 == 1)&&gucPmuLdoon==data2) break;
		}
	}
	gBatteryCheckBusy = 0;
	//irq_restore(flag);
	//asm volatile ("ei; nop; nop; nop;");
	DelayMs(10);
}
/**
for TouchKey Update
*/
void s_TouchKeyPowerSwitch(uchar OnOff)
{
	uchar data1 = 0,data2 = 0;
	int iRet1,iRet2;
	int i;

	gBatteryCheckBusy = 1;
	if (OnOff == 0)
 	{	
 		gucPmuLdoon &= (~PMU_TOUCHKEY_POWER);
	}
	else
	{
		gucPmuLdoon |= PMU_TOUCHKEY_POWER;
	}
	data1 = gucPmuLdoon;
	for (i = 0;i < 3;i++)
	{
		iRet1 = s_PmuWrite(PMU_ADDR_LDOON, &data1, 1);
		if (iRet1 == 1)
		{
			iRet2 = s_PmuRead(PMU_ADDR_LDOON, &data2, 1);
			if ((iRet2 == 1)&&gucPmuLdoon==data2) break;
		}
	}
	gBatteryCheckBusy = 0;
}

void s_PmuSetChargeMode(int mode)
{
	gpio_set_pin_type(GPIOA, 19, GPIO_OUTPUT);
	
	if (mode)
		gpio_set_pin_val(GPIOA, 19, 1);
	else
		gpio_set_pin_val(GPIOA, 19, 0);
}

/*
 * LDO1 : 3V VBAT
 * LDO2 : -
 * LDO3 : -
 * LDO4 : 3.3V CPU3.3V电源+LCD除背光外电源+串口IC电源
 * LDO5 : 3.3V WIFI(280ma)
 * LDO6 : 3.3V touchkey ic CY8C20436A电源
 * LDO7 : 3.3V BT(70ma)
 * LDO8 : 3.3V MAG电源(8ma)+LCD背光(60ma)
 */
void s_PmuInit(void)
{
	unsigned char data;
	int i;
	int iRet;
	pmu_i2c_config();
	for (i = 0;i < 3;i++)
	{
		iRet = s_PmuRead(PMU_ADDR_LDOON, &data, 1);
		if (iRet == 1) break;
	}
	if(iRet != 1)
	{
		ScrCls();
		ScrPrint(0, 0, 0x00, "PMU Init Err!");
		getkey();
		return ;
	}
	gucPmuLdoon = data;
	s_PowerSwitch(0x76, 0);//poweroff VOUT2 VOUT3 VOUT5 VOUT6 VOUT7
	data = 0x06;
	s_PmuWrite(PMU_ADDR_LDO5DAC, &data, 1);//VOUT5 3.3V
	data = 0x07;
	s_PmuWrite(PMU_ADDR_LDO6DAC, &data, 1);//VOUT6 3.3V
	s_PmuWrite(PMU_ADDR_LDO7DAC, &data, 1);//VOUT7 3.3V
	data = 0x48;
	s_PmuWrite(PMU_ADDR_DD2DAC, &data, 1);//DC2 1.8V
	data = 0x03;
	s_PmuWrite(PMU_ADDR_DDCTL1, &data, 1);//DC2/DC1 ON,DC3 OFF

	//s_PowerBatteryChargeCtl(0);//Disable charging
	s_PmuSetChargeCurrent(0);

	/* power on VOUT5 VOUT6 VOUT7 */
	s_PowerSwitch(PMU_WIFI_POWER, 1);
	s_PowerSwitch(PMU_TOUCHKEY_POWER, 1);
	s_PowerSwitch(PMU_BT_POWER, 1);
	DelayMs(100);

	s_PmuSetChargeMode(1);
}



void s_PmuSetChargeCurrent(int mode)
{
	unsigned char data;
	if(mode)
	{
		data = 0x08;
		s_PmuWrite(PMU_ADDR_FET1CNT, &data, 1);//set SW1 limit current to 1080mA

		data = 0x07; // 800mA
		data = 0x06; // 700mA
		data = 0x05; // 600mA
		//data = 0x04; // 500mA
		s_PmuWrite(PMU_ADDR_FET2CNT, &data, 1);//set rapid current to 900mA

		data = 0x28;
		s_PmuWrite(PMU_ADDR_TSET, &data, 1);//set rapid timer to 240min

		data = 0x03;
		s_PmuWrite(PMU_ADDR_CMPSET, &data, 1);//set complete current to 100mA
	}
	else
	{
		
		data = 0x04;
		s_PmuWrite(PMU_ADDR_FET1CNT, &data, 1);//set SW1 limit current to 600mA

		data = 0x04; // 500mA
		data = 0x03; // 400mA
		s_PmuWrite(PMU_ADDR_FET2CNT, &data, 1);//set rapid current to 500mA

		data = 0x2C;
		s_PmuWrite(PMU_ADDR_TSET, &data, 1);//set rapid timer to 300min

		data = 0x03;
		s_PmuWrite(PMU_ADDR_CMPSET, &data, 1);//set complete current to 100mA
	}
}
void s_PmuWifiPowerSwitch(int OnOff)
{
	static int flag = 0;
	int v = OnOff ? 1 : 0;
	
	if(flag == 0)
	{
		gpio_set_pin_type(GPIOE, 0, GPIO_OUTPUT);
		flag = 1;
	}
	if(v == 1)
	{
		s_PowerSwitch(PMU_WIFI_POWER, 1);
		gpio_set_pin_val(GPIOE, 0, 0);
	}
	else
	{
		gpio_set_pin_val(GPIOE, 0, 1);
	}
}

void s_PmuBtPowerSwitch(int OnOff)
{
	int v = OnOff ? 1 : 0;
	s_PowerSwitch(PMU_BT_POWER, v);
}

#ifdef TEST
void TestPmuIntrSet(int bit)
{
	unsigned char ch;
	unsigned char pmustatus;
	while(1)
	{
		iDPrintf("=========================\r\n");
		iDPrintf("1-Enable\r\n");
		iDPrintf("2-Disable\r\n");
		iDPrintf("e-return\r\n");
		iDPrintf("=========================\r\n");
		ch = s_getkey();
		s_PmuRead(PMU_ADDR_CHGEN1,&pmustatus,1);
		if (ch == '1')
		{
			pmustatus |= 1<<bit;
			s_PmuWrite(PMU_ADDR_CHGEN1,&pmustatus,1);
			s_PmuRead(PMU_ADDR_CHGEN1,&pmustatus,1);
			iDPrintf("CHGEN1=0x%02x\r\n",pmustatus);
		}
		else if (ch == '2')
		{
			pmustatus &= ~(1<<bit);
			s_PmuWrite(PMU_ADDR_CHGEN1,&pmustatus,1);
			s_PmuRead(PMU_ADDR_CHGEN1,&pmustatus,1);
			iDPrintf("CHGEN1=0x%02x\r\n",pmustatus);
		}
		else if (ch == 'e')
		{
			return;
		}
		else
		{}
	}
}
void TestPmuIntrConfig(void)
{
	unsigned char ch;
	while(1)
	{
		iDPrintf("=========================\r\n");
		iDPrintf("1-Battery Over voltage interrupt\r\n");
		iDPrintf("2-Adapter Over voltage interrupt\r\n");
		iDPrintf("3-No Battery Detect interrupt\r\n");
		iDPrintf("4-Battery abnormal temperature interrupt\r\n");
		iDPrintf("5-SW1 or SW2 in charger interrupt\r\n");
		iDPrintf("6-Adapter insert & remove interrupt\r\n");
		iDPrintf("e-return\r\n");
		iDPrintf("=========================\r\n");
		ch = s_getkey();
		switch(ch)
		{
			case '1':
				TestPmuIntrSet(7);
				break;
			case '2':
				TestPmuIntrSet(6);
				break;
			case '3':
				TestPmuIntrSet(5);
				break;
			case '4':
				TestPmuIntrSet(2);
				break;
			case '5':
				TestPmuIntrSet(1);
				break;
			case '6':
				TestPmuIntrSet(0);
				break;
			case 'e':
				return;
			default:
				break;
		}
	}
}
void TestPmuSetChargeTime(void)
{
	unsigned char ch;
	unsigned char pmustatus;
	while(1)
	{
		iDPrintf("=========================\r\n");
		iDPrintf("1-120min\r\n");
		iDPrintf("2-180min\r\n");
		iDPrintf("3-240min\r\n");
		iDPrintf("4-300min\r\n");
		iDPrintf("e-return\r\n");
		iDPrintf("=========================\r\n");
		ch = s_getkey();
		switch(ch)
		{
			case '1':
				s_PmuRead(PMU_ADDR_TSET,&pmustatus,1);
				pmustatus &= ~(3<<2);
				pmustatus |= (0<<2);
				s_PmuWrite(PMU_ADDR_TSET,&pmustatus,1);
				s_PmuRead(PMU_ADDR_TSET,&pmustatus,1);
				iDPrintf("TSET=0x%02x\r\n",pmustatus);
				break;
			case '2':
				s_PmuRead(PMU_ADDR_TSET,&pmustatus,1);
				pmustatus &= ~(3<<2);
				pmustatus |= (1<<2);
				s_PmuWrite(PMU_ADDR_TSET,&pmustatus,1);
				s_PmuRead(PMU_ADDR_TSET,&pmustatus,1);
				iDPrintf("TSET=0x%02x\r\n",pmustatus);
				break;
			case '3':
				s_PmuRead(PMU_ADDR_TSET,&pmustatus,1);
				pmustatus &= ~(3<<2);
				pmustatus |= (2<<2);
				s_PmuWrite(PMU_ADDR_TSET,&pmustatus,1);
				s_PmuRead(PMU_ADDR_TSET,&pmustatus,1);
				iDPrintf("TSET=0x%02x\r\n",pmustatus);
				break;
			case '4':
				s_PmuRead(PMU_ADDR_TSET,&pmustatus,1);
				pmustatus &= ~(3<<2);
				pmustatus |= (3<<2);
				s_PmuWrite(PMU_ADDR_TSET,&pmustatus,1);
				s_PmuRead(PMU_ADDR_TSET,&pmustatus,1);
				iDPrintf("TSET=0x%02x\r\n",pmustatus);
				break;
			case 'e':
				return;
			default:
				break;
		}
	}
}
void TestPmuSetChargeCompleteCurrent(void)
{
	unsigned char ch;
	unsigned char pmustatus;
	while(1)
	{
		iDPrintf("=========================\r\n");
		iDPrintf("1-25mA\r\n");
		iDPrintf("2-50mA\r\n");
		iDPrintf("3-75mA\r\n");
		iDPrintf("4-100mA\r\n");
		iDPrintf("5-125mA\r\n");
		iDPrintf("6-150mA\r\n");
		iDPrintf("7-175mA\r\n");
		iDPrintf("8-200mA\r\n");
		iDPrintf("e-return\r\n");
		iDPrintf("=========================\r\n");
		ch = s_getkey();
		switch(ch)
		{
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
				s_PmuRead(PMU_ADDR_CMPSET,&pmustatus,1);
				pmustatus &= ~(7<<0);
				pmustatus |= (ch-'1');
				s_PmuWrite(PMU_ADDR_CMPSET,&pmustatus,1);
				s_PmuRead(PMU_ADDR_CMPSET,&pmustatus,1);
				iDPrintf("CMPSET=0x%02x\r\n",pmustatus);
				break;
			case 'e':
				return;
			default:
				break;
		}
	}
}
void TestPmuSetCRCC(void)
{
	unsigned char ch;
	unsigned char pmustatus;
	while(1)
	{
		iDPrintf("=========================\r\n");
		iDPrintf("1-0mA\r\n");
		iDPrintf("2-10mA\r\n");
		iDPrintf("e-return\r\n");
		iDPrintf("=========================\r\n");
		ch = s_getkey();
		switch(ch)
		{
			case '1':
				s_PmuRead(PMU_ADDR_SUSPEND,&pmustatus,1);
				pmustatus |= (1<<4);
				s_PmuWrite(PMU_ADDR_SUSPEND,&pmustatus,1);
				s_PmuRead(PMU_ADDR_SUSPEND,&pmustatus,1);
				iDPrintf("SUSPEND=0x%02x\r\n",pmustatus);
				break;
			case '2':
				s_PmuRead(PMU_ADDR_SUSPEND,&pmustatus,1);
				pmustatus &= ~(1<<4);
				s_PmuWrite(PMU_ADDR_SUSPEND,&pmustatus,1);
				s_PmuRead(PMU_ADDR_SUSPEND,&pmustatus,1);
				iDPrintf("SUSPEND=0x%02x\r\n",pmustatus);
				break;
			case 'e':
				return;
			default:
				break;
		}
	}
}
void TestPmuSetILIM(void)
{
	unsigned char ch;
	unsigned char pmustatus;
	while(1)
	{
		iDPrintf("=========================\r\n");
		iDPrintf("1-480mA\r\n");
		iDPrintf("2-600mA\r\n");
		iDPrintf("3-720mA\r\n");
		iDPrintf("4-840mA\r\n");
		iDPrintf("5-960mA\r\n");
		iDPrintf("e-return\r\n");
		iDPrintf("=========================\r\n");
		ch = s_getkey();
		switch(ch)
		{
			case '1':
				s_PmuRead(PMU_ADDR_FET1CNT,&pmustatus,1);
				pmustatus &= ~(0x0f);
				pmustatus |= (3);
				s_PmuWrite(PMU_ADDR_FET1CNT,&pmustatus,1);
				s_PmuRead(PMU_ADDR_FET1CNT,&pmustatus,1);
				iDPrintf("FET1CNT=0x%02x\r\n",pmustatus);
				break;
			case '2':
				s_PmuRead(PMU_ADDR_FET1CNT,&pmustatus,1);
				pmustatus &= ~(0x0f);
				pmustatus |= (4);
				s_PmuWrite(PMU_ADDR_FET1CNT,&pmustatus,1);
				s_PmuRead(PMU_ADDR_FET1CNT,&pmustatus,1);
				iDPrintf("FET1CNT=0x%02x\r\n",pmustatus);
				break;
			case '3':
				s_PmuRead(PMU_ADDR_FET1CNT,&pmustatus,1);
				pmustatus &= ~(0x0f);
				pmustatus |= (5);
				s_PmuWrite(PMU_ADDR_FET1CNT,&pmustatus,1);
				s_PmuRead(PMU_ADDR_FET1CNT,&pmustatus,1);
				iDPrintf("FET1CNT=0x%02x\r\n",pmustatus);
				break;
			case '4':
				s_PmuRead(PMU_ADDR_FET1CNT,&pmustatus,1);
				pmustatus &= ~(0x0f);
				pmustatus |= (6);
				s_PmuWrite(PMU_ADDR_FET1CNT,&pmustatus,1);
				s_PmuRead(PMU_ADDR_FET1CNT,&pmustatus,1);
				iDPrintf("FET1CNT=0x%02x\r\n",pmustatus);
				break;
			case '5':
				s_PmuRead(PMU_ADDR_FET1CNT,&pmustatus,1);
				pmustatus &= ~(0x0f);
				pmustatus |= (7);
				s_PmuWrite(PMU_ADDR_FET1CNT,&pmustatus,1);
				s_PmuRead(PMU_ADDR_FET1CNT,&pmustatus,1);
				iDPrintf("FET1CNT=0x%02x\r\n",pmustatus);
				break;
			case 'e':
				return;
			default:
				break;
		}
	}
}
void TestPmuSetChargeCurrent(void)
{
	unsigned char ch;
	unsigned char pmustatus;
	while(1)
	{
		iDPrintf("=========================\r\n");
		iDPrintf("1-400mA\r\n");
		iDPrintf("2-500mA\r\n");
		iDPrintf("3-700mA\r\n");
		iDPrintf("4-900mA\r\n");
		iDPrintf("e-return\r\n");
		iDPrintf("=========================\r\n");
		ch = s_getkey();
		switch(ch)
		{
			case '1':
				s_PmuRead(PMU_ADDR_FET2CNT,&pmustatus,1);
				pmustatus &= ~(0x0f);
				pmustatus |= (3);
				s_PmuWrite(PMU_ADDR_FET2CNT,&pmustatus,1);
				s_PmuRead(PMU_ADDR_FET2CNT,&pmustatus,1);
				iDPrintf("FET2CNT=0x%02x\r\n",pmustatus);
				break;
			case '2':
				s_PmuRead(PMU_ADDR_FET2CNT,&pmustatus,1);
				pmustatus &= ~(0x0f);
				pmustatus |= (4);
				s_PmuWrite(PMU_ADDR_FET2CNT,&pmustatus,1);
				s_PmuRead(PMU_ADDR_FET2CNT,&pmustatus,1);
				iDPrintf("FET2CNT=0x%02x\r\n",pmustatus);
				break;
			case '3':
				s_PmuRead(PMU_ADDR_FET2CNT,&pmustatus,1);
				pmustatus &= ~(0x0f);
				pmustatus |= (6);
				s_PmuWrite(PMU_ADDR_FET2CNT,&pmustatus,1);
				s_PmuRead(PMU_ADDR_FET2CNT,&pmustatus,1);
				iDPrintf("FET2CNT=0x%02x\r\n",pmustatus);
				break;
			case '4':
				s_PmuRead(PMU_ADDR_FET2CNT,&pmustatus,1);
				pmustatus &= ~(0x0f);
				pmustatus |= (8);
				s_PmuWrite(PMU_ADDR_FET2CNT,&pmustatus,1);
				s_PmuRead(PMU_ADDR_FET2CNT,&pmustatus,1);
				iDPrintf("FET2CNT=0x%02x\r\n",pmustatus);
				break;
			case 'e':
				return;
			default:
				break;
		}
	}
}
void TestPmuSetRegister(int mode)
{
	unsigned char addr;
	unsigned char value;
	iDPrintf("Input Register Addr:\r\n");
	addr = s_getkey();
	iDPrintf("Addr:0x%02x\r\n",addr);
	if (mode)
	{
		iDPrintf("Input Register Value:\r\n");
		value = s_getkey();
		iDPrintf("Value:0x%02x\r\n",value);
		s_PmuWrite(addr,&value,1);
		s_PmuRead(addr,&value,1);
		iDPrintf("Set Ok! Addr:0x%02x,Value:0x%02x\r\n",addr,value);
	}
	else
	{
		s_PmuRead(addr,&value,1);
		iDPrintf("Addr:0x%02x,Value:0x%02x\r\n",addr,value);
	}
	
	
}
void pmustatus_isr(void)
{
	unsigned char status;
	unsigned char pmustatus;
	s_PmuRead(PMU_ADDR_CHGIR1,&pmustatus,1);
	iDPrintf("CHGIR1=0x%02x\r\n",pmustatus);
	if (pmustatus & (1<<7))
	{
		iDPrintf("Battery Over Voltage Intr\r\n");
	}
	if (pmustatus & (1<<6))
	{
		iDPrintf("Adapter Over voltage Intr\r\n");
	}
	if (pmustatus & (1<<5))
	{
		iDPrintf("No Battery Detect Intr\r\n");
	}
	if (pmustatus & (1<<2))
	{
		iDPrintf("Battery abnormal temperature Intr\r\n");
	}
	if (pmustatus & (1<<1))
	{
		iDPrintf("SW1 or SW2 in charger Intr\r\n");
	}
	if (pmustatus & (1<<0))
	{
		iDPrintf("Adapter insert & remove Intr\r\n");
	}
	s_PmuRead(PMU_ADDR_CHGMONI,&status,1);
	iDPrintf("CHGMONI=0x%02x\r\n",status);
	s_PmuRead(PMU_ADDR_CHGEN1,&status,1);
	iDPrintf("CHGEN1=0x%02x\r\n",status);
	s_PmuRead(PMU_ADDR_CHGSTATE,&status,1);
	iDPrintf("CHGSTATE=0x%02x\r\n",status);
	pmustatus = 0;
	s_PmuWrite(PMU_ADDR_CHGIR1,&pmustatus,1);
}
#define PMU_INTB   GPIOB,5 //for PMU
void TestPmu(void)
{
	unsigned char ch;
	unsigned char pmustatus;
	unsigned char status;
	uint puiData;
	while(1)
	{
		iDPrintf("=========================\r\n");
		iDPrintf("1-Interrupt Config\r\n");
		iDPrintf("2-Interrupt Status\r\n");
		iDPrintf("3-Config Charge Time\r\n");
		iDPrintf("4-Config Charge Complete Current\r\n");
		iDPrintf("5-Set CRCC\r\n");
		iDPrintf("6-Set ILIM\r\n");
		iDPrintf("7-Config Charge Current\r\n");
		iDPrintf("8-Set Register\r\n");
		iDPrintf("9-Read Register\r\n");
		iDPrintf("e-return\r\n");
		iDPrintf("=========================\r\n");
		ch = s_getkey();
		switch(ch)
		{
			case '1':
				TestPmuIntrConfig();
				break;
			case '2':
				gpio_set_pin_type(PMU_INTB,GPIO_INPUT);
				s_setShareIRQHandler(PMU_INTB,INTTYPE_FALL,(void *)pmustatus_isr);
				gpio_set_pin_interrupt(PMU_INTB,1);
				while(1)
				{
					if (PortRxPoolCheck(0) == 1)
					{
						if (s_getkey() == 'e')
						{
							gpio_set_pin_interrupt(PMU_INTB,0);
							break;
						}
					}
				}
				break;
			case '3':
				TestPmuSetChargeTime();
				break;
			case '4':
				TestPmuSetChargeCompleteCurrent();
				break;
			case '5':
				TestPmuSetCRCC();
				break;
			case '6':
				TestPmuSetILIM();
				break;
			case '7':
				TestPmuSetChargeCurrent();
				break;
			case '8':
				TestPmuSetRegister(1);
				break;
			case '9':
				TestPmuSetRegister(0);
				break;
			case 'e':
				return;
			default:
				break;
		}
	}
}
#endif

