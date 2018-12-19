#include "base.h"
#include "audio.h"
#include "queue.h"

volatile unsigned char IsAudioEnabled=0;
QUEUE_T gtWaveQue;
WAVE_ATTR_T gtWaveAttr;
uchar gtWaveBuffer[MAX_AUDIO_LENGTH];

static int OpenAudioFile(uchar *szFileName, uchar *szDataOut, uint iLenIn)
{
	int fd, iFileSize;

	if(szFileName == NULL) return -1;

	fd = open(szFileName, O_RDWR);
	if(fd < 0) return -1;

	iFileSize = filesize(szFileName);
	if(iFileSize <= 0) 
	{
		close(fd);
		return -2;
	}

	if(iFileSize > iLenIn) 
		iFileSize = iLenIn;

	read(fd, szDataOut, iFileSize);
	close(fd);
	return iFileSize;
}

static int ParseWaveDat(uchar *szDataIn, WAVE_FILE_T *ptWaveFile)
{
	int iFIdx=0;

	ptWaveFile->ptRIFFChunk = (RIFF_CHUNK_T *)(szDataIn+iFIdx);
	if(memcmp(ptWaveFile->ptRIFFChunk->szRiffID, "RIFF", 4)) return	-1;
	if(memcmp(ptWaveFile->ptRIFFChunk->szRiffFormat, "WAVE", 4)) return -2;

	iFIdx += sizeof(RIFF_CHUNK_T);
	ptWaveFile->ptFormatChunk = (FORMAT_CHUNK_T *)(szDataIn+iFIdx);
	if(memcmp(ptWaveFile->ptFormatChunk->szFmtID, "fmt ", 4)) return -3;

	if ((ptWaveFile->ptFormatChunk->tWaveFormat.usChannels != 2) &&
		(ptWaveFile->ptFormatChunk->tWaveFormat.usChannels != 1)) return -4;

	if ((ptWaveFile->ptFormatChunk->tWaveFormat.uiSamplesPerSec > 48000) ||
		(ptWaveFile->ptFormatChunk->tWaveFormat.uiSamplesPerSec < 8000)) return -5;


	if ((ptWaveFile->ptFormatChunk->tWaveFormat.usBitsPerSample != 8) &&
	    (ptWaveFile->ptFormatChunk->tWaveFormat.usBitsPerSample != 16))	return -6;

	iFIdx += sizeof(ptWaveFile->ptFormatChunk->szFmtID);
    iFIdx += sizeof(ptWaveFile->ptFormatChunk->uiFmtSize);
   	iFIdx += ptWaveFile->ptFormatChunk->uiFmtSize;

	ptWaveFile->ptFactChunk = (FACT_CHUNK_T *)(szDataIn+iFIdx);
	if(0 == memcmp(ptWaveFile->ptFactChunk->szFactID, "fact", 4))
	{
		iFIdx += sizeof(ptWaveFile->ptFactChunk->szFactID);
		iFIdx += sizeof(ptWaveFile->ptFactChunk->uiFactSize);
		iFIdx += ptWaveFile->ptFactChunk->uiFactSize;
	}

	ptWaveFile->ptDataChunk  = (DATA_CHUNK_T *)(szDataIn+iFIdx);
	if(memcmp(ptWaveFile->ptDataChunk->szDataID, "data", 4)) return -7;

	return 0;
}

uint guiAudioVolumeValue = DEFAULT_AUDIO_VOLUME;

int SysGetAudioVolume(void)
{
	int ret;
	uchar audioVolume;
	char buff[100];
	int 	i;

	memset(buff, 0, sizeof(buff));
    ret = SysParaRead(SET_AUDIO_VOLUME_PARA, buff);
	if(ret < 0) return -1;

	if(strlen(buff) != 2) 
		return -1;

	for(i=0; i<strlen(buff); i++)
	{
		if(buff[i] < '0' || buff[i] > '9')
			return -1;
	}

	audioVolume  = (buff[0]-'0') * 10 + (buff[1]-'0');
	if(audioVolume <= MAX_AUDIO_VOLUME) 
    	return audioVolume;
	else
		return -1;
}

