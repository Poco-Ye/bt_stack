/**
 * =============================================================================
 * Configure iccard device resource in this document
 * This document must be portable when change hardware platform.
 *
 * Author            : xuwt
 * Date              : 2014-06-29
 * Hardware platform : BCM5892
 * Email             : xuwt@paxsz.com
 * =============================================================================
 */
#include "base.h"
#include "icc_device_configure.h"
#include "icc_config.h"

#define INVALID_GPIO (0xffff)

static const SCI_NCN8025_INTERFACE sci_none_interface = {
	.vsel0_pin = -1, 		//8025_sel0 	
	.vsel0_port = INVALID_GPIO,
	
	.vsel1_pin = -1,			//8025_sel1	
	.vsel1_port = INVALID_GPIO,
	
	.ncmdvcc_pin = -1,		//8025_icc_vcc
	.ncmdvcc_port = INVALID_GPIO,
	
	.aux1_pin = -1,           //icc_c4
	.aux1_port = INVALID_GPIO,
	
	.aux2_pin = -1,           //icc_c8
	.aux2_port = INVALID_GPIO,
	
	.rstin_pin = -1,			 //icc_rst
	.rstin_port = INVALID_GPIO,

	.io_pin = -1,
	.io_port = INVALID_GPIO,
};



static const SCI_NCN8025_INTERFACE sci_ncn8025_interface_sxxx = {
	.vsel0_pin = 0, 		//8025_sel0 	
	.vsel0_port = GPIOC,
	
	.vsel1_pin = 1,			//8025_sel1	
	.vsel1_port = GPIOC,
	
	.ncmdvcc_pin = 15,		//8025_icc_vcc
	.ncmdvcc_port = GPIOA,
	
	.aux1_pin = 14,           //icc_c4
	.aux1_port = GPIOA,
	
	.aux2_pin = 21,           //icc_c8
	.aux2_port = GPIOA,
	
	.rstin_pin = 16,			 //icc_rst
	.rstin_port = GPIOA,

	.io_pin = 12,
	.io_port = GPIOA,
};

static const SCI_NCN8025_INTERFACE sci_ncn8025_interface_sxx = {
	/* ncn8025_interface */
	.vsel0_pin = 2, 		//8025_sel0 	
	.vsel0_port = GPIOA,
	
	.vsel1_pin = 3,			//8025_sel1	
	.vsel1_port = GPIOA,
	
	.ncmdvcc_pin = 15,		//8025_icc_vcc
	.ncmdvcc_port = GPIOA,
	
	.aux1_pin = 14,           //icc_c4
	.aux1_port = GPIOA,
	
	.aux2_pin = 21,           //icc_c8
	.aux2_port = GPIOA,
	
	.rstin_pin = 16,			 //icc_rst
	.rstin_port = GPIOA,

	.io_pin = 12,
	.io_port = GPIOA,
	
};



static const SCI_SAM_INTERFACE sci_sam_interface_S910 = {
	/* sam_interface */
	.sam1_pwr_pin		= 19,
	.sam1_pwr_enable 	= 1,
	.sam1_pwr_port = GPIOA,

	.sam2_pwr_pin		= -1,
	.sam2_pwr_enable 	= 1,
	.sam2_pwr_port = INVALID_GPIO,

	.sam3_pwr_pin		= -1,
	.sam3_pwr_enable 	= 1,
	.sam3_pwr_port = INVALID_GPIO,

	.sam4_pwr_pin		= -1,
	.sam4_pwr_enable 	= 1,
	.sam4_pwr_port = INVALID_GPIO,

	.sam_sel0_pin		= -1,
	.sam_sel1_pin		= -1,
	.sam_sel0_port = INVALID_GPIO,
	.sam_sel1_port = INVALID_GPIO,

	.sam_sel_en_pin		= -1,
	.sam_sel_en_enable	= 0,
	.sam_sel_en_port = INVALID_GPIO,

	.sam_rst_pin		= 20,
	.sam_rst_enable	= 1,
	.sam_rst_port = GPIOA,
};


