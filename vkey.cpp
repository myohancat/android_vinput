#include "vkey.h"

#include <ctype.h>
#include <sys/types.h> 
#include <sys/poll.h> 
#include <string.h>
#include <unistd.h>

#define TIMEOUT     (200) // 200ms
#define CRLF        "\r\n"
#define ASCII_ESC   '\x1B'

#define INIT_VKEY(sVKey)    do{                                 \
                                sVKey.mCode  = KEYCODE_UNKNOWN; \
                                sVKey.mValue = ' ';             \
                            }while(0)

typedef struct KeyBind_s
{
    KeyCode_e     mCode;
    const char*   mString;
}KeyBind_t;

static const KeyBind_t CONV_TABLE_ASCII[] =
{
    { KEYCODE_BACKSPACE,    "\b"   },
    { KEYCODE_RETURN,       "\n"   },
    { KEYCODE_RETURN,       "\r"   },
    { KEYCODE_TAB,          "\t"   },
    { KEYCODE_BEGIN_OF_LINE,"\x01" },
    { KEYCODE_ARROW_LEFT,   "\x02" },
    { KEYCODE_CANCEL_TEXT,  "\x03" },
    { KEYCODE_END_OF_LINE,  "\x04" },
    { KEYCODE_ARROW_RIGHT,  "\x06" },
    { KEYCODE_BACKSPACE,    "\x08" },
    { KEYCODE_END_OF_LINE,  "\x05" },
    { KEYCODE_CLEAR_EOL,    "\x0B" },
    { KEYCODE_ARROW_DOWN,   "\x0E" },
    { KEYCODE_ARROW_UP,     "\x10" },
    { KEYCODE_CLEAR_LINE,   "\x15" },
    { KEYCODE_BACKSPACE,    "\x7F" },
};

static const KeyBind_t CONV_TABLE_ESC_SEQ[] =
{
    { KEYCODE_BEGIN_OF_LINE,"\x1B[1~"   },
    { KEYCODE_INSERT,       "\x1B[2~"   },
    { KEYCODE_DELETE,       "\x1B[3~"   },
    { KEYCODE_END_OF_LINE,  "\x1B[4~"   },
    { KEYCODE_PAGE_UP,      "\x1B[5~"   },
    { KEYCODE_PAGE_DOWN,    "\x1B[6~"   },
    { KEYCODE_BEGIN_OF_LINE,"\x1B[7~"   },
    { KEYCODE_END_OF_LINE,  "\x1B[8~"   },
    { KEYCODE_CTRL_LEFT,    "\x1B[1;5D" },
    { KEYCODE_CTRL_RIGHT,   "\x1B[1;5C" },
    { KEYCODE_ARROW_UP,     "\x1B[A"    },
    { KEYCODE_ARROW_DOWN,   "\x1B[B"    },
    { KEYCODE_ARROW_RIGHT,  "\x1B[C"    },
    { KEYCODE_ARROW_LEFT,   "\x1B[D"    },
    { KEYCODE_END_OF_LINE,  "\x1B[F"    },
    { KEYCODE_BEGIN_OF_LINE,"\x1B[H"    },
    { KEYCODE_F1,           "\x1BOP"    },
    { KEYCODE_F2,           "\x1BOQ"    },
    { KEYCODE_F3,           "\x1BOR"    },
    { KEYCODE_F4,           "\x1BOS"    },
    { KEYCODE_F5,           "\x1B[15~"  },
    { KEYCODE_F6,           "\x1B[17~"  },
    { KEYCODE_F7,           "\x1B[18~"  },
    { KEYCODE_F8,           "\x1B[19~"  },
    { KEYCODE_F9,           "\x1B[20~"  },
    { KEYCODE_F10,          "\x1B[21~"  },
    { KEYCODE_F11,          "\x1B[23~"  },
    { KEYCODE_F12,          "\x1B[24~"  },
};

