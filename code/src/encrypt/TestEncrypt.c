#include <string.h>
#include "ZA9L.h"
#include "ZA9L_MacroDef.h"
#include "posapi.h"
#include "Rsa.h"
// 包含公钥和私钥数据文件
#include "Private_Key.h"
#include "Public_Key.h"

uchar ucData128[128], Buff128_1[128], Buff128_2[128];
uchar ucData144[144], Buff144_1[144], Buff144_2[144];
uchar ucData160[160], Buff160_1[160], Buff160_2[160];
uchar ucData248[248], Buff248_1[248], Buff248_2[248];
uchar ucData256[256], Buff256_1[256], Buff256_2[256];

void Rsa_Signal_Test(void);
void Rsa_Rate_Test(void);

void TestDES(void)
{
    uint i = 0;
	uchar ucKey = 0;
	uint j = 0;

	uchar DesKey[8] = {0x7D, 0x0E, 0x14, 0xB2, 0xA9, 0x5C, 0xF3, 0x68};
	uchar Buff[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0xBB, 0xDD};
	uchar BuffTmp[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	uchar buffTemp2[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    while(1)
    {
        ScrCls();
		ScrPrint(20,0,0,"  DES TEST");
		ScrPrint(0,2,1,"1-Singel Test");
		ScrPrint(0,4,1,"2-Multi Test");
	//	ScrPrint(0,4,0,"0 - return");
		ucKey = getkey();
		switch(ucKey)
		{
            case 0x31:
                 ScrCls();
				 des(Buff,BuffTmp,DesKey,ENCRYPT);
				 des(BuffTmp, buffTemp2, DesKey, DECRYPT);
				 for(i = 0;i< 8; i++)
				 {
				      if(buffTemp2[i] != Buff[i])
				      {
				          ScrPrint(0,5,0, " DES Error01");
						  getkey();
						  break;
				      }
				 }
                 ScrPrint(0,5,0, " DES SUCCESS01");

				 des(Buff,BuffTmp,DesKey,DECRYPT);
				 des(BuffTmp, buffTemp2, DesKey, ENCRYPT);
				 for(i = 0;i< 8; i++)
				 {
				      if(buffTemp2[i] != Buff[i])
				      {
				          ScrPrint(0,5,0," DES Error02");
						  getkey();
						  break;
				      }
				 }
				 ScrPrint(0,6,0, " DES SUCCESS02");
				 getkey();
				 break;

			case 0x32:
                 ScrCls();
				 ScrPrint(0,3,0, " DES Calu");
                 s_TimerSet(0,0x100000);
                 for(j = 0; j < 2000; j++)
                 {
                     des(Buff,BuffTmp,DesKey,ENCRYPT);
					 des(BuffTmp, buffTemp2, DesKey, DECRYPT);
					 for(i = 0;i< 8; i++)
					 {
					      if(buffTemp2[i] != Buff[i])
					      {
					          ScrPrint(0,5,0, " DES Error01");
							  getkey();
							  break;
					      }
					 }

					 des(Buff,BuffTmp,DesKey,DECRYPT);
					 des(BuffTmp, buffTemp2, DesKey, ENCRYPT);
					 for(i = 0;i < 8; i++)
					 {
					      if(buffTemp2[i] != Buff[i])
					      {
					          ScrPrint(0,5,0," DES Error02");
							  getkey();
							  break;
					      }
					 }
                 }
                 j = s_TimerCheck(0);
				 ScrPrint(0,6,0,"Time: %d",(0x100000 - j));
				 getkey();
				 break;
			case 0x1b:
				 return;

		}
    }
}

void TestRSA(void)
{
   uint i= 0;
   uchar ucKey = 0;

   memset(ucData128, 0x00, sizeof(ucData128));
   memset(Buff128_1, 0x00, sizeof(Buff128_1));
   memset(Buff128_2, 0x00, sizeof(Buff128_2));

   memset(ucData144, 0x00, sizeof(ucData144));
   memset(Buff144_1, 0x00, sizeof(Buff144_1));
   memset(Buff144_2, 0x00, sizeof(Buff144_2));

   memset(ucData160, 0x00, sizeof(ucData160));
   memset(Buff160_1, 0x00, sizeof(Buff160_1));
   memset(Buff160_2, 0x00, sizeof(Buff160_2));

   memset(ucData248, 0x00, sizeof(ucData248));
   memset(Buff248_1, 0x00, sizeof(Buff248_1));
   memset(Buff248_2, 0x00, sizeof(Buff248_2));

   memset(ucData256, 0x00, sizeof(ucData256));
   memset(Buff256_1, 0x00, sizeof(Buff256_1));
   memset(Buff256_2, 0x00, sizeof(Buff256_2));

   for(i = 0; i < sizeof(ucData128); i++)
   {
       ucData128[i] = (uchar)i;
   }

   for(i = 0; i < sizeof(ucData144); i++)
   {
       ucData144[i] = (uchar)i;
   }

   for(i = 0; i < sizeof(ucData160); i++)
   {
       ucData160[i] = (uchar)i;
   }

   for(i = 0; i < sizeof(ucData248); i++)
   {
       ucData248[i] = (uchar)i;
   }

   for(i = 0; i < 256; i++)
   {
       ucData256[i] = (uchar)i;
   }

   while(1)
   {
	   ScrCls();
	   ScrPrint(10,0, 0, "RSA TEST MENU");
	   ScrPrint(0,2,1, "1-Signal Test");
	   ScrPrint(0,4,1, "2-Rate Test");
	 //  ScrPrint(0,3,0, "0 - Return");
       ucKey = getkey();
	   switch(ucKey)
	   {
	        case 0x31:
				Rsa_Signal_Test();
				break;
			case 0x32:
				Rsa_Rate_Test();
				break;
			case 0x1b:
				return;
	   }
   }
}

void Rsa_Signal_Test(void)
{
   uchar ucKey = 0;
   uint i = 0;
   uint OutLen = 0, InLen = 0;
   int Ret = 0;

   while(1)
   {
       ScrCls();
	   ScrPrint(0,0, 0, "RSA Signal Test Menu");
	   ScrPrint(0,1, 0, "1 - 128Byte Test");
	   ScrPrint(0,2, 0, "2 - 144Byte Test");
	   ScrPrint(0,3, 0, "3 - 160Byte Test");
	   ScrPrint(0,4, 0, "4 - 248Byte Test");
	   ScrPrint(0,5, 0, "5 - 256Byte Test");
	   ScrPrint(0,7, 0, "0 - Return");

	   ucKey = getkey();
       switch(ucKey)
       {
           case 0x31:
		   	   ScrCls();
			   ScrPrint(0,1,0,"128Byte 03 Test");
               Ret = RSAPrivateEncrypt(Buff128_1,&OutLen,ucData128, 128, &testSkeyArray[8]);
			   if(Ret)
			   {
			       ScrPrint(0,2,0,"128B 03 Encry Err %x",Ret);
				   getkey();
				   break;
			   }
			   Ret = RSAPublicDecrypt(Buff128_2, &InLen, Buff128_1, OutLen, &testPkeyArray[8]);
			   if(Ret)
			   {
			       ScrPrint(0,2,0,"128B 03 Decry Err %x",Ret);
				   getkey();
				   break;
			   }
			   for(i = 0; i < 128; i++)
			   {
			       if(ucData128[i] != Buff128_2[i])
			       {
			           ScrPrint(0,2,0,"128B 03 RSA Err, %x", i);
					   getkey();
				       break;
			       }
			   }
			   ScrPrint(0,3,0,"128B 03 SUCC");

			   ScrPrint(0,4,0,"128Byte 65537 Test");
               Ret = RSAPrivateEncrypt(Buff128_1,&OutLen,ucData128, 128, &testSkeyArray[9]);
			   if(Ret)
			   {
			       ScrPrint(0,5,0,"128B Encry Err %x",Ret);
				   getkey();
				   break;
			   }
			   Ret = RSAPublicDecrypt(Buff128_2, &InLen, Buff128_1, OutLen, &testPkeyArray[9]);
			   if(Ret)
			   {
			       ScrPrint(0,5,0,"128B 65537 Decry Err %x",Ret);
				   getkey();
				   break;
			   }
			   for(i = 0; i < 128; i++)
			   {
			       if(ucData128[i] != Buff128_2[i])
			       {
			           ScrPrint(0,5,0,"128B 65537 RSA Err, %x", i);
					   getkey();
				       break;
			       }
			   }
			   ScrPrint(0,6,0,"128B 65537 SUCC");
			   getkey();
			   break;
           case 0x32:
		   	   ScrCls();
			   ScrPrint(0,1,0,"144Byte 03 Test");
               Ret = RSAPrivateEncrypt(Buff144_1,&OutLen,ucData144, 144, &testSkeyArray[6]);
			   if(Ret)
			   {
			       ScrPrint(0,2,0,"144B 03 Encry Err %x",Ret);
				   getkey();
				   break;
			   }
			   Ret = RSAPublicDecrypt(Buff144_2, &InLen, Buff144_1, OutLen, &testPkeyArray[6]);
			   if(Ret)
			   {
			       ScrPrint(0,2,0,"144B 03 Decry Err %x",Ret);
				   getkey();
				   break;
			   }
			   for(i = 0; i < 144; i++)
			   {
			       if(ucData144[i] != Buff144_2[i])
			       {
			           ScrPrint(0,2,0,"144B 03 RSA Err, %x", i);
					   getkey();
				       break;
			       }
			   }
			   ScrPrint(0,3,0,"144B 03 SUCC");

			   ScrPrint(0,4,0,"144Byte 65537 Test");
               Ret = RSAPrivateEncrypt(Buff144_1,&OutLen,ucData144, 144, &testSkeyArray[7]);
			   if(Ret)
			   {
			       ScrPrint(0,5,0,"144B Encry Err %x",Ret);
				   getkey();
				   break;
			   }
			   Ret = RSAPublicDecrypt(Buff144_2, &InLen, Buff144_1, OutLen, &testPkeyArray[7]);
			   if(Ret)
			   {
			       ScrPrint(0,5,0,"144B 65537 Decry Err %x",Ret);
				   getkey();
				   break;
			   }
			   for(i = 0; i < 144; i++)
			   {
			       if(ucData144[i] != Buff144_2[i])
			       {
			           ScrPrint(0,5,0,"144B 65537 RSA Err, %x", i);
					   getkey();
				       break;
			       }
			   }
			   ScrPrint(0,6,0,"144B 65537 SUCC");
			   getkey();
			   break;
		   case 0x33:
		   	   ScrCls();
			   ScrPrint(0,1,0,"160Byte 03 Test");
               Ret = RSAPrivateEncrypt(Buff160_1,&OutLen,ucData160, 160, &testSkeyArray[4]);
			   if(Ret)
			   {
			       ScrPrint(0,2,0,"160B 03 Encry Err %x",Ret);
				   getkey();
				   break;
			   }
			   Ret = RSAPublicDecrypt(Buff160_2, &InLen, Buff160_1, OutLen, &testPkeyArray[4]);
			   if(Ret)
			   {
			       ScrPrint(0,2,0,"160B 03 Decry Err %x",Ret);
				   getkey();
				   break;
			   }
			   for(i = 0; i < 160; i++)
			   {
			       if(ucData160[i] != Buff160_2[i])
			       {
			           ScrPrint(0,2,0,"160B 03 RSA Err, %x", i);
					   getkey();
				       break;
			       }
			   }
			   ScrPrint(0,3,0,"160B 03 SUCC");

			   ScrPrint(0,4,0,"160Byte 65537 Test");
               Ret = RSAPrivateEncrypt(Buff160_1,&OutLen,ucData160, 160, &testSkeyArray[5]);
			   if(Ret)
			   {
			       ScrPrint(0,5,0,"160B Encry Err %x",Ret);
				   getkey();
				   break;
			   }
			   Ret = RSAPublicDecrypt(Buff160_2, &InLen, Buff160_1, OutLen, &testPkeyArray[5]);
			   if(Ret)
			   {
			       ScrPrint(0,5,0,"160B 65537 Decry Err %x",Ret);
				   getkey();
				   break;
			   }
			   for(i = 0; i < 160; i++)
			   {
			       if(ucData160[i] != Buff160_2[i])
			       {
			           ScrPrint(0,5,0,"160B 65537 RSA Err, %x", i);
					   getkey();
				       break;
			       }
			   }
			   ScrPrint(0,6,0,"160B 65537 SUCC");
			   getkey();
			   break;
		   case 0x34:
		   	   ScrCls();
			   ScrPrint(0,1,0,"248Byte 03 Test");
               Ret = RSAPrivateEncrypt(Buff248_1,&OutLen,ucData248, 248, &testSkeyArray[2]);
			   if(Ret)
			   {
			       ScrPrint(0,2,0,"248B 03 Encry Err %x",Ret);
				   getkey();
				   break;
			   }
			   Ret = RSAPublicDecrypt(Buff248_2, &InLen, Buff248_1, OutLen, &testPkeyArray[2]);
			   if(Ret)
			   {
			       ScrPrint(0,2,0,"248B 03 Decry Err %x",Ret);
				   getkey();
				   break;
			   }
			   for(i = 0; i < 248; i++)
			   {
			       if(ucData248[i] != Buff248_2[i])
			       {
			           ScrPrint(0,2,0,"248B 03 RSA Err, %x", i);
					   getkey();
				       break;
			       }
			   }
			   ScrPrint(0,3,0,"248B 03 SUCC");

			   ScrPrint(0,4,0,"248Byte 65537 Test");
               Ret = RSAPrivateEncrypt(Buff248_1,&OutLen,ucData248, 248, &testSkeyArray[3]);
			   if(Ret)
			   {
			       ScrPrint(0,5,0,"248B Encry Err %x",Ret);
				   getkey();
				   break;
			   }
			   Ret = RSAPublicDecrypt(Buff248_2, &InLen, Buff248_1, OutLen, &testPkeyArray[3]);
			   if(Ret)
			   {
			       ScrPrint(0,5,0,"248B 65537 Decry Err %x",Ret);
				   getkey();
				   break;
			   }
			   for(i = 0; i < 248; i++)
			   {
			       if(ucData248[i] != Buff248_2[i])
			       {
			           ScrPrint(0,5,0,"248B 65537 RSA Err, %x", i);
					   getkey();
				       break;
			       }
			   }
			   ScrPrint(0,6,0,"248B 65537 SUCC");
			   getkey();
			   break;
		   case 0x35:
		   	   ScrCls();
			   ScrPrint(0,1,0,"256Byte 03 Test");
               Ret = RSAPrivateEncrypt(Buff256_1,&OutLen,ucData256, 256, &testSkeyArray[0]);
			   if(Ret)
			   {
			       ScrPrint(0,2,0,"256B 03 Encry Err %x",Ret);
				   getkey();
				   break;
			   }
			   Ret = RSAPublicDecrypt(Buff256_2, &InLen, Buff256_1, OutLen, &testPkeyArray[0]);
			   if(Ret)
			   {
			       ScrPrint(0,2,0,"256B 03 Decry Err %x",Ret);
				   getkey();
				   break;
			   }
			   for(i = 0; i < 256; i++)
			   {
			       if(ucData256[i] != Buff256_2[i])
			       {
			           ScrPrint(0,2,0,"256B 03 RSA Err, %x", i);
					   getkey();
				       break;
			       }
			   }
			   ScrPrint(0,3,0,"256B 03 SUCC");

			   ScrPrint(0,4,0,"256Byte 65537 Test");
               Ret = RSAPrivateEncrypt(Buff256_1,&OutLen,ucData256, 256, &testSkeyArray[1]);
			   if(Ret)
			   {
			       ScrPrint(0,5,0,"256B Encry Err %x",Ret);
				   getkey();
				   break;
			   }
			   Ret = RSAPublicDecrypt(Buff256_2, &InLen, Buff256_1, OutLen, &testPkeyArray[1]);
			   if(Ret)
			   {
			       ScrPrint(0,5,0,"256B 65537 Decry Err %x",Ret);
				   getkey();
				   break;
			   }
			   for(i = 0; i < 256; i++)
			   {
			       if(ucData256[i] != Buff256_2[i])
			       {
			           ScrPrint(0,5,0,"256B 65537 RSA Err, %x", i);
					   getkey();
				       break;
			       }
			   }
			   ScrPrint(0,6,0,"256B 65537 SUCC");
			   getkey();
			   break;
		   case 0x30:
		   	   return;
       }
   }
}

void Rsa_Rate_Test(void)
{
    uchar ucKey = 0;
	uint i = 0, j = 0;
	uint OutLen = 0, InLen = 0;
	int Ret = 0;

    while(1)
    {
        ScrCls();
		ScrPrint(0,0, 0, "RSA Rate Test Menu");
	    ScrPrint(0,1, 0, "1 - 128Byte Rate Test");
	    ScrPrint(0,2, 0, "2 - 144Byte Rate Test");
	    ScrPrint(0,3, 0, "3 - 160Byte Rate Test");
	    ScrPrint(0,4, 0, "4 - 248Byte Rate Test");
	    ScrPrint(0,5, 0, "5 - 256Byte Rate Test");
	    ScrPrint(0,7, 0, "0 - Return");

	    ucKey = getkey();
		switch(ucKey)
		{
             case 0x31:
               ScrCls();
			   ScrPrint(0,1,0,"128B 03 Rate Test");
			   s_TimerSet(0,0x100000);
               for(i = 0; i < 300; i++)
               {
                   Ret = RSAPrivateEncrypt(Buff128_1,&OutLen,ucData128,128, &testSkeyArray[8]);
				   if(Ret)
				   {
				       ScrPrint(0,2,0,"128B 03 Encry Err %x",Ret);
					   getkey();
					   break;
				   }
               }
               j = s_TimerCheck(0);
			   ScrPrint(0,2,0,"03 PrivateTime:%d",(0x100000 - j)/30);
               s_TimerSet(0,0x100000);
               for(i = 0; i < 1000; i++)
               {
                   Ret = RSAPublicDecrypt(Buff128_2, &InLen, Buff128_1, OutLen, &testPkeyArray[8]);
				   if(Ret)
				   {
				       ScrPrint(0,3,0,"128B 03 Decry Err %x",Ret);
					   getkey();
					   break;
				   }
               }
			   j = s_TimerCheck(0);
			   ScrPrint(0,3,0,"03 PublicTime:%d",(0x100000 - j)/100);

			   ScrPrint(0,4,0,"128B 65537 Rate Test");
			   s_TimerSet(0,0x100000);
               for(i = 0; i < 300; i++)
               {
                   Ret = RSAPrivateEncrypt(Buff128_1,&OutLen,ucData128,128, &testSkeyArray[9]);
				   if(Ret)
				   {
				       ScrPrint(0,5,0,"128B 65537 Encry Err %x",Ret);
					   getkey();
					   break;
				   }
               }
               j = s_TimerCheck(0);
			   ScrPrint(0,5,0,"65537 PrivateTime:%d",(0x100000 - j)/30);
               s_TimerSet(0,0x100000);
               for(i = 0; i < 1000; i++)
               {
                   Ret = RSAPublicDecrypt(Buff128_2, &InLen, Buff128_1, OutLen, &testPkeyArray[9]);
				   if(Ret)
				   {
				       ScrPrint(0,6,0,"128B 65537 Decry Err %x",Ret);
					   getkey();
					   break;
				   }
               }
			   j = s_TimerCheck(0);
			   ScrPrint(0,6,0,"65537 PublicTime:%d",(0x100000 - j)/100);
			   getkey();
			   break;
		   case 0x32:
               ScrCls();
			   ScrPrint(0,1,0,"144B 03 Rate Test");
			   s_TimerSet(0,0x100000);
               for(i = 0; i < 300; i++)
               {
                   Ret = RSAPrivateEncrypt(Buff144_1,&OutLen,ucData144,144, &testSkeyArray[6]);
				   if(Ret)
				   {
				       ScrPrint(0,2,0,"144B 03 Encry Err %x",Ret);
					   getkey();
					   break;
				   }
               }
               j = s_TimerCheck(0);
			   ScrPrint(0,2,0,"03 PrivateTime:%d",(0x100000 - j)/30);
               s_TimerSet(0,0x100000);
               for(i = 0; i < 1000; i++)
               {
                   Ret = RSAPublicDecrypt(Buff144_2, &InLen, Buff144_1, OutLen, &testPkeyArray[6]);
				   if(Ret)
				   {
				       ScrPrint(0,3,0,"144B 03 Decry Err %x",Ret);
					   getkey();
					   break;
				   }
               }
			   j = s_TimerCheck(0);
			   ScrPrint(0,3,0,"03 PublicTime:%d",(0x100000 - j)/100);

			   ScrPrint(0,4,0,"144B 65537 Rate Test");
			   s_TimerSet(0,0x100000);
               for(i = 0; i < 300; i++)
               {
                   Ret = RSAPrivateEncrypt(Buff144_1,&OutLen,ucData144,144, &testSkeyArray[7]);
				   if(Ret)
				   {
				       ScrPrint(0,5,0,"144B 65537 Encry Err %x",Ret);
					   getkey();
					   break;
				   }
               }
               j = s_TimerCheck(0);
			   ScrPrint(0,5,0,"65537 PrivateTime:%d",(0x100000 - j)/30);
               s_TimerSet(0,0x100000);
               for(i = 0; i < 1000; i++)
               {
                   Ret = RSAPublicDecrypt(Buff144_2, &InLen, Buff144_1, OutLen, &testPkeyArray[7]);
				   if(Ret)
				   {
				       ScrPrint(0,6,0,"144B 65537 Decry Err %x",Ret);
					   getkey();
					   break;
				   }
               }
			   j = s_TimerCheck(0);
			   ScrPrint(0,6,0,"65537 PublicTime:%d",(0x100000 - j)/100);
			   getkey();
			   break;
		   case 0x33:
               ScrCls();
			   ScrPrint(0,1,0,"160B 03 Rate Test");
			   s_TimerSet(0,0x100000);
               for(i = 0; i < 300; i++)
               {
                   Ret = RSAPrivateEncrypt(Buff160_1,&OutLen,ucData160,160, &testSkeyArray[4]);
				   if(Ret)
				   {
				       ScrPrint(0,2,0,"160B 03 Encry Err %x",Ret);
					   getkey();
					   break;
				   }
               }
               j = s_TimerCheck(0);
			   ScrPrint(0,2,0,"03 PrivateTime:%d",(0x100000 - j)/30);
               s_TimerSet(0,0x100000);
               for(i = 0; i < 1000; i++)
               {
                   Ret = RSAPublicDecrypt(Buff160_2, &InLen, Buff160_1, OutLen, &testPkeyArray[4]);
				   if(Ret)
				   {
				       ScrPrint(0,3,0,"160B 03 Decry Err %x",Ret);
					   getkey();
					   break;
				   }
               }
			   j = s_TimerCheck(0);
			   ScrPrint(0,3,0,"03 PublicTime:%d",(0x100000 - j)/100);

			   ScrPrint(0,4,0,"160B 65537 Rate Test");
			   s_TimerSet(0,0x100000);
               for(i = 0; i < 300; i++)
               {
                   Ret = RSAPrivateEncrypt(Buff160_1,&OutLen,ucData160,160, &testSkeyArray[5]);
				   if(Ret)
				   {
				       ScrPrint(0,5,0,"160B 65537 Encry Err %x",Ret);
					   getkey();
					   break;
				   }
               }
               j = s_TimerCheck(0);
			   ScrPrint(0,5,0,"65537 PrivateTime:%d",(0x100000 - j)/30);
               s_TimerSet(0,0x100000);
               for(i = 0; i < 1000; i++)
               {
                   Ret = RSAPublicDecrypt(Buff160_2, &InLen, Buff160_1, OutLen, &testPkeyArray[5]);
				   if(Ret)
				   {
				       ScrPrint(0,6,0,"160B 65537 Decry Err %x",Ret);
					   getkey();
					   break;
				   }
               }
			   j = s_TimerCheck(0);
			   ScrPrint(0,6,0,"65537 PublicTime:%d",(0x100000 - j)/100);
			   getkey();
			   break;
		   case 0x34:
               ScrCls();
			   ScrPrint(0,1,0,"248B 03 Rate Test");
			   s_TimerSet(0,0x100000);
               for(i = 0; i < 300; i++)
               {
                   Ret = RSAPrivateEncrypt(Buff248_1,&OutLen,ucData248,248, &testSkeyArray[2]);
				   if(Ret)
				   {
				       ScrPrint(0,2,0,"248B 03 Encry Err %x",Ret);
					   getkey();
					   break;
				   }
               }
               j = s_TimerCheck(0);
			   ScrPrint(0,2,0,"03 PrivateTime:%d",(0x100000 - j)/30);
               s_TimerSet(0,0x100000);
               for(i = 0; i < 1000; i++)
               {
                   Ret = RSAPublicDecrypt(Buff248_2, &InLen, Buff248_1, OutLen, &testPkeyArray[2]);
				   if(Ret)
				   {
				       ScrPrint(0,3,0,"248B 03 Decry Err %x",Ret);
					   getkey();
					   break;
				   }
               }
			   j = s_TimerCheck(0);
			   ScrPrint(0,3,0,"03 PublicTime:%d",(0x100000 - j)/100);

			   ScrPrint(0,4,0,"248B 65537 Rate Test");
			   s_TimerSet(0,0x100000);
               for(i = 0; i < 300; i++)
               {
                   Ret = RSAPrivateEncrypt(Buff248_1,&OutLen,ucData248,248, &testSkeyArray[3]);
				   if(Ret)
				   {
				       ScrPrint(0,5,0,"248B 65537 Encry Err %x",Ret);
					   getkey();
					   break;
				   }
               }
               j = s_TimerCheck(0);
			   ScrPrint(0,5,0,"65537 PrivateTime:%d",(0x100000 - j)/30);
               s_TimerSet(0,0x100000);
               for(i = 0; i < 1000; i++)
               {
                   Ret = RSAPublicDecrypt(Buff248_2, &InLen, Buff248_1, OutLen, &testPkeyArray[3]);
				   if(Ret)
				   {
				       ScrPrint(0,6,0,"248B 65537 Decry Err %x",Ret);
					   getkey();
					   break;
				   }
               }
			   j = s_TimerCheck(0);
			   ScrPrint(0,6,0,"65537 PublicTime:%d",(0x100000 - j)/100);
			   getkey();
			   break;
		   case 0x35:
               ScrCls();
			   ScrPrint(0,1,0,"256B 03 Rate Test");
			   s_TimerSet(0,0x100000);
               for(i = 0; i < 300; i++)
               {
                   Ret = RSAPrivateEncrypt(Buff256_1,&OutLen,ucData256,256, &testSkeyArray[0]);
				   if(Ret)
				   {
				       ScrPrint(0,2,0,"256B 03 Encry Err %x",Ret);
					   getkey();
					   break;
				   }
               }
               j = s_TimerCheck(0);
			   ScrPrint(0,2,0,"03 PrivateTime:%d",(0x100000 - j)/30);
               s_TimerSet(0,0x100000);
               for(i = 0; i < 1000; i++)
               {
                   Ret = RSAPublicDecrypt(Buff256_2, &InLen, Buff256_1, OutLen, &testPkeyArray[0]);
				   if(Ret)
				   {
				       ScrPrint(0,3,0,"256B 03 Decry Err %x",Ret);
					   getkey();
					   break;
				   }
               }
			   j = s_TimerCheck(0);
			   ScrPrint(0,3,0,"03 PublicTime:%d",(0x100000 - j)/100);

			   ScrPrint(0,4,0,"256B 65537 Rate Test");
			   s_TimerSet(0,0x100000);
               for(i = 0; i < 300; i++)
               {
                   Ret = RSAPrivateEncrypt(Buff256_1,&OutLen,ucData256,256, &testSkeyArray[1]);
				   if(Ret)
				   {
				       ScrPrint(0,5,0,"256B 65537 Encry Err %x",Ret);
					   getkey();
					   break;
				   }
               }
               j = s_TimerCheck(0);
			   ScrPrint(0,5,0,"65537 PrivateTime:%d",(0x100000 - j)/30);
               s_TimerSet(0,0x100000);
               for(i = 0; i < 1000; i++)
               {
                   Ret = RSAPublicDecrypt(Buff256_2, &InLen, Buff256_1, OutLen, &testPkeyArray[1]);
				   if(Ret)
				   {
				       ScrPrint(0,6,0,"256B 65537 Decry Err %x",Ret);
					   getkey();
					   break;
				   }
               }
			   j = s_TimerCheck(0);
			   ScrPrint(0,6,0,"65537 PublicTime:%d",(0x100000 - j)/100);
			   getkey();
			   break;
		   case 0x30:
		   	   return;
		}
    }

}

void TestEncrypt(void)
{
    uchar ucKeyTemp = 0;

	while(1)
	{
       ScrCls();
	   ScrPrint(12,0,0,"ENCRYPT TEST MENU");
	   ScrPrint(0,2,1,"1-DES Test");
	   ScrPrint(0,4,1,"2-RSA Test");
//	   ScrPrint(0,4,0,"0 - Return");

	   ucKeyTemp = getkey();
	   switch(ucKeyTemp)
	   {
	       case 0x31:
		   	   TestDES();
			   break;
		   case 0x32:
		   	   TestRSA();
			   break;
		   case 0x1b:
		   	   return;
	   }
	}
}


