#include <SDL2/SDL.h>
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <gbm.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <array>


#define OPENGL_SOCKET "/tmp/webkit_opengl"
static int epdc_fd = -1;
static int graphic_server_fd = -1;
static int control_fd = -1;
static int running = 1;
static pthread_t fetch_image_thread;
static int gbm_fd = -1;
static struct gbm_device* gbm_device;
#define SCALE_FACTOR 2
#define XWIDTH_ORI 1664
#define XHEIGHT_ORI 2304
#define XWIDTH_SCALE XWIDTH_ORI/SCALE_FACTOR
#define XHEIGHT_SCALE XHEIGHT_ORI/SCALE_FACTOR
static char *fb_mem;
static char *fb_mem_scale;
#define G_TYPE 23123
#define bmask 0x000000ff
#define gmask 0x0000ff00
#define rmask 0x00ff0000
#define amask 0xff000000
#define MAGIC_NUM 0xf1234567

#define WAVEFORM_MODE_INIT  0x0 /* Screen goes to white (clears) */
#define WAVEFORM_MODE_DU    0x1 /* Grey->white/grey->black */
#define WAVEFORM_MODE_GC16  0x2 /* High fidelity (flashing) */
#define WAVEFORM_MODE_GC4   0x3 /* Lower fidelity */
#define WAVEFORM_MODE_A2    0x4 /* Fast black/white animation */

 #define EVENT_TOUCH_DOWN 0xa1
    #define EVENT_TOUCH_UP 0xa2
    #define EVENT_TOUCH_MOTION 0xa3
    #define EVENT_KEYBOARD_DOWN 0xa4
    #define EVENT_KEYBOARD_UP 0xa5

struct ipc_header_t
{
    int magic;
    unsigned int left; 
    unsigned int top;
    unsigned int width;
    unsigned int height;
    int wave_mode;
    int wait_for_complete;
    unsigned int datalength;
};


struct event_header_t
{ 
  unsigned char type;
  int x;
  int y;
  uint32_t keycode;
};


