#include "typedefs.h"
#include "sdio.h"
#include "bcmdefs.h"
#include "bcmendian.h"
#include "bcmutils.h"
#include "osl_ext.h"
#include "generic_osl.h"
#include <stdlib.h>
#include "pkt_lbuf.h"
#include <stdarg.h>
#include "..\bcm5892-sdio.h"

#define MAX_SEM_COUNT           15
#define SEM_OCCUPIED_MASK       0x80000000

volatile int giSemList[MAX_SEM_COUNT] = {0};

int AHC_OS_CreateSem(int ubSemValue)
{
    int i;

    for(i=0; i<MAX_SEM_COUNT; i++)
    {
        if(giSemList[i] == 0)
            break;
    }

    if(i >= MAX_SEM_COUNT)
        return -1;

    giSemList[i] = ubSemValue | SEM_OCCUPIED_MASK;
    return i;
}

int AHC_OS_DeleteSem(int ulSemId)
{
    if(ulSemId >= MAX_SEM_COUNT) return 0;

    giSemList[ulSemId] = 0;
    return 0;
}

int AHC_OS_CheckSem(int ulSemId)
{
    if(ulSemId >= MAX_SEM_COUNT) return -1;

    ASSERT(giSemList[ulSemId] & SEM_OCCUPIED_MASK);

    return (giSemList[ulSemId] & ~SEM_OCCUPIED_MASK) ? 1 : 0;
}

int AHC_OS_AcquireSem(int ulSemId, uint ulTimeout)
{
    int t0, flag;

    if(ulSemId >= MAX_SEM_COUNT) return -1;

    ASSERT(giSemList[ulSemId] & SEM_OCCUPIED_MASK);

    t0 = s_Get10MsTimerCount();
	while (1)
    {
        irq_save_asm(&flag);
        if(giSemList[ulSemId] & ~SEM_OCCUPIED_MASK) break;

        if(!IsMainCodeTask()) 
        {
            if(get_cur_task() == 10)
            {
                while(1)
                {
                    prints1("error: fatal error, %s:%d\r\n", __FUNCTION__, __LINE__);
                    DelayMs(1000);
                }
            }

            irq_restore_asm(&flag);
            OsSleep(1);
            if(ulTimeout == -1) 
                continue;
        }
        else
        {
            irq_restore_asm(&flag);
            DelayUs(500);
            if(ulTimeout == -1)
                continue;
        }

        if((s_Get10MsTimerCount()-t0) > ulTimeout/10) 
        {
            return 1;
        }
	}

    giSemList[ulSemId]--;
    irq_restore_asm(&flag);
    return 0;
}

uchar AHC_OS_ReleaseSem(int ulSemId)
{
    int flag;

    if(ulSemId >= MAX_SEM_COUNT) return 1;

    ASSERT(giSemList[ulSemId] & SEM_OCCUPIED_MASK);
    irq_save_asm(&flag);
    giSemList[ulSemId]++;
    irq_restore_asm(&flag);
    return 0;
}

int AHC_SDIO_ReadMultiReg(uchar byFuncNum, uchar byBlkMode, uchar byOpCode, uint uiRegAddr, uint uiCount, uint uiBlkSize, uchar *pbyDst)
{
    int ret;

    if(byBlkMode == SD_IO_BYTE_MODE)
        ret = wwd_bus_sdio_cmd53(BUS_READ, byFuncNum, SDIO_BYTE_MODE, byOpCode, uiRegAddr, uiCount, pbyDst, RESPONSE_NEEDED, NULL);
    else
        ret = wwd_bus_sdio_cmd53(BUS_READ, byFuncNum, SDIO_BLOCK_MODE, byOpCode, uiRegAddr, uiCount * uiBlkSize, pbyDst, RESPONSE_NEEDED, NULL);
    
    return ret;
}

int AHC_SDIO_WriteMultiReg(uchar byFuncNum, uchar byBlkMode, uchar byOpCode, uint uiRegAddr, uint uiCount, uint  uiBlkSize, uchar *pbySrc)
{
    int ret;

    if(byBlkMode == SD_IO_BYTE_MODE)
        ret = wwd_bus_sdio_cmd53(BUS_WRITE, byFuncNum, SDIO_BYTE_MODE, byOpCode, uiRegAddr, uiCount, pbySrc, RESPONSE_NEEDED, NULL);
    else
        ret = wwd_bus_sdio_cmd53(BUS_WRITE, byFuncNum, SDIO_BLOCK_MODE, byOpCode, uiRegAddr, uiCount * uiBlkSize, pbySrc, RESPONSE_NEEDED, NULL);

    return ret;
}