int SysSetAudioVolume(uchar volume)
{
    char buff[3];

	if(volume > MAX_AUDIO_VOLUME) return -1;

	buff[0] = volume % 100 / 10 + '0';
    buff[1] = volume % 10 + '0';
	buff[2] = '\0';

    if(SysParaWrite(SET_AUDIO_VOLUME_PARA, buff) == 0)
	{
		guiAudioVolumeValue = volume;
	   	return 0;
	}
	return -1;
}

int AudioVolumeInit()
{
	char volume;
	int mach;
	mach = get_machine_type();
	if ((mach != S300) && (mach != S800) && (mach != S900))
		return;
	bcm5892_i2s_init();

	volume = SysGetAudioVolume();
	if(volume < 0)
	{
	   	SysSetAudioVolume(DEFAULT_AUDIO_VOLUME);
		return 0;
	}

	guiAudioVolumeValue = volume;
}

static int PlayWAV(WAVE_FILE_T *ptWaveFile)
{
	int len, i;
	uchar *pData;
	char volume = guiAudioVolumeValue;
	
	if(volume <= 0) return -10;

	i2s_param_set(ptWaveFile->ptFormatChunk->tWaveFormat.usChannels, 
			ptWaveFile->ptFormatChunk->tWaveFormat.uiSamplesPerSec);

	if(ptWaveFile->ptDataChunk->uiDataSize > MAX_AUDIO_LENGTH)
		len = MAX_AUDIO_LENGTH;
	else
		len = ptWaveFile->ptDataChunk->uiDataSize;

	pData = (uchar *)&ptWaveFile->ptDataChunk->puaRawData;

	QueueInit(&gtWaveQue, gtWaveBuffer, sizeof(gtWaveBuffer));
	for (i = 0; i < len; i++)
	{
		if (!QueuePutc(&gtWaveQue, pData[i]))
			break;
	}

	gtWaveAttr.usChannels =  ptWaveFile->ptFormatChunk->tWaveFormat.usChannels;
	gtWaveAttr.uiSamplesPerSec = ptWaveFile->ptFormatChunk->tWaveFormat.uiSamplesPerSec;
	gtWaveAttr.usBitsPerSample = ptWaveFile->ptFormatChunk->tWaveFormat.usBitsPerSample;

	i2s_phy_write(&gtWaveQue, ptWaveFile->ptFormatChunk->tWaveFormat.usBitsPerSample); 
	return 0;
}

static int PlayWaveFile(uchar *uaAudioData, int len)
{
	WAVE_FILE_T tWaveFile;
	int ret;

	ret = ParseWaveDat(uaAudioData, &tWaveFile);
	if(ret) return AUDIO_INVALID_FORMAT;

	ret = PlayWAV(&tWaveFile);
	if (ret == -10)
		return AUDIO_VOLUME_ERROR;
	else if (ret)
		return AUDIO_OTHER_ERROR;

	return 0;
}

/* reserve for future use */
static int PlayMP3File(uchar *uaFileData, int len)
{
	return AUDIO_INVALID_FORMAT;
}

//************************************************
//				API Definition                  
//************************************************

/* play audio file in non-blocking mode */
int SoundPlay(char *param, uchar type)
{
	uchar uaFileData[MAX_AUDIO_LENGTH];
	int ret, iDataLen;
	int mach;

    //printk("%s,%d\n",__func__,type);

    if(param==NULL) return -1;
    if(type==AUDIO_FREQ)return s_SoundPlayFreq(param);
	
	mach = get_machine_type();
	if ((mach != S300) && (mach != S800) && (mach != S900))
		return AUDIO_INVALID_FORMAT;
	if ((type != AUDIO_WAV) && (type != AUDIO_MP3)) 
		return AUDIO_INVALID_FORMAT;
	
    switch(type)
    {
    case AUDIO_WAV:
		iDataLen = OpenAudioFile(param, uaFileData, sizeof(uaFileData));
		if(iDataLen < 0) return AUDIO_FILE_ERROR;
		return PlayWaveFile(uaFileData, iDataLen);

    case AUDIO_MP3:
		return PlayMP3File(uaFileData, iDataLen);

    default:return AUDIO_INVALID_FORMAT;
    }
}

/* end of audio.c */