unsigned short convert_keycode(const SDL_Scancode &scan_code) {
    static const std::array<SDL_Scancode, 249> code_map = {{
    SDL_SCANCODE_UNKNOWN,        /*  KEY_RESERVED        0 */
    SDL_SCANCODE_ESCAPE,         /*  KEY_ESC         1 */
    SDL_SCANCODE_1,              /*  KEY_1           2 */
    SDL_SCANCODE_2,              /*  KEY_2           3 */
    SDL_SCANCODE_3,              /*  KEY_3           4 */
    SDL_SCANCODE_4,              /*  KEY_4           5 */
    SDL_SCANCODE_5,              /*  KEY_5           6 */
    SDL_SCANCODE_6,              /*  KEY_6           7 */
    SDL_SCANCODE_7,              /*  KEY_7           8 */
    SDL_SCANCODE_8,              /*  KEY_8           9 */
    SDL_SCANCODE_9,              /*  KEY_9           10 */
    SDL_SCANCODE_0,              /*  KEY_0           11 */
    SDL_SCANCODE_MINUS,          /*  KEY_MINUS       12 */
    SDL_SCANCODE_EQUALS,         /*  KEY_EQUAL       13 */
    SDL_SCANCODE_BACKSPACE,      /*  KEY_BACKSPACE       14 */
    SDL_SCANCODE_TAB,            /*  KEY_TAB         15 */
    SDL_SCANCODE_Q,              /*  KEY_Q           16 */
    SDL_SCANCODE_W,              /*  KEY_W           17 */
    SDL_SCANCODE_E,              /*  KEY_E           18 */
    SDL_SCANCODE_R,              /*  KEY_R           19 */
    SDL_SCANCODE_T,              /*  KEY_T           20 */
    SDL_SCANCODE_Y,              /*  KEY_Y           21 */
    SDL_SCANCODE_U,              /*  KEY_U           22 */
    SDL_SCANCODE_I,              /*  KEY_I           23 */
    SDL_SCANCODE_O,              /*  KEY_O           24 */
    SDL_SCANCODE_P,              /*  KEY_P           25 */
    SDL_SCANCODE_LEFTBRACKET,    /*  KEY_LEFTBRACE       26 */
    SDL_SCANCODE_RIGHTBRACKET,   /*  KEY_RIGHTBRACE      27 */
    SDL_SCANCODE_RETURN,         /*  KEY_ENTER       28 */
    SDL_SCANCODE_LCTRL,          /*  KEY_LEFTCTRL        29 */
    SDL_SCANCODE_A,              /*  KEY_A           30 */
    SDL_SCANCODE_S,              /*  KEY_S           31 */
    SDL_SCANCODE_D,              /*  KEY_D           32 */
    SDL_SCANCODE_F,              /*  KEY_F           33 */
    SDL_SCANCODE_G,              /*  KEY_G           34 */
    SDL_SCANCODE_H,              /*  KEY_H           35 */
    SDL_SCANCODE_J,              /*  KEY_J           36 */
    SDL_SCANCODE_K,              /*  KEY_K           37 */
    SDL_SCANCODE_L,              /*  KEY_L           38 */
    SDL_SCANCODE_SEMICOLON,      /*  KEY_SEMICOLON       39 */
    SDL_SCANCODE_APOSTROPHE,     /*  KEY_APOSTROPHE      40 */
    SDL_SCANCODE_GRAVE,          /*  KEY_GRAVE       41 */
    SDL_SCANCODE_LSHIFT,         /*  KEY_LEFTSHIFT       42 */
    SDL_SCANCODE_BACKSLASH,      /*  KEY_BACKSLASH       43 */
    SDL_SCANCODE_Z,              /*  KEY_Z           44 */
    SDL_SCANCODE_X,              /*  KEY_X           45 */
    SDL_SCANCODE_C,              /*  KEY_C           46 */
    SDL_SCANCODE_V,              /*  KEY_V           47 */
    SDL_SCANCODE_B,              /*  KEY_B           48 */
    SDL_SCANCODE_N,              /*  KEY_N           49 */
    SDL_SCANCODE_M,              /*  KEY_M           50 */
    SDL_SCANCODE_COMMA,          /*  KEY_COMMA       51 */
    SDL_SCANCODE_PERIOD,         /*  KEY_DOT         52 */
    SDL_SCANCODE_SLASH,          /*  KEY_SLASH       53 */
    SDL_SCANCODE_RSHIFT,         /*  KEY_RIGHTSHIFT      54 */
    SDL_SCANCODE_KP_MULTIPLY,    /*  KEY_KPASTERISK      55 */
    SDL_SCANCODE_LALT,           /*  KEY_LEFTALT     56 */
    SDL_SCANCODE_SPACE,          /*  KEY_SPACE       57 */
    SDL_SCANCODE_CAPSLOCK,       /*  KEY_CAPSLOCK        58 */
    SDL_SCANCODE_F1,             /*  KEY_F1          59 */
    SDL_SCANCODE_F2,             /*  KEY_F2          60 */
    SDL_SCANCODE_F3,             /*  KEY_F3          61 */
    SDL_SCANCODE_F4,             /*  KEY_F4          62 */
    SDL_SCANCODE_F5,             /*  KEY_F5          63 */
    SDL_SCANCODE_F6,             /*  KEY_F6          64 */
    SDL_SCANCODE_F7,             /*  KEY_F7          65 */
    SDL_SCANCODE_F8,             /*  KEY_F8          66 */
    SDL_SCANCODE_F9,             /*  KEY_F9          67 */
    SDL_SCANCODE_F10,            /*  KEY_F10         68 */
    SDL_SCANCODE_NUMLOCKCLEAR,   /*  KEY_NUMLOCK     69 */
    SDL_SCANCODE_SCROLLLOCK,     /*  KEY_SCROLLLOCK      70 */
    SDL_SCANCODE_KP_7,           /*  KEY_KP7         71 */
    SDL_SCANCODE_KP_8,           /*  KEY_KP8         72 */
    SDL_SCANCODE_KP_9,           /*  KEY_KP9         73 */
    SDL_SCANCODE_KP_MINUS,       /*  KEY_KPMINUS     74 */
    SDL_SCANCODE_KP_4,           /*  KEY_KP4         75 */
    SDL_SCANCODE_KP_5,           /*  KEY_KP5         76 */
    SDL_SCANCODE_KP_6,           /*  KEY_KP6         77 */
    SDL_SCANCODE_KP_PLUS,        /*  KEY_KPPLUS      78 */
    SDL_SCANCODE_KP_1,           /*  KEY_KP1         79 */
    SDL_SCANCODE_KP_2,           /*  KEY_KP2         80 */
    SDL_SCANCODE_KP_3,           /*  KEY_KP3         81 */
    SDL_SCANCODE_KP_0,           /*  KEY_KP0         82 */
    SDL_SCANCODE_KP_PERIOD,      /*  KEY_KPDOT       83 */
    SDL_SCANCODE_UNKNOWN,        /*  84 */
    SDL_SCANCODE_LANG5,          /*  KEY_ZENKAKUHANKAKU  85 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_102ND       86 */
    SDL_SCANCODE_F11,            /*  KEY_F11         87 */
    SDL_SCANCODE_F12,            /*  KEY_F12         88 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_RO          89 */
    SDL_SCANCODE_LANG3,          /*  KEY_KATAKANA        90 */
    SDL_SCANCODE_LANG4,          /*  KEY_HIRAGANA        91 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_HENKAN      92 */
    SDL_SCANCODE_LANG3,          /*  KEY_KATAKANAHIRAGANA    93 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_MUHENKAN        94 */
    SDL_SCANCODE_KP_COMMA,       /*  KEY_KPJPCOMMA       95 */
    SDL_SCANCODE_KP_ENTER,       /*  KEY_KPENTER     96 */
    SDL_SCANCODE_RCTRL,          /*  KEY_RIGHTCTRL       97 */
    SDL_SCANCODE_KP_DIVIDE,      /*  KEY_KPSLASH     98 */
    SDL_SCANCODE_SYSREQ,         /*  KEY_SYSRQ       99 */
    SDL_SCANCODE_RALT,           /*  KEY_RIGHTALT        100 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_LINEFEED        101 */
    SDL_SCANCODE_HOME,           /*  KEY_HOME        102 */
    SDL_SCANCODE_UP,             /*  KEY_UP          103 */
    SDL_SCANCODE_PAGEUP,         /*  KEY_PAGEUP      104 */
    SDL_SCANCODE_LEFT,           /*  KEY_LEFT        105 */
    SDL_SCANCODE_RIGHT,          /*  KEY_RIGHT       106 */
    SDL_SCANCODE_END,            /*  KEY_END         107 */
    SDL_SCANCODE_DOWN,           /*  KEY_DOWN        108 */
    SDL_SCANCODE_PAGEDOWN,       /*  KEY_PAGEDOWN        109 */
    SDL_SCANCODE_INSERT,         /*  KEY_INSERT      110 */
    SDL_SCANCODE_DELETE,         /*  KEY_DELETE      111 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_MACRO       112 */
    SDL_SCANCODE_MUTE,           /*  KEY_MUTE        113 */
    SDL_SCANCODE_VOLUMEDOWN,     /*  KEY_VOLUMEDOWN      114 */
    SDL_SCANCODE_VOLUMEUP,       /*  KEY_VOLUMEUP        115 */
    SDL_SCANCODE_POWER,          /*  KEY_POWER       116 SC System Power Down */
    SDL_SCANCODE_KP_EQUALS,      /*  KEY_KPEQUAL     117 */
    SDL_SCANCODE_KP_MINUS,       /*  KEY_KPPLUSMINUS     118 */
    SDL_SCANCODE_PAUSE,          /*  KEY_PAUSE       119 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_SCALE       120 AL Compiz Scale (Expose) */
    SDL_SCANCODE_KP_COMMA,       /*  KEY_KPCOMMA     121 */
    SDL_SCANCODE_LANG1,          /*  KEY_HANGEUL,KEY_HANGUEL 122 */
    SDL_SCANCODE_LANG2,          /*  KEY_HANJA       123 */
    SDL_SCANCODE_INTERNATIONAL3, /*  KEY_YEN         124 */
    SDL_SCANCODE_LGUI,           /*  KEY_LEFTMETA        125 */
    SDL_SCANCODE_RGUI,           /*  KEY_RIGHTMETA       126 */
    SDL_SCANCODE_APPLICATION,    /*  KEY_COMPOSE     127 */
    SDL_SCANCODE_STOP,           /*  KEY_STOP        128 AC Stop */
    SDL_SCANCODE_AGAIN,          /*  KEY_AGAIN       129 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_PROPS       130 AC Properties */
    SDL_SCANCODE_UNDO,           /*  KEY_UNDO        131 AC Undo */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_FRONT       132 */
    SDL_SCANCODE_COPY,           /*  KEY_COPY        133 AC Copy */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_OPEN        134 AC Open */
    SDL_SCANCODE_PASTE,          /*  KEY_PASTE       135 AC Paste */
    SDL_SCANCODE_FIND,           /*  KEY_FIND        136 AC Search */
    SDL_SCANCODE_CUT,            /*  KEY_CUT         137 AC Cut */
    SDL_SCANCODE_HELP,           /*  KEY_HELP        138 AL Integrated Help Center */
    SDL_SCANCODE_MENU,           /*  KEY_MENU        139 Menu (show menu) */
    SDL_SCANCODE_CALCULATOR,     /*  KEY_CALC        140 AL Calculator */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_SETUP       141 */
    SDL_SCANCODE_SLEEP,          /*  KEY_SLEEP       142 SC System Sleep */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_WAKEUP      143 System Wake Up */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_FILE        144 AL Local Machine Browser */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_SENDFILE        145 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_DELETEFILE      146 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_XFER        147 */
    SDL_SCANCODE_APP1,           /*  KEY_PROG1       148 */
    SDL_SCANCODE_APP1,           /*  KEY_PROG2       149 */
    SDL_SCANCODE_WWW,            /*  KEY_WWW         150 AL Internet Browser */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_MSDOS       151 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_COFFEE,KEY_SCREENLOCK      152 AL Terminal
                                Lock/Screensaver */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_DIRECTION       153 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_CYCLEWINDOWS    154 */
    SDL_SCANCODE_MAIL,           /*  KEY_MAIL        155 */
    SDL_SCANCODE_AC_BOOKMARKS,   /*  KEY_BOOKMARKS       156 AC Bookmarks */
    SDL_SCANCODE_COMPUTER,       /*  KEY_COMPUTER        157 */
    SDL_SCANCODE_AC_BACK,        /*  KEY_BACK        158 AC Back */
    SDL_SCANCODE_AC_FORWARD,     /*  KEY_FORWARD     159 AC Forward */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_CLOSECD     160 */
    SDL_SCANCODE_EJECT,          /*  KEY_EJECTCD     161 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_EJECTCLOSECD    162 */
    SDL_SCANCODE_AUDIONEXT,      /*  KEY_NEXTSONG        163 */
    SDL_SCANCODE_AUDIOPLAY,      /*  KEY_PLAYPAUSE       164 */
    SDL_SCANCODE_AUDIOPREV,      /*  KEY_PREVIOUSSONG    165 */
    SDL_SCANCODE_AUDIOSTOP,      /*  KEY_STOPCD      166 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_RECORD      167 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_REWIND      168 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_PHONE       169 Media Select Telephone */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_ISO         170 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_CONFIG      171 AL Consumer Control
                                  Configuration */
    SDL_SCANCODE_AC_HOME,        /*  KEY_HOMEPAGE        172 AC Home */
    SDL_SCANCODE_AC_REFRESH,     /*  KEY_REFRESH     173 AC Refresh */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_EXIT        174 AC Exit */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_MOVE        175 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_EDIT        176 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_SCROLLUP        177 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_SCROLLDOWN      178 */
    SDL_SCANCODE_KP_LEFTPAREN,   /*  KEY_KPLEFTPAREN     179 */
    SDL_SCANCODE_KP_RIGHTPAREN,  /*  KEY_KPRIGHTPAREN    180 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_NEW         181 AC New */
    SDL_SCANCODE_AGAIN,          /*  KEY_REDO        182 AC Redo/Repeat */
    SDL_SCANCODE_F13,            /*  KEY_F13         183 */
    SDL_SCANCODE_F14,            /*  KEY_F14         184 */
    SDL_SCANCODE_F15,            /*  KEY_F15         185 */
    SDL_SCANCODE_F16,            /*  KEY_F16         186 */
    SDL_SCANCODE_F17,            /*  KEY_F17         187 */
    SDL_SCANCODE_F18,            /*  KEY_F18         188 */
    SDL_SCANCODE_F19,            /*  KEY_F19         189 */
    SDL_SCANCODE_F20,            /*  KEY_F20         190 */
    SDL_SCANCODE_F21,            /*  KEY_F21         191 */
    SDL_SCANCODE_F22,            /*  KEY_F22         192 */
    SDL_SCANCODE_F23,            /*  KEY_F23         193 */
    SDL_SCANCODE_F24,            /*  KEY_F24         194 */
    SDL_SCANCODE_UNKNOWN,        /*  195 */
    SDL_SCANCODE_UNKNOWN,        /*  196 */
    SDL_SCANCODE_UNKNOWN,        /*  197 */
    SDL_SCANCODE_UNKNOWN,        /*  198 */
    SDL_SCANCODE_UNKNOWN,        /*  199 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_PLAYCD      200 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_PAUSECD     201 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_PROG3       202 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_PROG4       203 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_DASHBOARD       204 AL Dashboard */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_SUSPEND     205 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_CLOSE       206 AC Close */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_PLAY        207 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_FASTFORWARD     208 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_BASSBOOST       209 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_PRINT       210 AC Print */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_HP          211 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_CAMERA      212 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_SOUND       213 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_QUESTION        214 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_EMAIL       215 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_CHAT        216 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_SEARCH      217 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_CONNECT     218 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_FINANCE     219 AL Checkbook/Finance */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_SPORT       220 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_SHOP        221 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_ALTERASE        222 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_CANCEL      223 AC Cancel */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_BRIGHTNESSDOWN  224 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_BRIGHTNESSUP    225 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_MEDIA       226 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_SWITCHVIDEOMODE 227 Cycle between available
                             video outputs (Monitor/LCD/TV-out/etc) */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_KBDILLUMTOGGLE  228 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_KBDILLUMDOWN    229 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_KBDILLUMUP      230 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_SEND        231 AC Send */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_REPLY       232 AC Reply */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_FORWARDMAIL     233 AC Forward Msg */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_SAVE        234 AC Save */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_DOCUMENTS       235 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_BATTERY     236  */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_BLUETOOTH       237 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_WLAN        238 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_UWB         239 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_UNKNOWN     240 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_VIDEO_NEXT      241 drive next video source */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_VIDEO_PREV      242 drive previous video
                             source */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_BRIGHTNESS_CYCLE    243 brightness up, after
                             max is min */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_BRIGHTNESS_ZERO 244 brightness off, use
                             ambient */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_DISPLAY_OFF     245 display device to off
                             state */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_WIMAX       246 */
    SDL_SCANCODE_UNKNOWN,        /*  KEY_RFKILL      247 Key that controls all radios
                             */
    SDL_SCANCODE_UNKNOWN         /*  KEY_MICMUTE     248 Mute / unmute the microphone */
    }};


  for (std::uint16_t n = 0; n < code_map.size(); n++) {
    if (code_map[n] == scan_code) return n;
  }
  return 0;
}


