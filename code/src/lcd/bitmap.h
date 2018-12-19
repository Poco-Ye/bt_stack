#ifndef __BITMAP_H__
#define __BITMAP_H__

#include "base.h"

typedef struct __TAG_BITMAP_FILE_HEADER
{
	ushort bfType;              //BitFile Type,
	uint bfSize;
	ushort bfReserved1;
	ushort bfReserved2;
	uint bfOffBits;
} __attribute__ ((packed)) TAG_BITMAP_FILE_HEADER;

typedef struct __TAG_BITMAP_INFO_HEADER
{
	uint biSize;
	long biWidth;
	long biHeight;
	ushort biPlanes;
	ushort biBitCount;
	uint biCompression;
	uint biSizeImage;
	long biXPelsPerMeter;
	long biYPelsPerMeter;
	uint biClrUsed;
	uint biClrImportant;
} __attribute__ ((packed))TAG_BITMAP_INFO_HEADER;

typedef struct __BITMAP_FILE_INFO
{
	TAG_BITMAP_FILE_HEADER FileHeader;
	TAG_BITMAP_INFO_HEADER BmpHeader;
} __attribute__ ((packed)) BITMAP_FILE_INFO;

#endif /* end of __BITMAP_H__ */

