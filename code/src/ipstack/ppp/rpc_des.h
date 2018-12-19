#define DES_MAXLEN 	65536	/* maximum # of bytes to encrypt  */
#define DES_QUICKLEN	16	/* maximum # of bytes to encrypt quickly */

#ifdef HEADER_DES_H
#undef ENCRYPT
#undef DECRYPT
#endif

enum desdir { ENCRYPT, DECRYPT };
enum desmode { CBC, ECB };

/*
 * parameters to ioctl call
 */
struct desparams {
	unsigned char des_key[8];	/* key (with low bit parity) */
	enum desdir des_dir;	/* direction */
	enum desmode des_mode;	/* mode */
	unsigned char des_ivec[8];	/* input vector */
	unsigned des_len;	/* number of bytes to crypt */
	union {
		unsigned char UDES_data[DES_QUICKLEN];
		unsigned char *UDES_buf;
	} UDES;
#define des_data UDES.UDES_data	/* direct data here if quick */
#define des_buf	UDES.UDES_buf	/* otherwise, pointer to data */
};

