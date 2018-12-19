#define PRINTER_STYLUS_TEST
#ifdef PRINTER_STYLUS_TEST

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "base.h"
#include "printer_stylus.h"
#include "..\..\font\font.h" 
#include "..\..\cfgmanage\cfgmanage.h"

/*****************************************************/
void u_prn_test(void)
{
    uchar code[3];
    memset(code, 0x00, sizeof(code));
    code[2] = 0x00;
    ScrCls();
    ScrPrint(0, 0, 0x00, "u_prn_test : GB2312");
    
    // 打印GB2312字符集全部字符
    for(code[0]=0xA1; code[0]<=0xA9; code[0]++)
    {
        PrnInit_stylus();
        PrnFontSet_stylus(0, 2);
        PrnStr_stylus("\x01\x01\x01\x03");
        for(code[1]=0xA1; code[1]<=0xFE; code[1]++)
        {
            PrnStr_stylus("%s", code);
        }
        PrnStr_stylus("  0x%x\n", code[0]);
        ScrPrint(0, 4, 0x00, "0x%2x, PrnStart Ret = %d", code[0],PrnStart_stylus());
        getkey();        
    }
    getkey();

    for(code[0]=0xB0; code[0]<=0xF7; code[0]++)
    {
        PrnInit_stylus();
        PrnFontSet_stylus(0, 2);
        PrnStr_stylus("\x01\x01\x01\x03");
        for(code[1]=0xA1; code[1]<=0xFE; code[1]++)
        {
            PrnStr_stylus("%s", code);
        }
        PrnStr_stylus("  0x%x\n", code[0]);
        ScrPrint(0, 4, 0x00, "0x%2x, PrnStart Ret = %d", code[0],PrnStart_stylus());
        getkey();
    }
    getkey();
}


