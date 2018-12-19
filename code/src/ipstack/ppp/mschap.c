/*
 * MS-CHAP v1, v2
 */
#include <inet/inet.h>
#include <inet/inet_list.h>
#include <inet/dev.h>
#include "stdlib.h"
#include "string.h"
#include "ecb_des.h"
#include "sha.h"
#include "md4.h"
#include "mschap.h"
#include "ppp.h"


#ifndef MAX
#define MAX(a, b)	((a) > (b)? (a): (b))
#endif
void DES_set_odd_parity(DES_cblock *key);

//void buf_printf(const unsigned char *buf, int size, const char *descr);

static void BZERO(void *p, int size)
{
	memset(p, 0, size);
}

static uchar Get7Bits(uchar *input, int startBit)
{
    register unsigned int	word1;

    word1  = (unsigned)input[startBit / 8] << 8;
    word1 |= (unsigned)input[startBit / 8 + 1];

    word1 >>= 15 - (startBit % 8 + 7);

    return word1 & 0xFE;
}

/* IN  56 bit DES key missing parity bits
   OUT 64 bit DES key with parity bits added */
void MakeKey(uchar *key, uchar *des_key)
{
    des_key[0] = Get7Bits(key,  0);
    des_key[1] = Get7Bits(key,  7);
    des_key[2] = Get7Bits(key, 14);
    des_key[3] = Get7Bits(key, 21);
    des_key[4] = Get7Bits(key, 28);
    des_key[5] = Get7Bits(key, 35);
    des_key[6] = Get7Bits(key, 42);
    des_key[7] = Get7Bits(key, 49);

    DES_set_odd_parity((DES_cblock *)des_key);
}

static void /* IN 8 octets IN 7 octest OUT 8 octets */
DesEncrypt(uchar *clear, uchar *key, uchar *cipher)
{
    DES_cblock		des_key;
    DES_key_schedule	key_schedule;

    MakeKey(key, des_key);
    DES_set_key(&des_key, &key_schedule);
    DES_ecb_encrypt((DES_cblock *)clear, (DES_cblock *)cipher, &key_schedule, 1);

	buf_printf(key, 7, "raw key:");
	buf_printf(des_key, 8, "des_key:");
	buf_printf(clear, 8, "clear:");
	buf_printf(cipher, 8, "cipher:");
}

/*
 * Convert the ASCII version of the password to Unicode.
 * This implicitly supports 8-bit ISO8859/1 characters.
 * This gives us the little-endian representation, which
 * is assumed by all M$ CHAP RFCs.  (Unicode byte ordering
 * is machine-dependent.)
 */
static void
ascii2unicode(char ascii[], int ascii_len, uchar unicode[])
{
	int i;

	buf_printf((unsigned char*)ascii, ascii_len, "ascii:");

	BZERO(unicode, ascii_len * 2);
	for (i = 0; i < ascii_len; i++)
	unicode[i * 2] = (uchar) ascii[i];
}

static void
NTPasswordHash(uchar *secret, int secret_len, uchar hash[MD4_SIGNATURE_SIZE])
{
	int			mdlen = secret_len * 8;
	MD4_CTX		md4Context;

	MD4Init(&md4Context);
	/* MD4Update can take at most 64 bytes at a time */
	while (mdlen > 512) 
	{
		MD4Update(&md4Context, secret, 512);
		secret += 64;
		mdlen -= 512;
	}
	MD4Update(&md4Context, secret, mdlen);
	MD4Final(hash, &md4Context);
}

static void
ChallengeResponse(uchar *challenge,
		  uchar PasswordHash[MD4_SIGNATURE_SIZE],
		  uchar response[24])
{
	uchar    ZPasswordHash[21];

	BZERO(ZPasswordHash, sizeof(ZPasswordHash));
	INET_MEMCPY(ZPasswordHash, PasswordHash, MD4_SIGNATURE_SIZE);

	DesEncrypt(challenge, ZPasswordHash + 0, response + 0);      
	DesEncrypt(challenge, ZPasswordHash + 7, response + 8);
	DesEncrypt(challenge, ZPasswordHash + 14, response + 16);
}

static void
ChapMS_NT(uchar *rchallenge, char *secret, int secret_len,
	  uchar NTResponse[24])
{
	uchar	unicodePassword[MAX_NT_PASSWORD * 2];
	uchar	PasswordHash[MD4_SIGNATURE_SIZE];

	/* Hash the Unicode version of the secret (== password). */
	ascii2unicode(secret, secret_len, unicodePassword);
	NTPasswordHash(unicodePassword, secret_len * 2, PasswordHash);
	buf_printf(PasswordHash, sizeof(PasswordHash), "PasswordHash:");

	ChallengeResponse(rchallenge, PasswordHash, NTResponse);
    
	buf_printf(NTResponse, 24, "NTResponse:");
}

void ChapMS(unsigned char *rchallenge, char *secret, int secret_len,
       unsigned char *response)
{
	memset(response,0, MS_CHAP_RESPONSE_LEN);

	ChapMS_NT(rchallenge, secret, secret_len, &response[MS_CHAP_NTRESP]);
	response[MS_CHAP_USENT] = 1;
}