int open_control()
{
    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if(fd <= 0)
    {
        fprintf(stderr, "Unable to init a unix socket\n");
        return -1;
    }
    control_fd = fd;
    return 0;
}

void send_control_ev(struct event_header_t *t)
{
  #define INPUT_SERVER_PATH "/tmp/wpekit_input"
   struct sockaddr_un client_addr;
   memset(&client_addr, 0, sizeof(client_addr));
   client_addr.sun_family = AF_UNIX;
   strncpy(client_addr.sun_path, INPUT_SERVER_PATH, 64);
   if (sendto(control_fd, t, sizeof(struct event_header_t), 0, (struct sockaddr *)&client_addr,
       sizeof(struct sockaddr_un)) < 0) 
   {
      printf("sending datagram message\n");
   }

}

int open_edpc()
{
  epdc_fd = socket(AF_INET, SOCK_STREAM, 0);
  if(epdc_fd < 0)
  {
     return -1;
  }

  const char* server_name = "10.3.3.1";
  const int server_port = 11322;

  struct sockaddr_in server_address;
  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;

  inet_pton(AF_INET, server_name, &server_address.sin_addr);
  server_address.sin_port = htons(server_port);

  if (connect(epdc_fd, (struct sockaddr*)&server_address,
              sizeof(server_address)) < 0) {
    printf("could not connect to server\n");
    close(epdc_fd);
    epdc_fd = -1;
    return -1;
  }

  return 0;

}

