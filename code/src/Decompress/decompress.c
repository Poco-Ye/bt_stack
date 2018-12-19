#include <string.h>
#include "posapi.h"


static ulong CharSym[320], SymChar[320];
static ulong SymFreq[320], SymCum[320], PosCum[4100];

int Decompress(uchar *srcbuf, uchar *dstbuf, ulong srclen, ulong *dstlen);

static ulong GetSym(ulong x)
{
    ulong i, j, k;

    i = 1;
    j = 314;
    while (i < j)
    {
        k = (i + j) / 2;
        if (SymCum[k] > x)
            i = k + 1;
        else
            j = k;
    }
    return i;
}

static ulong GetPos(ulong x)
{
    ulong i, j, k;

    i = 1;
    j = 4096;
    while (i < j)
    {
        k = (i + j) / 2;
        if (PosCum[k] > x)
            i = k + 1;
        else
            j = k;
    }
    return (i - 1);
}

static void UpdateNode(ulong sym)
{
    ulong i, j, k, ch;

    if (SymCum[0] >= 0x7FFF)
    {
        j = 0;
        for (i = 314; i > 0; i--)
        {
            SymCum[i] = j;
            j += (SymFreq[i] = (SymFreq[i] + 1) >> 1);
        }
        SymCum[0] = j;
    }
    for (i = sym; SymFreq[i] == SymFreq[i - 1]; i--) ;
    if (i < sym)
    {
        k = SymChar[i];
        ch = SymChar[sym];
        SymChar[i] = ch;
        SymChar[sym] = k;
        CharSym[k] = sym;
        CharSym[ch] = i;
    }
    SymFreq[i]++;
    while (--i > 0)
        SymCum[i]++;
    SymCum[0]++;
}

int Decompress(uchar *srcbuf, uchar *dstbuf, ulong srclen, ulong *dstlen)
{
    uchar ch, mask, *srcend, *dstend, *sp, *dp;
    ulong i, r, j, k, c, low, high, value, range, sym;
    uchar DataBuf[4200];

    ch = 0;
    mask = 0;
    low = 0;
    value = 0;
    high = 0x20000;
    sp = srcbuf;
    dp = dstbuf;

    i = *sp++;
    i <<= 8;
    i += *sp++;
    i <<= 8;
    i += *sp++;
    i <<= 8;
    i += *sp++;
    *dstlen=i;

    srcend = srcbuf + srclen;
    dstend = dstbuf + i;
    for (i = 0; i < 15 + 2; i++)
    {
        value *= 2;
        if ((mask >>= 1) == 0)
        {
            ch = (sp >= srcend) ? 0 : *(sp++);
            mask = 128;
        }
        value += ((ch & mask) != 0);
    }

    SymCum[314] = 0;

    for (k = 314; k >= 1; k--)
    {
        j = k - 1;
        CharSym[j] = k;
        SymChar[k] = j;
        SymFreq[k] = 1;
        SymCum[k - 1] = SymCum[k] + SymFreq[k];
    }

    SymFreq[0] = 0;
    PosCum[4096] = 0;
    for (i = 4096; i >= 1; i--)
        PosCum[i - 1] = PosCum[i] + 10000 / (i + 200);

    for (i = 0; i < 4096 - 60; i++)
        DataBuf[i] = ' ';
    
	r = 4096 - 60;

    while (dp < dstend)
    {
        range = high - low;
        sym = GetSym((ulong)(((value - low + 1) * SymCum[0] - 1) / range));
        high = low + (range * SymCum[sym - 1]) / SymCum[0];
        low += (range * SymCum[sym]) / SymCum[0];
        while(1)
        {
            if (low >= 0x10000)
            {
                value -= 0x10000;
                low -= 0x10000;
                high -= 0x10000;
            }
            else if (low >= 0x8000 && high <= 0x18000)
            {
                value -= 0x8000;
                low -= 0x8000;
                high -= 0x8000;
            }
            else if (high > 0x10000)
                break;
            low += low;
            high += high;
            value *= 2;
            if ((mask >>= 1) == 0)
            {
                ch = (sp >= srcend) ? 0 : *(sp++);
                mask = 128;
            }
            value += ((ch & mask) != 0);
        }
        c = SymChar[sym];
        UpdateNode(sym);
        if (c < 256)
        {
            if (dp >= dstend)
                return -1;
            *(dp++) = (uchar)c;
            DataBuf[r++] = (uchar)c;
            r &= 4095;
        }
        else
        {
            j = c - 255 + 2;
            range = high - low;
            i = GetPos((ulong)
                (((value - low + 1) * PosCum[0] - 1) / range));
            high = low + (range * PosCum[i    ]) / PosCum[0];
            low += (range * PosCum[i + 1]) / PosCum[0];
            while(1)
            {
                if (low >= 0x10000)
                {
                    value -= 0x10000;
                    low -= 0x10000;
                    high -= 0x10000;
                }
                else if (low >= 0x8000 && high <= 0x18000)
                {
                    value -= 0x8000;
                    low -= 0x8000;
                    high -= 0x8000;
                }
                else if (high > 0x10000)
                    break;
                low += low;
                high += high;
                value *= 2;
                if ((mask >>= 1) == 0)
                {
                    ch = (sp >= srcend) ? 0 : *(sp++);
                    mask = 128;
                }
                value += ((ch & mask) != 0);
            }
            i = (r - i - 1) & 4095;
            for (k = 0; k < j; k++)
            {
                c = DataBuf[(i + k) & 4095];
                if (dp >= dstend)
                    return -1;
                *(dp++) = (uchar)c;
                DataBuf[r++] = (uchar)c;
                r &= 4095;
            }
        }
    }
    return 0;
}