void
chapms_make_response(unsigned char *response, int id, char *our_name,
		     unsigned char *challenge, char *secret, int secret_len,
		     unsigned char *private)
{
	*response++ = MS_CHAP_RESPONSE_LEN;
	ChapMS(challenge, secret, secret_len, response);
}

void
ChallengeHash(uchar PeerChallenge[16], uchar *rchallenge,
	      char *username, uchar Challenge[8])
    
{
	SHA_CTX	sha1Context;
	uchar	sha1Hash[SHA1_SIGNATURE_SIZE];
	char	*user;

	buf_printf(PeerChallenge, 16, "PeerChallenge");
	buf_printf(rchallenge, 16, "rchallenge");
	buf_printf((uchar*)username, strlen(username), "username");
	
	/* remove domain from "domain\username" */
	if ((user = strrchr(username, '\\')) != NULL)
		++user;
	else
		user = username;

	SHA1_Init(&sha1Context);
	SHA1_Update(&sha1Context, PeerChallenge, 16);
	SHA1_Update(&sha1Context, rchallenge, 16);
	SHA1_Update(&sha1Context, (unsigned char *)user, strlen(user));
	SHA1_Final(sha1Hash, &sha1Context);

	INET_MEMCPY(Challenge, sha1Hash, 8);
}

static void
ChapMS2_NT(uchar *rchallenge, uchar PeerChallenge[16], char *username,
	   char *secret, int secret_len, uchar NTResponse[24])
{
	uchar	unicodePassword[MAX_NT_PASSWORD * 2];
	uchar	PasswordHash[MD4_SIGNATURE_SIZE];
	uchar	Challenge[8];

	buf_printf(PeerChallenge, 16, "PeerChallenge:");
	ChallengeHash(PeerChallenge, rchallenge, username, Challenge);

	buf_printf(Challenge, 8, "ChallengeHash:");

	/* Hash the Unicode version of the secret (== password). */
	ascii2unicode(secret, secret_len, unicodePassword);
	NTPasswordHash(unicodePassword, secret_len * 2, PasswordHash);

	buf_printf(PasswordHash, MD4_SIGNATURE_SIZE, "PasswordHash:");

	ChallengeResponse(Challenge, PasswordHash, NTResponse);

	buf_printf(NTResponse, 24, "NTResponse:");
}

void
GenerateAuthenticatorResponse(uchar PasswordHashHash[MD4_SIGNATURE_SIZE],
			      uchar NTResponse[24], uchar PeerChallenge[16],
			      uchar *rchallenge, char *username,
			      uchar authResponse[MS_AUTH_RESPONSE_LENGTH+1])
{
	/*
	 * "Magic" constants used in response generation, from RFC 2759.
	 */
	uchar Magic1[39] = /* "Magic server to client signing constant" */
	{ 
		0x4D, 0x61, 0x67, 0x69, 0x63, 0x20, 0x73, 0x65, 0x72, 0x76,
		0x65, 0x72, 0x20, 0x74, 0x6F, 0x20, 0x63, 0x6C, 0x69, 0x65,
		0x6E, 0x74, 0x20, 0x73, 0x69, 0x67, 0x6E, 0x69, 0x6E, 0x67,
		0x20, 0x63, 0x6F, 0x6E, 0x73, 0x74, 0x61, 0x6E, 0x74 
	};
	uchar Magic2[41] = /* "Pad to make it do more than one iteration" */
	{ 
		0x50, 0x61, 0x64, 0x20, 0x74, 0x6F, 0x20, 0x6D, 0x61, 0x6B,
		0x65, 0x20, 0x69, 0x74, 0x20, 0x64, 0x6F, 0x20, 0x6D, 0x6F,
		0x72, 0x65, 0x20, 0x74, 0x68, 0x61, 0x6E, 0x20, 0x6F, 0x6E,
		0x65, 0x20, 0x69, 0x74, 0x65, 0x72, 0x61, 0x74, 0x69, 0x6F,
		0x6E 
	};

	int		i;
	SHA_CTX	sha1Context;
	uchar	Digest[SHA1_SIGNATURE_SIZE];
	uchar	Challenge[8];

	SHA1_Init(&sha1Context);
	SHA1_Update(&sha1Context, PasswordHashHash, MD4_SIGNATURE_SIZE);
	SHA1_Update(&sha1Context, NTResponse, 24);
	SHA1_Update(&sha1Context, Magic1, sizeof(Magic1));
	SHA1_Final(Digest, &sha1Context);

	ChallengeHash(PeerChallenge, rchallenge, username, Challenge);

	SHA1_Init(&sha1Context);
	SHA1_Update(&sha1Context, Digest, sizeof(Digest));
	SHA1_Update(&sha1Context, Challenge, sizeof(Challenge));
	SHA1_Update(&sha1Context, Magic2, sizeof(Magic2));
	SHA1_Final(Digest, &sha1Context);

	/* Convert to ASCII hex string. */
	for (i = 0; i < MAX((MS_AUTH_RESPONSE_LENGTH / 2), sizeof(Digest)); i++)
		sprintf((char *)&authResponse[i * 2], "%02X", Digest[i]);
	authResponse[MS_AUTH_RESPONSE_LENGTH+1] = 0;
}