int open_gbm()
{
    const char* renderCard = "/dev/dri/renderD128";

    gbm_fd = open(renderCard, O_RDWR | O_CLOEXEC);
    if (gbm_fd < 0) {
        fprintf(stderr, "ViewBackend: couldn't connect DRM to card %s\n", renderCard);
        return -1;
    }


    gbm_device = gbm_create_device(gbm_fd);
    if (!gbm_device)
    {
        fprintf(stderr, "ViewBackend: couldn't open gbm\n");
        close(gbm_fd);
        return -1;
    }
    return 0;
}

int _recvfd(int s)
{
    int n;
    int fd;
    char buf[1];
    struct iovec iov;
    struct msghdr msg;
    struct cmsghdr *cmsg;
    char cms[CMSG_SPACE(sizeof(int))];

    iov.iov_base = buf;
    iov.iov_len = 1;

    memset(&msg, 0, sizeof msg);
    msg.msg_name = 0;
    msg.msg_namelen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    msg.msg_control = (caddr_t)cms;
    msg.msg_controllen = sizeof cms;

    if((n=recvmsg(s, &msg, 0)) < 0)
        return -1;
    if(n == 0){
        fprintf(stderr, "unexpected EOF");
        return -1;
    }
    cmsg = CMSG_FIRSTHDR(&msg);
    memmove(&fd, CMSG_DATA(cmsg), sizeof(int));
    return fd;
}

