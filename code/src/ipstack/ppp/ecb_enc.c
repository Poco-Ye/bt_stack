#include "des_locl.h"
#include "des_ver.h"
#include "spr.h"

void DES_ecb_encrypt(const_DES_cblock *input, DES_cblock *output,
		     DES_key_schedule *ks, int enc)
{
	register DES_LONG l;
	DES_LONG ll[2];
	const unsigned char *in = &(*input)[0];
	unsigned char *out = &(*output)[0];

	c2l(in,l); ll[0]=l;
	c2l(in,l); ll[1]=l;
	DES_encrypt1(ll,ks,enc);
	l=ll[0]; l2c(l,out);
	l=ll[1]; l2c(l,out);
	l=ll[0]=ll[1]=0;
}
