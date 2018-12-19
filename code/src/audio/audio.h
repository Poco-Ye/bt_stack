#ifndef __AUDIO_H__
#define __AUDIO_H__

#define audio_print		printk

#define MAX_AUDIO_LENGTH		400*1024
#define MAX_AUDIO_VOLUME 		10
#define MIN_AUDIO_VOLUME 		0
#define DEFAULT_AUDIO_VOLUME	5

/* return code */
#define AUDIO_RET_OK			0
#define AUDIO_FILE_ERROR		-1	
#define AUDIO_INVALID_FORMAT	-2
#define AUDIO_VOLUME_ERROR		-3
#define AUDIO_DEV_BUSY			-4
#define AUDIO_OTHER_ERROR		-255

enum AUDIO_FORMAT
{
	AUDIO_WAV=1,
    AUDIO_FREQ,
	AUDIO_MP3,
};

typedef struct
{
	uchar uaErrCode[64];
	uchar iIdx;
}ERROR_MSG;

typedef struct 
{
	char szRiffID[4];
	uint uiSize;
	char szRiffFormat[4];
}RIFF_CHUNK_T;

typedef struct 
{
	ushort usFormatTag;
	ushort usChannels;
	uint uiSamplesPerSec;
	uint uiAvgBytesPerSec;
	ushort usBlockAlign;
	ushort usBitsPerSample;
	ushort usAuxiliaryWord;
}WAVE_FORMAT_T;

typedef struct
{
	char szFmtID[4];
	uint uiFmtSize;
	WAVE_FORMAT_T tWaveFormat;
}FORMAT_CHUNK_T;

typedef struct 
{
	char szFactID[4];
	uint uiFactSize;
	uchar *pFactData;
}FACT_CHUNK_T;

typedef struct 
{
	char szDataID[4];
	uint uiDataSize;
	uchar *puaRawData;
}DATA_CHUNK_T;

typedef struct 
{
	RIFF_CHUNK_T 	*ptRIFFChunk;
	FORMAT_CHUNK_T 	*ptFormatChunk;
	FACT_CHUNK_T	*ptFactChunk;
	DATA_CHUNK_T	*ptDataChunk;
}WAVE_FILE_T;

typedef struct 
{
	ushort usChannels;
	uint uiSamplesPerSec;
	ushort usBitsPerSample;
}WAVE_ATTR_T;

#endif /* end of __AUDIO_API_H__ */