static void send_epd_update()
{
  cv::Mat greyMat;
  cv::Mat colorMat(XWIDTH_ORI, XHEIGHT_ORI, CV_8UC4, fb_mem);
  cv::cvtColor(colorMat, greyMat, cv::COLOR_BGRA2GRAY);
  struct ipc_header_t header; 
  header.left = 0;
  header.top = 0;
  header.height = XHEIGHT_ORI;
  header.width = XWIDTH_ORI;
  header.wait_for_complete = 0;
  header.wave_mode = WAVEFORM_MODE_A2;
  header.datalength = XWIDTH_ORI * XHEIGHT_ORI;
  header.magic = MAGIC_NUM;
  // static int ti = 0;
  //  FILE *fp;
  //     char name[256];
  //     snprintf(name, 255, "raw%d.dat", ti);
  //     fp = fopen(name , "wb" );
      
  //     if(fwrite(addr , 1 ,  EWIDTH * EHEIGHT * 4 , fp ) != EWIDTH * EHEIGHT * 4)
  //     {
  //       abort();
  //     }
      
  //     fclose(fp);
  if(write(epdc_fd, &header, sizeof(struct ipc_header_t)) !=   sizeof(struct ipc_header_t))
  {
    printf("Write header failed\n");
    abort();
  }
  
  if(write(epdc_fd, greyMat.data, header.datalength) != header.datalength)
  {
     printf("Write data failed\n");
    abort();
  }

}

