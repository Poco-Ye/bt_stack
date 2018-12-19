#ifndef __LCD_KB_H__
#define __LCD_KB_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

void s_LcdInit(void);
void ScrCls(void);
void ScrClrLine(unsigned char startline, unsigned char endline);
void ScrBackLight(unsigned char mode);
void ScrGray(unsigned char level);
void ScrGotoxy(unsigned char x, unsigned char y);
void ScrSetIcon(unsigned char icon_no, unsigned char mode);
int  printf(const char *fmt,...);
unsigned char ScrFontSet(unsigned char font_type);
unsigned char ScrAttrSet(unsigned char attr);


#endif /* end of __LCD_KB_H__ */
