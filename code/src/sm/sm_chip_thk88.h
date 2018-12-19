#ifndef SM_CHIP_THK88__H
#define SM_CHIP_THK88__H


typedef struct _AtPara
{
	unsigned char flag; //1=com changed 2=session changed 4=eth changed
    unsigned char id; //which com id?
    unsigned char atmode;//0=disable(data mode)  1=direct enable 2=ctrl break, 3=command code
}T_AtPara;


typedef void (*AtAction)(T_AtPara *para);

typedef struct AT_COMMD_LIST
{
	char * at_command;
	AtAction at_action;
}T_AT_COMMD_LIST;

///////////////////////////////////////////////////////////////////////

#define RECV_ACK_TIME 2000 // 2s
//#define RECV_WRITE_TIME 10000
#define RECV_DATA_TIME 20000 // 20s
#define RECV_PAIRKEY_TIME 20000 // 20s

/* 注意该部分定义与华大的代码定义要保持一致。开始: */
/*BEG*/
#define SPI_BEGIN_DATA  	0xB0

/*END*/
#define SPI_END_DATA  		0xE0

/*CMD*/
#define CMD_SHAKE_HAND  	0x00
#define CMD_WRITE_KEY  	0x01
#define CMD_ENCRYPT	   	0x02
#define CMD_DECRYPT	   	0x03
#define CMD_GET_DATA    	0x04
#define CMD_GEN_PAIRKEY    	0x05
#define CMD_READ_KEY  		0x06
#define CMD_SIGN    			0x07
#define CMD_VERIFY  			0x08
#define CMD_HASH  			0x09
#define CMD_READ_VERSION 	0x0A
#define CMD_ACK_SUCCESS	0x0B
#define CMD_ACK_FAILED		0x0C
#define CMD_QUERY			0x0D
#define CMD_POWERON		0x0E
#define CMD_POWEROFF		0x0F
#define CMD_SEND_DATA  	0xA0
#define CMD_UNSHAKE_HAND 	0xBB

/*Key type*/
#define KEY_TYPE_NULL  		0x00
#define KEY_TYPE_RSA  		0x01
#define KEY_TYPE_PED  		0x02
#define KEY_TYPE_GM_SM2	0x03
#define KEY_TYPE_GM_SM3	0x04
#define KEY_TYPE_GM_SM4	0x05
#define KEY_TYPE_END		0x06

/*State*/
#define NO_ERR 				0x00
#define CMD_ERR 			0x01
#define TYPE_ERR 			0x02
#define LENGTH_ERR 			0x03
#define DATA_LOST_ERR 		0x04
#define BUSY_ERR 			0x05
#define NO_DATA_ERR 		0x06
#define READ_KEY_ERR 		0x07
#define SIGN_ERR 			0x08
#define VERIFY_ERR 			0x09
#define HASH_ERR 			0x0A
#define UNKNOWN_ERR 		0xA0

//--error code define of API call
#define USB_ERR_OPERATION_SUCCESS    0
#define USB_ERR_NOTOPEN    3
#define USB_ERR_BUF        4
#define USB_ERR_NOTFREE    5
#define USB_ERR_TIMEOUT    0xff
#define USB_ERR_OPERATION_FAILED        0xdd

#define GM_CMD_TYPE_BASE			(0x10)
#define GM_CMD_BASE_SHAKE_HAND			(0x01)
#define GM_CMD_BASE_UNSHAKE_HAND		(0x02)
#define GM_CMD_BASE_GET_VERSION			(0x03)

#define GM_CMD_TYPE_SM1				(0x20)
#define GM_CMD_SM1_ENCRYPT				(0x01)
#define GM_CMD_SM1_DECRYPT				(0x02)

#define GM_CMD_TYPE_SM2				(0x30)
#define GM_CMD_SM2_GNE_Z				(0x01)
#define GM_CMD_SM2_GEN_KEYPAIR			(0x02)
#define GM_CMD_SM2_SIGN					(0x03)
#define GM_CMD_SM2_VERIFY				(0x04)
#define GM_CMD_SM2_ENCRYPT				(0x05)
#define GM_CMD_SM2_DECRYPT				(0x06)

