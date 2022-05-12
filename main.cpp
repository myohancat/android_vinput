#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <termios.h>
#include <errno.h>
#include <linux/uinput.h>
#include <fcntl.h>
#include <unistd.h>

#include "vkey.h"

static int        gTermFd;
struct termios  gOldTermConf;

static int        gUinputFd;

static int     exit_program = 0;

static void usr_sig_handler(int sig)
{
    printf("\nTerminating by Ctrl+C.\n");

    exit_program = 1;

    return;
}

#define CMIN  1
#define CTIME 1

static void terminal_init()
{
    struct termios newInputTty;

    gTermFd = fileno(stdin);
    tcgetattr(gTermFd, &gOldTermConf);

    newInputTty             = gOldTermConf;
    newInputTty.c_iflag     =  0;      /* input mode  */
    newInputTty.c_lflag    &= ~ICANON; /* line settings  */
    newInputTty.c_lflag    &= ~ECHO;   /* disable echo  */
    newInputTty.c_cc[VMIN]  = CMIN;    /* minimum chars to wait for */
    newInputTty.c_cc[VTIME] = CTIME;   /* minimum time to wait */

    tcsetattr(gTermFd, TCSAFLUSH, &newInputTty);
}
    

static void terminal_deinit()
{
    tcsetattr(gTermFd, TCSAFLUSH, &gOldTermConf);
}

typedef struct KeymapTable_s {
    VKey_t        mVKey;
    int           mKeyCode;
}KeymapTable_t;

#define VKEY(code, value)     { .mCode = code, .mValue = value }
static KeymapTable_t gKeymapTable[] = 
{
    { VKEY(KEYCODE_BEGIN_OF_LINE, 0  ),  KEY_HOMEPAGE },
    { VKEY(KEYCODE_ARROW_UP,      0  ),  KEY_UP },
    { VKEY(KEYCODE_ARROW_DOWN,    0  ),  KEY_DOWN },
    { VKEY(KEYCODE_ARROW_LEFT,    0  ),  KEY_LEFT },
    { VKEY(KEYCODE_ARROW_RIGHT,   0  ),  KEY_RIGHT },
    { VKEY(KEYCODE_RETURN,        0  ),  KEY_ENTER },
    { VKEY(KEYCODE_BACKSPACE,     0  ),  KEY_BACK },
    { VKEY(KEYCODE_ESC,           0  ),  KEY_BACK },
    { VKEY(KEYCODE_CHAR,          ' '),  KEY_SELECT },
    { VKEY(KEYCODE_CHAR,          '+'),  KEY_VOLUMEUP },
    { VKEY(KEYCODE_CHAR,          '-'),  KEY_VOLUMEDOWN },
    { VKEY(KEYCODE_PAGE_UP,       '+'),  KEY_CHANNELUP },
    { VKEY(KEYCODE_PAGE_DOWN,     '-'),  KEY_CHANNELDOWN },
};

static int uinput_init(void)
{   
    struct uinput_user_dev dev;
    int i;
    
    gUinputFd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (gUinputFd < 0)
    {   
        fprintf(stderr, "Can't open input device %s (%d)\n", strerror(errno), errno);
        return -1;
    }
    
    memset(&dev, 0, sizeof(dev));
    
    snprintf(dev.name, UINPUT_MAX_NAME_SIZE -1, "INNO_IR");
    dev.id.bustype = BUS_USB;
    dev.id.vendor = 0x0001;
    dev.id.product = 0x0001;
    dev.id.version = 0x0001;
    
    if (write(gUinputFd, &dev, sizeof(dev)) < 0)
    {   
        fprintf(stderr, "Can't write device information %s (%d)\n", strerror(errno), errno);
        close(gUinputFd);
        gUinputFd = -1;
        return -1;
    }
    
    ioctl(gUinputFd, UI_SET_EVBIT, EV_KEY);
//  ioctl(gUinputFd, UI_SET_EVBIT, EV_SYN);
//  ioctl(gUinputFd, UI_SET_EVBIT, EV_REL);
//  ioctl(gUinputFd, UI_SET_EVBIT, EV_REP);

    for (int ii = 0; ii < sizeof(gKeymapTable) / sizeof(gKeymapTable[0]); ii++)
        ioctl(gUinputFd, UI_SET_KEYBIT, gKeymapTable[ii].mKeyCode);

    if (ioctl(gUinputFd, UI_DEV_CREATE) < 0)
    {   
        fprintf(stderr, "Can't create uinput device %s (%d)\n", strerror(errno), errno);
        close(gUinputFd);
        gUinputFd = -1;
        return -1;
    }
    
    return 0;
}