void PrinterTest(void)
{
    unsigned char ucStatus = 0;
    unsigned char ucKey = 0;
    unsigned char ucData = 0xFF;
    unsigned char aucLogoBuff[20000];
    unsigned int i = 0;
    unsigned int cnt=0;
    unsigned char tmpc0,tmpc1,tmpc2;
    int tmpd;

    s_PrnInit_stylus();

	while(1)
	{
        ScrCls();
	    ScrPrint(0,0,0,"Printer 20121101_01");
	    ScrPrint(0,1,0,"1-Init 2-Manual Feed");
	    ScrPrint(0,2,0,"3-AutoFeed 4-Print Str");
	    ScrPrint(0,3,0,"5-Pulse 6-Print Photo");
	    ScrPrint(0,4,0,"7-Step  8-Print Test");
        ScrPrint(0,5,0,"0-TestPrinterSignal");
	    ucKey = getkey();

        ScrCls();
	    switch(ucKey)
	    {
		case KEYCANCEL:return;

        case '0':
            //TestPrinterSignal();
            break;
			
        case '1'://Init
            ucStatus = PrnInit_stylus();
            ScrPrint(0,0,0,"Init Status:%d",ucStatus);
            getkey();
            break;
        case '2'://Manual Feed
            s_ManualFeedPaper_stylus(1,1);
            ScrPrint(0,0,0,"FeedPaper_stylus");
            getkey();
            break;
        case '3'://Auto Feed
            s_AutoFeedPaper();
            ScrPrint(0,0,0,"s_AutoFeedPaper");
            getkey();
            break;
        case '4'://Print Str
            PrnInit_stylus();
	        PrnSpaceSet_stylus(0, 3);
            PrnFontSet_stylus(0, 0);
            ucStatus = PrnStr_stylus("0123456789012345678901234567890123456789");
            ucStatus = PrnStart_stylus();
            ScrPrint(0,0,0,"PrnStr Status:%d\n\r",ucStatus);
		    getkey();
            break;
        case '5'://Pulse
            while(1)
            {
                ps_PrnControl(0,0x55);
                ps_PrnControl(1, ucData&0xdf);
                ps_PrnControl(1, ucData&0xbf);
                for (i = 0;i < 20; i++);
                ps_PrnControl(1, ucData);
                for (i = 0;i < 25000;i++);

                ScrPrint(0,0,0,"Pules");
                if (!kbhit() && getkey() == KEYCANCEL)
                    break;
            }                    
            break;
        case '6'://Print Photo
            ScrPrint(0,0,0,"Print Photo");
    		memset(aucLogoBuff,(unsigned char)0xFF,sizeof(aucLogoBuff));
    		aucLogoBuff[0]= 10;
    		for(i=0; i<10; i++)
    		{
    			aucLogoBuff[i*182+1]=0x00;
    			aucLogoBuff[i*182+2]=180;
    		}
            PrnLogo_stylus(aucLogoBuff);
            PrnStart_stylus();
            getkey();
            break;
        case '7'://step
            ScrPrint(0,0,0,"Step");
            PrnStep_stylus(100);
            PrnStart_stylus();
            getkey();
            break;

        case '8':
        while(1)
        {
            PrnInit_stylus();
            ScrPrint(0,0,0,"Ascii Font Set");
            ScrPrint(0,1,0,"1-8x16 2-16x24");
            ucKey = getkey();
            if(ucKey==KEYCANCEL)break;
            if(ucKey=='1')tmpc0=0;
            else tmpc0=1;

            ScrCls();
            ScrPrint(0,0,0,"Chinese Font Set");
            ScrPrint(0,1,0,"1-16x16 2-24x24");
            ucKey = getkey();
            if(ucKey==KEYCANCEL)break;
            if(ucKey=='1')tmpc1=0;
            else tmpc1=1;

            PrnFontSet_stylus(tmpc0, tmpc1);

            ScrCls();
            ScrPrint(0,0,0,"SpaceSet x");
            ScrPrint(0,1,0,"1-0  2-5");
            ScrPrint(0,2,0,"3-20 4-60");
            ucKey = getkey();
            if(ucKey==KEYCANCEL)break;
            if(ucKey=='1')tmpc0=0;
            else if(ucKey=='2')tmpc0=5;
            else if(ucKey=='3')tmpc0=20;
            else if(ucKey=='4')tmpc0=60;
            else tmpc0=0;

            ScrCls();
            ScrPrint(0,0,0,"SpaceSet y");
            ScrPrint(0,1,0,"1-0  2-5");
            ScrPrint(0,2,0,"3-20 4-255");
            ucKey = getkey();
            if(ucKey==KEYCANCEL)break;
            if(ucKey=='1')tmpc1=0;
            else if(ucKey=='2')tmpc1=5;
            else if(ucKey=='3')tmpc1=20;
            else if(ucKey=='4')tmpc1=255;
            else tmpc1=0;

            PrnSpaceSet_stylus(tmpc0, tmpc1);

            ScrCls();
            ScrPrint(0,0,0,"Step");
            ScrPrint(0,1,0,"1-5   2-20");
            ScrPrint(0,2,0,"3- -5 4- -20");
            ScrPrint(0,3,0,"0-0");
            ucKey = getkey();
            if(ucKey==KEYCANCEL)break;
            if(ucKey=='1')tmpd=5;
            else if(ucKey=='2')tmpd=20;
            else if(ucKey=='3')tmpd=-5;
            else if(ucKey=='4')tmpd=-20;
            else if(ucKey=='0')tmpd=0;
            else tmpd=0;

            PrnStep_stylus(tmpd);

            ScrCls();
            ScrPrint(0,0,0,"LeftIndent");
            ScrPrint(0,1,0,"1-0  2-5");
            ScrPrint(0,2,0,"3-20 4-60");
            ucKey = getkey();
            if(ucKey==KEYCANCEL)break;
            if(ucKey=='1')tmpc0=0;
            else if(ucKey=='2')tmpc0=5;
            else if(ucKey=='3')tmpc0=20;
            else if(ucKey=='4')tmpc0=60;
            else tmpc0=0;
            PrnLeftIndent_stylus(tmpc0);
        
            ScrCls();
            ucStatus = PrnStr_stylus("中国建设银行0123456789012345678901234567890123456789");
            if(ucStatus)
            {
                ScrPrint(0,6,0,"PrnStr Err:%d",ucStatus);
                getkey();
                break;
            }
            ucStatus = PrnStart_stylus();
            ScrPrint(0,0,0,"Start Print:%d",ucStatus);
            getkey();
        }
            break;            			
        default:
            break;
	    }
	}
}
#endif