static const SCI_SAM_INTERFACE sci_sam_interface_sxxx = {
	/* sam_interface */
	.sam1_pwr_pin		= 28,
	.sam1_pwr_enable 	= 1,
	.sam1_pwr_port = GPIOB,

	.sam2_pwr_pin		= 29,
	.sam2_pwr_enable 	= 1,
	.sam2_pwr_port = GPIOB,

	.sam3_pwr_pin		= 30,
	.sam3_pwr_enable 	= 1,
	.sam3_pwr_port = GPIOB,

	.sam4_pwr_pin		= -1,
	.sam4_pwr_enable 	= 0,
	.sam4_pwr_port = -1,

	.sam_sel0_pin		= 26,
	.sam_sel1_pin		= 27,
	.sam_sel0_port = GPIOB,
	.sam_sel1_port = GPIOB,

	.sam_sel_en_pin		= -1,
	.sam_sel_en_enable	= 0,
	.sam_sel_en_port = -1,

	.sam_rst_pin		= 24,
	.sam_rst_enable	= 1,
	.sam_rst_port = GPIOB,
};


static const SCI_SAM_INTERFACE sci_sam_interface_sxx = {
	/* sam_interface */
	.sam1_pwr_pin		= 15,
	.sam1_pwr_enable 	= 1,
	.sam1_pwr_port = GPIOC,

	.sam2_pwr_pin		= 16,
	.sam2_pwr_enable 	= 1,
	.sam2_pwr_port = GPIOC,

	.sam3_pwr_pin		= 17,
	.sam3_pwr_enable 	= 1,
	.sam3_pwr_port = GPIOC,

	.sam4_pwr_pin		= 8,
	.sam4_pwr_enable 	= 1,
	.sam4_pwr_port = GPIOC,

	.sam_sel0_pin		= 18,
	.sam_sel1_pin		= 19,
	.sam_sel0_port = GPIOC,
	.sam_sel1_port = GPIOC,

	.sam_sel_en_pin		= 7,
	.sam_sel_en_enable	= 0,
	.sam_sel_en_port = GPIOC,

	.sam_rst_pin		= 14,
	.sam_rst_enable	= 1,
	.sam_rst_port = GPIOC,
};

static const SCI_SAM_INTERFACE sci_sam_interface_mt30 = {
	/* sam_interface */
	.sam1_pwr_pin		= 26,
	.sam1_pwr_enable 	= 1,
	.sam1_pwr_port = GPIOB,

	.sam2_pwr_pin		= 27,
	.sam2_pwr_enable 	= 1,
	.sam2_pwr_port = GPIOB,

	.sam3_pwr_pin		= 28,
	.sam3_pwr_enable 	= 1,
	.sam3_pwr_port = GPIOB,

	.sam4_pwr_pin		= 29,
	.sam4_pwr_enable 	= 1,
	.sam4_pwr_port = GPIOB,

	.sam_sel0_pin		= 30,
	.sam_sel1_pin		= 31,
	.sam_sel0_port = GPIOB,
	.sam_sel1_port = GPIOB,

	.sam_sel_en_pin		= 17,
	.sam_sel_en_enable	= 0,
	.sam_sel_en_port = GPIOD,

	.sam_rst_pin		= 25,
	.sam_rst_enable	= 1,
	.sam_rst_port = GPIOB,
};


static const SCI_TDA8026_INTERFACE sci_tda8026_interface = {
	.sdwn_pd_pin = 21,
	.sdwn_pd_enable = 1,
	.sdwn_pd_port = GPIOA,

	.intpd_pin = 17,
	.intpd_port = GPIOA,

	.i2c_scl_pin = 14,
	.i2c_scl_port = GPIOA,

	.i2c_sda_pin = 15,
	.i2c_sda_port = GPIOA,

	.i2c_base_addr = 0xC9000,
};

