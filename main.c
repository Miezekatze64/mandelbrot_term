#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include <string.h>
#include <errno.h>
#include <assert.h>

#if defined(_WIN32) || defined(_WIN64) || defined(__MINGW32__) || defined(__CYGWIN__)
#error This program currently does NOT support windows! \
    if you are thinking otherwise, please delete this line or add `undef(_WIN32)`..
int main() {}
#else
#ifndef __linux__
#warning this program is not tested for your operating systems, some bugs might occur...
#endif //__linux__

#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define die(str) \
    do {                                                                \
        printf("An unexpected error occured at %s:%d: (%s) %s [ERRNO: %d]\n", __FILE__,  __LINE__,  str, strerror(errno), errno); \
        exit(1);                                                        \
    } while (0);                                                        \

typedef struct termios termios_t;

termios_t orig_term;

void restore_term() {
    if (tcsetattr(0, TCSAFLUSH, &orig_term) < 0) {
        die("restore_term / tcsetattr");
    }
}

void raw_mode() {
    if (tcgetattr(1, &orig_term) < 0) die("raw_mode / tcgetattr");
    atexit(restore_term);

    termios_t raw = orig_term;
    raw.c_lflag &= ~(IXON | ICRNL);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    
    if (tcsetattr(0, TCSAFLUSH, &raw) < 0) die("raw_mode / tcsetattr");
}

int termsize(int *cols, int *rows) {
    struct winsize ws;
    if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        return -1;
    } else {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
        return 0;
    }
}

double fpowi(double x, int y) {
    if (y < 0) {
        return 1.0 / fpowi(x, -y);
    }
    
    double res = 1;
    for (int i = 0; i < y; i++) {
        res *= x;
    }
    return res;
}

double fabs(double x) {
    return (x >= 0) ? x : -x;
}

#define START_ITERATIONS 100
#define SCALE_FACTOR 1.1
uint32_t get_color(const int x, const int y, const double x_off_, const double y_off_, const int scale_, const int w, const int h, const bool ship) {
    const double scale = fpowi(SCALE_FACTOR, scale_);
    const double x_off = x_off_*scale;
    const double y_off = y_off_*scale;
    const double pos_x = (((double)(x+x_off*2) / w) * 4 - 2) / scale;
    const double pos_y = (((double)(y+y_off*2) / h) * 2 - 1) / scale;
    
    double cx = pos_x, cy = pos_y;
    double zx = 0, zy = 0;
    for (int it = 0; it < (int)(START_ITERATIONS * (scale_*0.1+1)); it++) {
        double orig_zx = zx;
        zx = zx * zx - zy * zy;
        zy = 2 * zy * orig_zx;

        zx += cx;
        zy += cy;

        if (ship) {
            zx = fabs(zx);
            zy = fabs(zy);
        }

        if (zx * zx + zy * zy > 2 * 2) {
            uint8_t red = it*40 % 256;
            uint8_t green = it*20 % 256;
            uint8_t blue = it*10 % 256;
            
            return (red << 16) | (green << 8) | blue;
        }
    }
    return 0;
}

static char *buf = NULL;
static int lw = 0, lh = 0;
void render(double x_off, double y_off, int scale, bool force, const bool ship, const bool back) {
    int width = 20, height = 20;
    termsize(&width, &height);
    
    if ((lw != width) || (lh != height)) {
        force = true;
        if (((lw < width) || (lh < height)) && buf != NULL) {
            buf = realloc(buf, width*height * sizeof(char) * 21);
        }
    }
    lw = width;
    lh = height;

    if (!force) return;

    if (buf == NULL) buf = malloc(width*height * sizeof(char) * 21);
    buf[0] = '\x1b';
    buf[1] = '[';
    buf[2] = 'H';
    buf[3] = '\x1b';
    buf[4] = '[';
    buf[5] = '0';
    buf[6] = 'm';
    int ind = 6;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            ind++;
            uint32_t color = get_color(x, y, x_off, y_off, scale, width, height, ship);
            uint8_t red   = (color >> 16) & 0xff;
            uint8_t green = (color >> 8) & 0xff;
            uint8_t blue  = (color >> 0) & 0xff;
            
            strcpy(buf+ind, back?"\x1b[48;2;":"\x1b[38;2;");
            buf[ind+7] =  '0' + (red/100)%10;
            buf[ind+8] =  '0' + (red/10)%10;
            buf[ind+9] =  '0' + (red/1)%10;
            buf[ind+10] = ';';
            buf[ind+11] =  '0' + (green/100)%10;
            buf[ind+12] =  '0' + (green/10)%10;
            buf[ind+13] =  '0' + (green/1)%10;
            buf[ind+14] = ';';
            buf[ind+15] =  '0' + (blue/100)%10;
            buf[ind+16] =  '0' + (blue/10)%10;
            buf[ind+17] =  '0' + (blue/1)%10;
            buf[ind+18] = 'm';
            buf[ind+19] = back?' ':'#';

            ind += 19;
            
        }
        if (y < height-1) buf[ind] = '\n';
    }

    write(1, buf, ind++);
}

int main() {
    raw_mode();
    
    bool esc_pre = false, esc = false;
    double x = 0, y = 0;
    int scale = 0;
    bool rerender = true;
    bool ship = false;
    bool back = true;
    for (;;) {
        render(x, y, scale, rerender, ship, back);
        char c = '\0';
        if (read(1, &c, 1) < 0) die("main / read");
        if (c == 'q') {
            printf("\033[H\033[2J");
            exit(0);
            break;
        }

        rerender = true;
        switch (c) {
        case 27:
            esc_pre = true;
            break;
        case '[':
            esc = esc_pre;
            break;
        case 'A':
            if (esc) y -= fpowi(SCALE_FACTOR, -scale);
            esc = false;
            break;
        case 'B':
            if (esc) y += fpowi(SCALE_FACTOR, -scale);
            esc = false;
            break;
        case 'C':
            if (esc) x += fpowi(SCALE_FACTOR, -scale);
            esc = false;
            break;
        case 'D':
            if (esc) x -= fpowi(SCALE_FACTOR, -scale);
            esc = false;
            break;
        case '+':
            scale++;
            break;
        case '-':
            scale--;
            break;
        case ' ':
            ship = !ship;
            break;
        case 'b':
        case '#':
            back = !back;
            break;
        default:
            rerender = false;
            esc = esc_pre = false;
            break;
        }
    }
    return 0;
}

#endif //_WIN32
