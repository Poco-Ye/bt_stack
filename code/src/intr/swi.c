#include "base.h"
#include "..\dll\dll.h"

extern void ProcFunc(void);
extern void ClearIDCache(void);
static PARAM_STRUCT * s_app_param = (PARAM_STRUCT *)DLL_PARAM_ADDR;
extern PARAM_STRUCT  *glParam;

void SWIHandler(int swi_num,uint param_addr)
{
	switch(swi_num)
	{
	case 0:
        glParam = (PARAM_STRUCT *)DLL_PARAM_ADDR;
		ProcFunc();
		break;
	case 1:
		break;
	case 2:
		ClearIDCache();
		break;
    case 3:
        glParam = s_app_param;
        ProcFunc();
        break;
    case 4:
        s_app_param = (PARAM_STRUCT *)param_addr;
        s_app_param->u_int1 = 0x4000000;  /*Memory Size*/
        break;
    case 5:
        glParam = (PARAM_STRUCT *)0x2DF00000;  /*SLIM 调用monitor api*/
		ProcFunc();
        break;

    case 0x100:
        glParam = (PARAM_STRUCT *)0x2DF00000;  /*SLIM 调用baseso api*/
        s_BaseSoEntry(glParam);
        break;
	default:
		break;
	}
}