static void
GenerateAuthenticatorResponsePlain
		(char *secret, int secret_len,
		 uchar NTResponse[24], uchar PeerChallenge[16],
		 uchar *rchallenge, char *username,
		 uchar authResponse[MS_AUTH_RESPONSE_LENGTH+1])
{
	uchar	unicodePassword[MAX_NT_PASSWORD * 2];
	uchar	PasswordHash[MD4_SIGNATURE_SIZE];
	uchar	PasswordHashHash[MD4_SIGNATURE_SIZE];

	/* Hash (x2) the Unicode version of the secret (== password). */
	ascii2unicode(secret, secret_len, unicodePassword);
	NTPasswordHash(unicodePassword, secret_len * 2, PasswordHash);
	NTPasswordHash(PasswordHash, sizeof(PasswordHash),
		PasswordHashHash);
	buf_printf(PasswordHashHash, 16, "PasswordHashHash:");

	authResponse[0]='S';
	authResponse[1]='=';
	GenerateAuthenticatorResponse(PasswordHashHash, NTResponse, PeerChallenge,
		rchallenge, username, authResponse+2);
}

/*
 * If PeerChallenge is NULL, one is generated and the PeerChallenge
 * field of response is filled in.  Call this way when generating a response.
 * If PeerChallenge is supplied, it is copied into the PeerChallenge field.
 * Call this way when verifying a response (or debugging).
 * Do not call with PeerChallenge = response.
 *
 * The PeerChallenge field of response is then used for calculation of the
 * Authenticator Response.
 */
void
ChapMS2(uchar *rchallenge, uchar *PeerChallenge,
	char *user, char *secret, int secret_len, unsigned char *response,
	uchar authResponse[], int authenticator)
{
	/* ARGSUSED */
	uchar *p = &response[MS_CHAP2_PEER_CHALLENGE];
	int i;

	//BZERO(response, 50);

	/* Generate the Peer-Challenge if requested, or copy it if supplied. */
	if (!PeerChallenge)
	{
		for (i = 0; i < MS_CHAP2_PEER_CHAL_LEN; i++)
			*p++ = rand();//(uchar) (i+1);
	}
	else
	{
		INET_MEMCPY(&response[MS_CHAP2_PEER_CHALLENGE],PeerChallenge, 
			MS_CHAP2_PEER_CHAL_LEN);
	}

	buf_printf(rchallenge, 16, "rchallenge");

	/* Generate the NT-Response */
	ChapMS2_NT(rchallenge, &response[MS_CHAP2_PEER_CHALLENGE], user,
			secret, secret_len, &response[MS_CHAP2_NTRESP]);

	/* Generate the Authenticator Response. */
	GenerateAuthenticatorResponsePlain(secret, secret_len,
			&response[MS_CHAP2_NTRESP],
			&response[MS_CHAP2_PEER_CHALLENGE],
			rchallenge, user, authResponse);
}


void
chapms2_make_response(unsigned char *response, int id, char *our_name,
		      unsigned char *challenge, char *secret, int secret_len,
		      unsigned char *private)
{
	uchar peerchange[]={0x21,0x40,0x23,0x24,0x25,0x5E,0x26,0x2A,0x28,0x29,0x5F,0x2B,0x3A,0x33,0x7C,0x7E};
	*response++ = MS_CHAP2_RESPONSE_LEN;
	memset(response, 0, MS_CHAP2_RESPONSE_LEN);
	buf_printf((uchar*)our_name, strlen(our_name), "UserID:");
	buf_printf(challenge, 16, "challenge:");
	buf_printf((uchar*)secret, secret_len, "Passwd:");
	ChapMS2(challenge,
		peerchange,
		our_name, secret, secret_len, response, private,
		MS_CHAP2_AUTHENTICATEE);
}

void chapms_test(void)
{
	unsigned char response[100];
	//char challenge[100]={0x8,0x07,0x7a,0xb7,0x74,0x74,0xe2,0x50,0x3a,};
	//char secret[]={0x39,0x36,0x31,0x36,0x39,0x39,0x36,0x31,0x36,0x39,};
	unsigned char challenge[100]={0x8, 0x10, 0x2D, 0xB5, 0xDF, 0x08, 0x5D, 0x30, 0x41,};
	char secret[]={0x4D,0x79,0x50,0x77,};
	int secret_len = sizeof(secret);

	//test1();
	//return;

	chapms_make_response(response, 0x0, "96169", challenge, secret, secret_len, NULL);
	//buf_printf(challenge, 8, "Challenge:");
	//buf_printf(secret, secret_len, "Secret:");
	//buf_printf(response, MS_CHAP_RESPONSE_LEN+1, "Response:");
}

