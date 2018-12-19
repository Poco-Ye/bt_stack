#ifndef AS3911_REGS_CONF_H
#define AS3911_REGS_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

struct ST_As3911RegVal 
{  
    unsigned char index;
    unsigned char value;
};

void As3911Iso14443TypeAInit( struct ST_PCDINFO* );
void As3911Iso14443TypeAConf( struct ST_PCDINFO* );

void As3911Iso14443TypeBInit( struct ST_PCDINFO* );
void As3911Iso14443TypeBConf( struct ST_PCDINFO* );

void As3911Jisx6319_4Init( struct ST_PCDINFO* );
void As3911Jisx6319_4Conf( struct ST_PCDINFO* );

int As3911SetDefaultConfigVal( int iType );
int As3911SetRegisterConfigVal( int iType, unsigned char ucReg, unsigned char ucVal );
int As3911GetRegisterConfigVal( int iType, unsigned char ucReg );


#ifdef __cplusplus
}

#endif

#endif

