#ifndef _PIC_H_
#define _PIC_H_
#pragma pack( push, 1 )

#include "Picture.h"



struct PICHeader
{
    unsigned long int magic;
    float version;
    unsigned char szComment[80];
    unsigned char PICT[4];
    unsigned short int nWidth;
    unsigned short int nHeight;
    unsigned long int nAspectRatio;
    unsigned short int nFields;
    unsigned short int _pad;
};

#define PIC_FIELD_NONE      (0)
#define PIC_FIELD_ODD       (1)
#define PIC_FIELD_EVEN      (2)
#define PIC_FIELD_FULLFRAME (3)


struct PICChannelInfo
{
    unsigned char isChained;
    unsigned char nSize;
    unsigned char type;
    unsigned char channel;
};

#define PIC_CHANNELTYPE_UNCOMPRESSED     (0x00)
#define PIC_CHANNELTYPE_MIXED_RUN_LENGTH (0x02)

#define PIC_CHANNELCODE_RED    (0x80)
#define PIC_CHANNELCODE_GREEN  (0x40)
#define PIC_CHANNELCODE_BLUE   (0x20)
#define PIC_CHANNELCODE_ALPHA  (0x10)



extern bool LoadPIC( const char* pszFilename, MMRGBA &mmrgba, unsigned long int dwFlags );

#pragma pack( pop )
#endif //_PIC_H_