/*add by wanls 20160810*/
static const SCI_TDA8026_INTERFACE sci_second_tda8026_interface = {
	.sdwn_pd_pin = 27,
	.sdwn_pd_enable = 1,
	.sdwn_pd_port = GPIOB,

	.intpd_pin = 26,
	.intpd_port = GPIOB,

	.i2c_scl_pin = 24,
	.i2c_scl_port = GPIOB,

	.i2c_sda_pin = 25,
	.i2c_sda_port = GPIOB,

	.i2c_base_addr = 0x0,
};
/*add*/
 
static const SCI_BCM5892_CONTROLLER sci_bcm5892_interface = {
	.sci_user_io = 12,
	.sci_sam_io = 13,
	.sci_user_clk = 18,
	.sci_sam_clk = 22,
	.sci_user_base = 0xCE000,
	.sci_sam_base = 0xCF000,
	.sci_ex_intr = 17,
};







SCI_RESOURCE sci_device_resource;


/*add by wanls 20160810*/
int GetSamSlotNum(unsigned char * ucSamSlotNum)
{
	int ret = 0;
	unsigned char buffer[5];
	
	memset(buffer, 0x00, sizeof(buffer));
	ret = ReadCfgInfo("SAM_NUM", buffer);
	if(ret < 0)
		return ret;

	*ucSamSlotNum = (unsigned char)atoi(buffer);

	return 0;
}
/*add end*/