static void handle_client_fb_request(int clientfd)
{
    int prime_fd = -1;
    char reply = 0;
    prime_fd = _recvfd(clientfd);
    if (prime_fd <= 0)
    {
        printf("Failed to receive\n");
        close(clientfd);
        return;
    }
    struct gbm_import_fd_data data;
    memset(&data, 0, sizeof(data));
    data.width = XWIDTH_ORI;
    data.height = XHEIGHT_ORI;
    data.format = GBM_FORMAT_ABGR8888;
    data.fd = prime_fd;

    struct gbm_bo* our_bo = gbm_bo_import(gbm_device, GBM_BO_IMPORT_FD, &data, GBM_BO_USE_SCANOUT);
    if(our_bo == NULL)
    {
      fprintf(stderr, "ViewBackend: failed to import bo\n");
      abort();
    }

    uint32_t stride = 0;
    void *map_data = 0;
    void *addr=0;
    addr = gbm_bo_map(our_bo, 0, 0, gbm_bo_get_width(our_bo), gbm_bo_get_height(our_bo), GBM_BO_TRANSFER_READ, &stride, &map_data );
    if(addr == 0)
    {
        fprintf(stderr, "ViewBackend: failed to lock bo\n");
        abort();
    }
    //printf("Stride %d %d %d\n", gbm_bo_get_width(our_bo), gbm_bo_get_height(our_bo), stride);
    memcpy(fb_mem, addr, XWIDTH_ORI * XHEIGHT_ORI * 4);
    gbm_bo_unmap(our_bo, map_data);
    gbm_bo_destroy(our_bo);
    close(prime_fd);
    send(clientfd,  &reply, sizeof(reply), 0);
    close(clientfd);
    SDL_Event sdlevent;
    sdlevent.type = G_TYPE;
    SDL_PushEvent(&sdlevent);
    //send_epd_update();
}

