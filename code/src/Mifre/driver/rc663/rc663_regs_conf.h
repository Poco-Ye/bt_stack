#ifndef RC663_REGS_CONF_H
#define RC663_REGS_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

struct ST_Rc663RegVal 
{  
    unsigned char index;
    unsigned char value;
};

void Rc663Iso14443TypeAInit( struct ST_PCDINFO* );
void Rc663Iso14443TypeAConf( struct ST_PCDINFO* );

void Rc663Iso14443TypeBInit( struct ST_PCDINFO* );
void Rc663Iso14443TypeBConf( struct ST_PCDINFO* );

void Rc663Jisx6319_4Init( struct ST_PCDINFO* );
void Rc663Jisx6319_4Conf( struct ST_PCDINFO* );

int Rc663SetDefaultConfigVal( int iType );
int Rc663SetRegisterConfigVal( int iType, unsigned char ucReg, unsigned char ucVal );
int Rc663GetRegisterConfigVal( int iType, unsigned char ucReg );

#ifdef __cplusplus
}
#endif

#endif
 


