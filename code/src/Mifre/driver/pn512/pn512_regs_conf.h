#ifndef PN512_REGS_CONF_H
#define PN512_REGS_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

struct ST_Pn512RegVal 
{  
    unsigned char index;
    unsigned char value;
};

void Pn512Iso14443TypeAInit( struct ST_PCDINFO* );
void Pn512Iso14443TypeAConf( struct ST_PCDINFO* );

void Pn512Iso14443TypeBInit( struct ST_PCDINFO* );
void Pn512Iso14443TypeBConf( struct ST_PCDINFO* );

void Pn512Jisx6319_4Init( struct ST_PCDINFO* );
void Pn512Jisx6319_4Conf( struct ST_PCDINFO* );

int Pn512SetDefaultConfigVal( int iType );
int Pn512SetRegisterConfigVal( int iType, unsigned char ucReg, unsigned char ucVal );
int Pn512GetRegisterConfigVal( int iType, unsigned char ucReg );

#ifdef __cplusplus
}
#endif

#endif
 