static void *fetch_image_thread_fn(void *x_void_ptr)
{

  while(running)
  {
      int client_addr_len = 0;
      struct sockaddr_un client_addr;
      memset(&client_addr, 0 ,sizeof(struct sockaddr_un));
      int clientfd = accept(graphic_server_fd,  (struct sockaddr*) (&client_addr), (socklen_t*)&client_addr_len);
      if(clientfd <= 0)
      {
          fprintf(stderr, "socket listen error %d\n", errno); 
          running = 0;
      }
      else
      {
          handle_client_fb_request(clientfd);
      }
  }
  
  return NULL;
}

int _create_unix_server(const char *path)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(fd <= 0)
    {
        fprintf(stderr, "Unable to init a unix socket\n");
        return -1;
    }
    

    struct sockaddr_un client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sun_family = AF_UNIX;
    strncpy(client_addr.sun_path, path, 64);
    unlink(client_addr.sun_path);

    if(bind(fd,  (struct sockaddr*) (&client_addr), (socklen_t) sizeof(client_addr)) == -1)
    {
        printf("Unable to send bind clientaddr \n");
        close(fd);
        return -1;
    }

    if (listen(fd, 16) < 0) {
        printf("Unable to listen socket %s\n", path);
        return -1;
    }

    const char mode[] = "0777";
  
    if (chmod (client_addr.sun_path, strtol(mode, 0, 8)) < 0)
    {
     printf("Unable to change permission\n");
     return -1;
    }

    return fd;

}