int sci_resource_configure()
{
	unsigned char context[20];
	unsigned char ucSamSlotNum = 0;
	int machine_type;

	
	sci_device_resource.tda8026_interface = sci_tda8026_interface;
	sci_device_resource.sci_controller = sci_bcm5892_interface;
	/*add by wanls 20160810*/
	sci_device_resource.second_tda8026_interface = sci_second_tda8026_interface;
	/*add end*/


	machine_type = get_machine_type();
	

	switch(machine_type)
    {
		case S300:
			sci_device_resource.user_slot_num = 1;
			sci_device_resource.sam_slot_num = 3;
			/*change by wanls 20160811*/
			if(GetSamSlotNum(&ucSamSlotNum) == 0)
			{
			    sci_device_resource.sam_slot_num = ucSamSlotNum;
			}
			/*add end*/
			sci_device_resource.ncn8025_interface = sci_ncn8025_interface_sxxx;
			sci_device_resource.sam_interface = sci_sam_interface_sxxx;
		break;

		case S900:
			if (ReadCfgInfo("DUAL_SIM", context)>0)
			{
				sci_device_resource.user_slot_num = 1;
				sci_device_resource.sam_slot_num = 2;
			}
			else
			{
				sci_device_resource.user_slot_num = 1;
				sci_device_resource.sam_slot_num = 3;
			}
			sci_device_resource.ncn8025_interface = sci_ncn8025_interface_sxxx;
			sci_device_resource.sam_interface = sci_sam_interface_sxxx;
		break;

		case S800:
			if(ReadCfgInfo("DUAL_SIM", context)>0)
			{
				sci_device_resource.user_slot_num = 1;
				sci_device_resource.sam_slot_num = 1;
			}
			else if ((ReadCfgInfo("CDMA", context)>0) ||
			    	 (ReadCfgInfo("GPRS", context)>0)     ||
					 (ReadCfgInfo("WCDMA", context)>0)    )
			{
				sci_device_resource.user_slot_num = 1;
				sci_device_resource.sam_slot_num = 2;
			}
			else
			{
				sci_device_resource.user_slot_num = 1;
				sci_device_resource.sam_slot_num = 3;
			}
			sci_device_resource.ncn8025_interface = sci_ncn8025_interface_sxxx;
			sci_device_resource.sam_interface = sci_sam_interface_sxxx;
		break;

		case S80:
			if(ReadCfgInfo("DUAL_SIM", context)>0)
			{
				sci_device_resource.user_slot_num = 1;
				sci_device_resource.sam_slot_num = 2;
			}
			else
			{
				sci_device_resource.user_slot_num = 1;
				sci_device_resource.sam_slot_num = 3;
			}
			sci_device_resource.ncn8025_interface = sci_ncn8025_interface_sxx;
			sci_device_resource.sam_interface = sci_sam_interface_sxx;
			break;
		case D200:
			if ((ReadCfgInfo("CDMA", context)>0)	||
				(ReadCfgInfo("GPRS", context)>0)	||
				(ReadCfgInfo("WCDMA", context)>0))
			{
				sci_device_resource.user_slot_num = 1;
				sci_device_resource.sam_slot_num = 1;
			}
			else
			{
				sci_device_resource.user_slot_num = 1;
				sci_device_resource.sam_slot_num = 2;
			}
			sci_device_resource.ncn8025_interface = sci_ncn8025_interface_sxx;
			sci_device_resource.sam_interface = sci_sam_interface_sxx;
			break;
		case D210:
			if(ReadCfgInfo("DUAL_SIM", context)>0)
			{
				sci_device_resource.user_slot_num = 1;
				sci_device_resource.sam_slot_num = 1;
			}
			else if ((ReadCfgInfo("CDMA", context)>0) ||
			    	 (ReadCfgInfo("GPRS", context)>0)     ||
					 (ReadCfgInfo("WCDMA", context)>0)    )
			{
				sci_device_resource.user_slot_num = 1;
				sci_device_resource.sam_slot_num = 2;
			}
			else
			{
				sci_device_resource.user_slot_num = 1;
				sci_device_resource.sam_slot_num = 3;
			}
			sci_device_resource.ncn8025_interface = sci_ncn8025_interface_sxxx;
			sci_device_resource.sam_interface = sci_sam_interface_sxxx;
			break;
		case S90:
			if(ReadCfgInfo("DUAL_SIM", context)>0)
			{
				sci_device_resource.sam_slot_num = 1;
			}
			else
			{
				sci_device_resource.sam_slot_num = 2;
			}

			sci_device_resource.user_slot_num = 1;
			sci_device_resource.ncn8025_interface = sci_ncn8025_interface_sxx;
			sci_device_resource.sam_interface = sci_sam_interface_sxx;
			break;

		case SP30:
			sci_device_resource.user_slot_num = 1;
			sci_device_resource.sam_slot_num = 3;
			sci_device_resource.ncn8025_interface = sci_ncn8025_interface_sxx;
			sci_device_resource.sam_interface = sci_sam_interface_sxx;
		break;

		case S58:
			if((ReadCfgInfo("CDMA", context)>0) ||
			   (ReadCfgInfo("GPRS", context)>0) ||
			   (ReadCfgInfo("WCDMA", context)>0) ||
			   (ReadCfgInfo("DUAL_SIM", context)>0))
			{
				sci_device_resource.user_slot_num = 1;
				sci_device_resource.sam_slot_num = 2;
			}
			else
			{
				sci_device_resource.user_slot_num = 1;
				sci_device_resource.sam_slot_num = 3;
			}

			sci_device_resource.ncn8025_interface = sci_ncn8025_interface_sxx;
			sci_device_resource.sam_interface = sci_sam_interface_sxx;
		case S500:
			if (ReadCfgInfo("DUAL_SIM", context)>0)
			{
			    sci_device_resource.user_slot_num = 1;
				sci_device_resource.sam_slot_num = 1;
			}
			else if((ReadCfgInfo("CDMA", context)>0) ||
			   (ReadCfgInfo("GPRS", context)>0) ||
			   (ReadCfgInfo("WCDMA", context)>0))
			{
				sci_device_resource.user_slot_num = 1;
				sci_device_resource.sam_slot_num = 2;
			}
			else
			{
				sci_device_resource.user_slot_num = 1;
				sci_device_resource.sam_slot_num = 3;
			}

			sci_device_resource.ncn8025_interface = sci_ncn8025_interface_sxx;
			sci_device_resource.sam_interface = sci_sam_interface_sxx;
		break;

		case S78:
			if((ReadCfgInfo("CDMA", context)>0) ||
			   (ReadCfgInfo("GPRS", context)>0) ||
			   (ReadCfgInfo("WCDMA", context)>0) ||
			   (ReadCfgInfo("DUAL_SIM", context)>0))
			{
				sci_device_resource.user_slot_num = 1;
				sci_device_resource.sam_slot_num = 3;
			}
			else
			{
				sci_device_resource.user_slot_num = 1;
				sci_device_resource.sam_slot_num = 4;
			}
			sci_device_resource.ncn8025_interface = sci_ncn8025_interface_sxx;
			sci_device_resource.sam_interface = sci_sam_interface_sxx;
		break;

		case S60:
			sci_device_resource.user_slot_num = 1;
			sci_device_resource.sam_slot_num = 3;
			sci_device_resource.ncn8025_interface = sci_ncn8025_interface_sxx;
			sci_device_resource.sam_interface = sci_sam_interface_sxx;
		break;

		case MT30:
			if(ReadCfgInfo("DUAL_SIM", context)>0)
			{
				sci_device_resource.user_slot_num = 1;
				sci_device_resource.sam_slot_num = 2;
			}
			else
			{
				sci_device_resource.user_slot_num = 1;
				sci_device_resource.sam_slot_num = 4;
			}
			sci_device_resource.ncn8025_interface = sci_ncn8025_interface_sxx;
			sci_device_resource.sam_interface = sci_sam_interface_mt30;
        break;

		case S910:
			sci_device_resource.user_slot_num = 1;
			sci_device_resource.sam_slot_num = 1;
			sci_device_resource.ncn8025_interface = sci_ncn8025_interface_sxx;
			sci_device_resource.sam_interface = sci_sam_interface_S910;
		break;

        default:
            ScrCls();
            ScrPrint(0, 0, 0, "ICC Card    ");
            ScrPrint(0, 3, 0, "Invalid Pos Type");
            getkey();
            return -1;
        break;
    }

	sci_device_resource.icc_interfaceIC_type = NCN8025;
	if(ReadCfgInfo("IC_CARD", context)>0)
	{
		sci_device_resource.icc_interfaceIC_type = atoi(context);
		if(sci_device_resource.icc_interfaceIC_type == 0) //no usercard
		{
			sci_device_resource.user_slot_num = 0;
			sci_device_resource.icc_interfaceIC_type = NCN8025;//此时走NCN8025配置分支，但无效掉所有的口线
			sci_device_resource.ncn8025_interface = sci_none_interface;
		}
	}

	if ((machine_type == S300) && (sci_device_resource.icc_interfaceIC_type == NCN8025))
	{
		if(GetVolt()>=5500)
        {    
        	gpio_set_pin_type(GPIOB, 31, GPIO_OUTPUT);
        	gpio_set_pin_val(GPIOB,31, 0);//power off
            return 0XFF; 
        }
		gpio_set_pin_type(GPIOB, 31, GPIO_OUTPUT);
		gpio_set_pin_val(GPIOB,31, 1);//power open
	}

	return 0;
}



void insert_card_action(void)
{
	int machine_type;

	machine_type = get_machine_type();
	
    if(get_scrbacklightmode() < 2) ScrBackLight(1);
	if(get_kblightmode() <2) kblight(1);
	// add by aihu 对于硬件方面的差异加上机型判断
	if(D200 == machine_type)	
	{
		if(get_touchkeylockmode()<2)s_KbLock(1); //For D200 Touch key
	}

}


