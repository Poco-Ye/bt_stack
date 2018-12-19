/*
 * MS-Chap v1, v2
 */
#ifndef _MSCHAP_H_
#define _MSCHAP_H_

#define MS_CHAP_RESPONSE_LEN 49
#define uchar unsigned char
#define MD4_SIGNATURE_SIZE	16	/* 16 bytes in a MD4 message digest */
#define MAX_NT_PASSWORD		256	/* Max (Unicode) chars in an NT pass */
#define MS_CHAP_LANMANRESP	0
#define MS_CHAP_LANMANRESP_LEN	24
#define MS_CHAP_NTRESP		24
#define MS_CHAP_NTRESP_LEN	24
#define MS_CHAP_USENT		48


/*
 * Offsets within the response field for MS-CHAP2
 */
#define MS_CHAP2_PEER_CHALLENGE	0
#define MS_CHAP2_PEER_CHAL_LEN	16
#define MS_CHAP2_RESERVED_LEN	8
#define MS_CHAP2_NTRESP		24
#define MS_CHAP2_NTRESP_LEN	24
#define MS_CHAP2_FLAGS		48

#define MS_CHAP2_RESPONSE_LEN	49	/* Response length for MS-CHAPv2 */
#define MS_AUTH_RESPONSE_LENGTH	40	/* MS-CHAPv2 authenticator response, */
					/* as ASCII */

/* Are we the authenticator or authenticatee?  For MS-CHAPv2 key derivation. */
#define MS_CHAP2_AUTHENTICATEE 0
#define MS_CHAP2_AUTHENTICATOR 1
#define SHA1_SIGNATURE_SIZE 20

void
chapms_make_response(unsigned char *response, int id, char *our_name,
		     unsigned char *challenge, char *secret, int secret_len,
		     unsigned char *private);


void
chapms2_make_response(unsigned char *response, int id, char *our_name,
		      unsigned char *challenge, char *secret, int secret_len,
		      unsigned char *private);
void ppp_md5_digest(unsigned char *digest, const unsigned char *data, unsigned int len);

#endif/* _MSCHAP_H_ */
