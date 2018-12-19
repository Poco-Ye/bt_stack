#ifndef HEADER_NEW_DES_H
#define HEADER_NEW_DES_H


#ifdef  __cplusplus
extern "C" {
#endif

typedef unsigned char DES_cblock[8];
typedef unsigned char const_DES_cblock[8];
#define DES_LONG unsigned long

typedef struct DES_ks
{
	union
	{
	DES_cblock cblock;
	/* make sure things are correct size on machines with
	 * 8 byte longs */
	DES_LONG deslong[2];
	} ks[16];
} DES_key_schedule;

#define DES_KEY_SZ 	(sizeof(DES_cblock))
#define DES_SCHEDULE_SZ (sizeof(DES_key_schedule))

#define DES_ENCRYPT	1
#define DES_DECRYPT	0

#define DES_CBC_MODE	0
#define DES_PCBC_MODE	1

DES_LONG DES_cbc_cksum(const unsigned char *input,DES_cblock *output,
		       long length,DES_key_schedule *schedule,
		       const_DES_cblock *ivec);
/* DES_cbc_encrypt does not update the IV!  Use DES_ncbc_encrypt instead. */
void DES_cbc_encrypt(const unsigned char *input,unsigned char *output,
		     long length,DES_key_schedule *schedule,DES_cblock *ivec,
		     int enc);
void DES_ncbc_encrypt(const unsigned char *input,unsigned char *output,
		      long length,DES_key_schedule *schedule,DES_cblock *ivec,
		      int enc);
void DES_xcbc_encrypt(const unsigned char *input,unsigned char *output,
		      long length,DES_key_schedule *schedule,DES_cblock *ivec,
		      const_DES_cblock *inw,const_DES_cblock *outw,int enc);
void DES_cfb_encrypt(const unsigned char *in,unsigned char *out,int numbits,
		     long length,DES_key_schedule *schedule,DES_cblock *ivec,
		     int enc);
void DES_ecb_encrypt(const_DES_cblock *input,DES_cblock *output,
		     DES_key_schedule *ks,int enc);


int DES_set_key(const_DES_cblock *key,DES_key_schedule *schedule);

#define DES_fixup_key_parity DES_set_odd_parity

#ifdef  __cplusplus
}
#endif

#endif
