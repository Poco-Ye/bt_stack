/*****************************************************************
2007-11-13
修改人:杨瑞彬
修改日志:
修改MSR_TrackInfo结构体
*****************************************************************/
#ifndef _ENMAGCARD_H_
#define	_ENMAGCARD_H_

void  encryptmag_init(void);

// API接口
void  encryptmag_reset(void);
void  encryptmag_open(void);
void  encryptmag_close(void);
void  encryptmag_read_rawdata(void);
void  encryptmag_swiped(void);

#endif