#define GM_CMD_SM2_SIGN_EXT				(0x07)
#define GM_CMD_SM2_VERIFY_EXT			(0x08)
#define GM_CMD_SM2_RECOVER				(0x09)

#define GM_CMD_TYPE_SM3				(0x40)
#define GM_CMD_SM3_HASH					(0x01)
#define GM_CMD_SM3_HMAC					(0x02)
#define GM_CMD_SM3_START					(0x06)
#define GM_CMD_SM3_UPDATE					(0x07)
#define GM_CMD_SM3_FINAL					(0x08)

#define GM_CMD_SM3_HASH_1				(0x03)
#define GM_CMD_SM3_HASH_2				(0x04)
#define GM_CMD_SM3_HASH_3				(0x05)

#define GM_CMD_TYPE_SM4				(0x50)
#define GM_CMD_SM4_CBC_ENCRYPT			(0x01)
#define GM_CMD_SM4_CBC_DECRYPT			(0x02)
#define GM_CMD_SM4_ECB_ENCRYPT			(0x03)
#define GM_CMD_SM4_ECB_DECRYPT			(0x04)
#define GM_CMD_SM4_RECOVER					(0x05)

#define GM_CMD_START_VALUE				(0xB0)
#define GM_CMD_END_VALUE				(0xE0)

#define GM_ERR_CMD_TYPE					(1<<0)
#define GM_ERR_CMD_VALUE				(1<<1)
#define GM_ERR_CMD_LEN					(1<<2)
#define GM_ERR_CMD_DATA					(1<<3)
#define GM_ERR_CMD_END					(1<<4)

#define GM_CMD_BUF_MAX_LEN				(1024+512+96)

#define GM_SPI_CMD_DATA_MAX_LEN		(2048)
typedef struct{
	int sm_spi_cs_port;
	int sm_spi_cs_pin;
}SM_Config;

#define SM_SPI_CS SMConfig->sm_spi_cs_port,SMConfig->sm_spi_cs_pin

struct gm_spi_opt {
	void (*spi_init)(void);
	void (*spi_cs)(int );
	void (*spi_enable)(void);
	void (*spi_disable)(void);
	void (*spi_send)(unsigned char );
	void (*spi_recv)(unsigned char *);
};

typedef struct
{
    unsigned char privkey[32];  /*SM2 私钥*/
    unsigned char pubkey[64];  /*SM2 公钥*/
} ST_SM2_KEY;

typedef struct
{
    ST_SM2_KEY  key;  /*SM2 密钥对*/
	unsigned char *ida;		  /*用户身份标识ID*/
	unsigned int idalen;      /*用户身份标识ID长度*/
} ST_SM2_INFO;


//return code
#define SM_RET_OK					 (0)
#define SM_RET_INVALID_PARAM	    (-1)
#define SM_RET_ERROR				(-2)
#define SM_RET_TIMEOUT				(-3)


#define USRID_MAX					512
#define MSG_MAX						1024
#define SM2_ENCRYPT_MAX			    1731 //0x06C3
#define SM2_DECRYPT_MAX			    1827 //0x0723
#define SM3_INPUT_MAX				3712
#define SM3_BUF_MAX					2048
#define SM4_BUF_MAX					2048

#define SM_CORE_THK88				1
#define SM_CORE_SW					0

extern int SM2GenKeyPair(ST_SM2_KEY  *key );
extern int SM2Sign(ST_SM2_INFO *info, unsigned char *data, unsigned int datalen, 
unsigned char *sign, unsigned int *signlen);
extern int SM2Verify(ST_SM2_INFO *info, unsigned char *data, unsigned int datalen,
unsigned char *sign, unsigned int signlen);
extern int SM2Recover(ST_SM2_KEY *key , unsigned char *datain, unsigned int datainlen,
unsigned char *dataout, unsigned short *dataoutlen, unsigned int mode);
extern int SM3(unsigned char *key, unsigned int keylen ,
unsigned char *datain, unsigned int datainlen, unsigned char *dataout );
extern int SM4(unsigned char *datain, unsigned int datainlen, unsigned char *iv,
			unsigned char *key, unsigned char *dataout, unsigned int mode );

#endif