static int send_event(uint16_t keycode)
{
    struct input_event event;

    if (gUinputFd == -1)
    {
        fprintf(stderr, "uinput device is not open\n");
        return -1;
    }

    memset (&event, 0, sizeof(event));
    gettimeofday(&event.time, NULL);
    event.type = EV_KEY;
    event.code = keycode;
    event.value = 1;
    write(gUinputFd, &event, sizeof(event));

    usleep(50000);
    event.type = EV_SYN;
    event.code = SYN_REPORT;
    event.value = 0;
    write(gUinputFd, &event, sizeof(event));

    gettimeofday(&event.time, NULL);
    event.type = EV_KEY;
    event.code = keycode;
    //event.value = 0;
    write(gUinputFd, &event, sizeof(event));

    event.type = EV_SYN;
    event.code = SYN_REPORT;
    write(gUinputFd, &event, sizeof(event));

    return 0;
}

static int uinput_deinit(void)
{
    if (gUinputFd >= 0)
    {
        ioctl(gUinputFd, UI_DEV_DESTROY);
        close(gUinputFd);
        gUinputFd = -1;
    }

    return 0;
}

void print_usage()
{
    printf("'[q|Q]'                 quit program\n");
    printf("====================================\n");
    printf("HOME            -->  KEY_HOMPAGE\n");
    printf("UP              -->  KEY_UP\n");
    printf("DOWN            -->  KEY_DOWN\n");
    printf("LEFT            -->  KEY_LEFT\n");
    printf("RIGHT           -->  KEY_RIGHT\n");
    printf("ENTER           -->  KEY_ENTER\n");
    printf("SPACE           -->  KEY_SELECT\n");
    printf("ESC | BACKSPACE -->  KEY_BACK\n");
    printf("'+'             -->  KEY_VOLUMEUP\n");
    printf("'-'             -->  KEY_VOLUMEDOWN\n");
    printf("PAGE_UP         -->  KEY_CHANNELUP\n");
    printf("PAGE_DOWN       -->  KEY_CHANNELDOWN\n");
    printf("====================================\n");
    printf("Input Key >"); fflush(stdout);

}

int main(int argc, char** argv)
{
    VKey_t vkey;
    int    ret;

    terminal_init();
    uinput_init();

    signal(SIGINT,  usr_sig_handler);

    while(!exit_program)
    {
        print_usage();

        do
        {
            ret = ReadVKey(1, &vkey);
        } while (ret == 0);

        if (ret < 0)
        {
            printf("\n");
            continue;
        }

        if ((vkey.mCode == KEYCODE_CHAR) && (vkey.mValue == 'q' || vkey.mValue == 'Q'))
        {
            exit_program = 1;
            break;
        }

        for (int ii = 0; ii < sizeof(gKeymapTable) / sizeof(gKeymapTable[0]); ii++)
        {
            if (gKeymapTable[ii].mVKey.mCode == vkey.mCode && 
                (gKeymapTable[ii].mVKey.mCode != KEYCODE_CHAR || gKeymapTable[ii].mVKey.mValue == vkey.mValue))
            {
                send_event(gKeymapTable[ii].mKeyCode);
                break;
            }
        }
        printf("\n");
    }    

    printf("\n");
    uinput_deinit();
    terminal_deinit();

    return 0;
}