int main() {

  fb_mem = (char*)malloc(XWIDTH_ORI * XHEIGHT_ORI * 4);
  if(fb_mem == NULL)
  {
    std::cout << "fb mem Failed" <<std::endl;
    return -1;
  }

  fb_mem_scale = (char*)malloc(XWIDTH_SCALE * XHEIGHT_SCALE * 4);
  if(fb_mem_scale == NULL)
  {
    std::cout << "fb mem Failed" <<std::endl;
    return -1;
  }

  if(open_gbm() != 0)
  {
    std::cout << "Open GBM Failed" <<std::endl;
    return -1;
  }

  if(open_control() != 0)
  {
    std::cout << "Open Control Failed" <<std::endl;
    return -1;
  }

  // if(open_edpc() != 0)
  // {
  //   std::cout << "Open EPDC Failed" <<std::endl;
  //   return -1;
  // }


  graphic_server_fd = _create_unix_server(OPENGL_SOCKET);
  if(graphic_server_fd <= 0)
  {
     std::cout << "Graphic Socket Error" <<std::endl;
    return -1;
  }

  if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
    std::cout << "SDL_Init Error: " << SDL_GetError() << std::endl;
    return 1;
  }

  SDL_Window *win = SDL_CreateWindow("Hello World!", 0, 0, XWIDTH_SCALE, XHEIGHT_SCALE, SDL_WINDOW_SHOWN);
  if (win == NULL) {
    std::cout << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
    return 1;
  }

  SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (ren == NULL) {
    std::cout << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
    return 1;
  }



  pthread_create(&fetch_image_thread, NULL, fetch_image_thread_fn, 0);
  SDL_Event e;
  while(running)
  {
      while (SDL_PollEvent(&e))
      {
          if (e.type == SDL_QUIT){
            running = 0;
          }
          else if(e.type == G_TYPE)
          {

              SDL_Surface *ori_bmp = SDL_CreateRGBSurfaceFrom(fb_mem, XWIDTH_ORI, XHEIGHT_ORI, 32, XWIDTH_ORI * 4, rmask, gmask, bmask, amask);
              if (ori_bmp == NULL) {
                std::cout << "SDL_LoadBMP Error: " << SDL_GetError() << std::endl;
                return 1;
              }


              SDL_Surface *scale_bmp = SDL_CreateRGBSurfaceFrom(fb_mem_scale, XWIDTH_SCALE, XHEIGHT_SCALE, 32, XWIDTH_SCALE * 4, rmask, gmask, bmask, amask);
              if (ori_bmp == NULL) {
                std::cout << "SDL_LoadBMP Error: " << SDL_GetError() << std::endl;
                return 1;
              }

              SDL_BlitScaled(ori_bmp, NULL, scale_bmp, NULL);
              SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, scale_bmp);
              SDL_FreeSurface(scale_bmp);
              SDL_FreeSurface(ori_bmp);


              if (tex == NULL) {
                std::cout << "SDL_CreateTextureFromSurface Error: " << SDL_GetError() << std::endl;
                return 1;
              }
              SDL_RenderClear(ren);
              SDL_RenderCopy(ren, tex, NULL, NULL);
              SDL_RenderPresent(ren);
              SDL_DestroyTexture(tex);
          }
          else if(e.type == SDL_MOUSEBUTTONUP)
          {
              int x, y;
              SDL_GetMouseState(&x, &y);
              struct event_header_t ev;
              ev.type = EVENT_TOUCH_UP;
              ev.x = x *SCALE_FACTOR;
              ev.y = y * SCALE_FACTOR;
              send_control_ev(&ev);

          }
          else if(e.type == SDL_MOUSEBUTTONDOWN)
          {
              int x, y;
              SDL_GetMouseState(&x, &y);
              struct event_header_t ev;
              ev.type = EVENT_TOUCH_DOWN;
              ev.x = x *SCALE_FACTOR;
              ev.y = y * SCALE_FACTOR;
              send_control_ev(&ev);
          }
          else if(e.type == SDL_KEYDOWN)
          {
              struct event_header_t ev;
              ev.type = EVENT_KEYBOARD_DOWN;
              ev.keycode = convert_keycode(e.key.keysym.scancode);
              send_control_ev(&ev);
          }
          else if(e.type == SDL_KEYUP)
          {
              struct event_header_t ev;
              ev.type = EVENT_KEYBOARD_UP;
              ev.keycode = convert_keycode(e.key.keysym.scancode);
              send_control_ev(&ev);
          }

      }
      SDL_Delay(10);
  }

 

  return 0;
}