int AHC_SDIO_ReadReg(uchar byFuncNum, uint uiRegAddr, uchar *pbyDst)
{
    return wwd_bus_sdio_read_register_value(byFuncNum, uiRegAddr, 1, pbyDst);
}

int AHC_SDIO_WriteReg(uchar byFuncNum, uint uiRegAddr, uchar bySrc)
{
    return wwd_bus_write_register_value(byFuncNum, uiRegAddr, 1, bySrc);
}

osl_ext_status_t osl_ext_sem_create(char *name, int init_cnt, osl_ext_sem_t *sem)
{
	int semid;

	memset(sem, 0, sizeof(osl_ext_sem_t));
	semid = AHC_OS_CreateSem(init_cnt);

	if(semid == 0xFF || semid == 0xFE)
		return OSL_EXT_ERROR;

	*sem = semid;
	
	return OSL_EXT_SUCCESS;
}

osl_ext_status_t osl_ext_sem_delete(osl_ext_sem_t *sem)
{
	if(AHC_OS_DeleteSem(*sem) == 0)
		return OSL_EXT_SUCCESS;

	return OSL_EXT_ERROR;
}

osl_ext_status_t osl_ext_sem_give(osl_ext_sem_t *sem)
{
	if(AHC_OS_ReleaseSem(*sem) == 0)
		return OSL_EXT_SUCCESS;

	return OSL_EXT_ERROR;
}

osl_ext_status_t osl_ext_sem_take(osl_ext_sem_t *sem, osl_ext_time_ms_t timeout_msec)
{
	uchar 	status;

    status = AHC_OS_AcquireSem(*sem, timeout_msec);
	if(status == 0)
		return OSL_EXT_SUCCESS;
	else if (status == 1)
		return OSL_EXT_TIMEOUT;

	return OSL_EXT_ERROR;
}

osl_ext_status_t osl_ext_mutex_check(osl_ext_sem_t *sem)
{
    return AHC_OS_CheckSem(*sem);
}

osl_ext_status_t osl_ext_mutex_create(char *name, osl_ext_mutex_t *mutex)
{
	return (osl_ext_sem_create(name, 1, mutex));
}

osl_ext_status_t osl_ext_mutex_delete(osl_ext_mutex_t *mutex)
{
	return (osl_ext_sem_delete(mutex));
}

osl_ext_status_t osl_ext_mutex_acquire(osl_ext_mutex_t *mutex, osl_ext_time_ms_t timeout_msec)
{
	return (osl_ext_sem_take(mutex, timeout_msec));
}

osl_ext_status_t osl_ext_mutex_release(osl_ext_mutex_t *mutex)
{
	return (osl_ext_sem_give(mutex));
}

void osl_ext_printf(char* fmt, ...)
{
    char dbg_str[256];
    va_list args;

    va_start(args, fmt);
    vsprintf(dbg_str, fmt, args);
    va_end(args);

	prints1("%s\r", dbg_str);
}

bool osl_ext_sdio_write_reg(uchar byFuncNum, uint uiRegAddr, uchar bySrc)
{
	return AHC_SDIO_WriteReg(byFuncNum, uiRegAddr, bySrc) ? FALSE : TRUE;
}

bool osl_ext_sdio_read_reg(uchar byFuncNum, uint uiRegAddr, uchar *pbyDst)
{
	return AHC_SDIO_ReadReg(byFuncNum, uiRegAddr, pbyDst) ? FALSE : TRUE;
}

bool osl_ext_sdio_write_multi_reg(uchar byFuncNum,uchar byBlkMode,uchar byOpCode,uint uiRegAddr,uint uiCount,uint uiBlkSize,uchar * pbySrc)
{
	return AHC_SDIO_WriteMultiReg(byFuncNum, byBlkMode, byOpCode, uiRegAddr, uiCount, uiBlkSize, pbySrc) ? FALSE : TRUE;
}

bool osl_ext_sdio_read_multi_reg(uchar byFuncNum,uchar byBlkMode,uchar byOpCode,uint uiRegAddr,uint uiCount,uint uiBlkSize,uchar * pbyDst)
{
	return AHC_SDIO_ReadMultiReg(byFuncNum, byBlkMode, byOpCode, uiRegAddr, uiCount, uiBlkSize, pbyDst) ? FALSE : TRUE;
}	

/* Micro-second resolution not supported. Convert to milli-seconds. */
void osl_delay(uint usec)
{
	uint msec;

	if (usec == 0) return;
	if (usec < 1000) msec = 1;
	else msec = usec / 1000;

    DelayMs(msec);
    return ;
}