static int _ReadKey(int nFd, char* pKey)
{
    unsigned int    nReadChar = -1;
    struct pollfd   sPollFd;
    int ret;
    
    sPollFd.fd = nFd;
    sPollFd.events = POLLPRI|POLLIN|POLLERR|POLLHUP|POLLNVAL|POLLRDHUP;
    sPollFd.revents = 0;

    ret = poll(&sPollFd, 1, TIMEOUT);
    if (ret > 0)
    {
        if(sPollFd.revents & (POLLRDHUP | POLLERR | POLLHUP | POLLNVAL))
            return -1;

        if (sPollFd.revents & POLLIN)
        {
            nReadChar = read(nFd, pKey, 1);
            if(nReadChar < 1)
                return -1;
        }
    }
    else if (ret == 0)
    {
        // TIMEOUT
        return 0;
    }
    
    return 1;
}

static int _GetVKeyFromASCII(VKey_t* pVKey, char cIn)
{
    unsigned int ii;
    
    for(ii = 0; ii < sizeof(CONV_TABLE_ASCII)/sizeof(KeyBind_t); ii++)
    {
        if(CONV_TABLE_ASCII[ii].mString[0] == cIn)
        {
            pVKey->mCode  = CONV_TABLE_ASCII[ii].mCode;
            pVKey->mValue = ' ';
            return 1;
        }
    }

    return 0;
}

static int _GetVKeyFromEscSeq(int nFd, VKey_t* pVKey)
{
    unsigned char abEscSeqBuf[8];
    char          cIn; 
    int           nIdx = 0;
    int           ret;
    
    abEscSeqBuf[nIdx++] = ASCII_ESC;
    abEscSeqBuf[nIdx]   = '\0';

    do
    {
        ret = _ReadKey(nFd, &cIn);
        if (ret < 0)
            return -1;

        /* TIMEOUT */
        if (ret == 0)
        {
            pVKey->mCode = KEYCODE_ESC;
            pVKey->mValue = 0;
            return 1;
        }

    }while(cIn == ASCII_ESC);

    if(cIn != '[' && cIn != 'O')
    {
        pVKey->mCode  = KEYCODE_CHAR;
        pVKey->mValue = cIn;
    
        return 1;
    }

    abEscSeqBuf[nIdx++] = cIn;
    abEscSeqBuf[nIdx]   = '\0';

    while(nIdx < 7)
    {
        unsigned int ii;
        ret = _ReadKey(nFd, &cIn);
        if (ret <= 0)
            return ret;
        
        abEscSeqBuf[nIdx++] = cIn;
        abEscSeqBuf[nIdx]   = '\0';
        
        for(ii = 0; ii < sizeof(CONV_TABLE_ESC_SEQ)/sizeof(KeyBind_t); ii++)
        {
            if(!strcmp((char *)abEscSeqBuf, (char *)CONV_TABLE_ESC_SEQ[ii].mString))
            {
                pVKey->mCode  = CONV_TABLE_ESC_SEQ[ii].mCode;
                pVKey->mValue = ' ';
                return 1;
            }
        }
    }
    
    INIT_VKEY((*pVKey));
    
    return 0;
}

int ReadVKey(int nFd, VKey_t* pVKey)
{
    char      cIn;
    int       ret;

    ret = _ReadKey(nFd, &cIn);
    if(ret <= 0)
        return ret;

    INIT_VKEY((*pVKey));

    if(isprint(cIn))
    {
        pVKey->mCode  = KEYCODE_CHAR;
        pVKey->mValue = cIn;
    }
    else if(cIn != ASCII_ESC)
    {
        ret = _GetVKeyFromASCII(pVKey, cIn);
        if (ret <= 0)
            return ret; 
    }
    else
    {    
        ret = _GetVKeyFromEscSeq(nFd, pVKey);
        if (ret <= 0)
            return ret;
    }

    return 1;
}

