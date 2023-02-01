/*** includes ***/
#include <asm-generic/ioctls.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <string.h>

/*** defines ***/
#define KILO_VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f) // define the ctrl key

/*** data ***/
struct editorConfig // get the size of the terminal to draw row num
{
    int cx, cy; // for cursor move
    int screenrows;
    int screencols;
    struct termios orig_termios;
};

struct editorConfig E;
struct termios orig_termios;

/*** terminal ***/
void die(const char *s) // error check func
{
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

void disableRawMode() // disable raw mode on exit
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
        die("tcsetattr"); // error check
    };
}

void enableRawMode() // turn off echoing terminal input
{
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) {
        die("tcgetattr"); // error check
    };
    atexit(disableRawMode); // when terminal exists

    struct termios raw = E.orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON); // disable signals
    raw.c_oflag &= ~(OPOST); // new line
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG); // read bytes instead of line, disable signals
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1; // time to wait for read

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) { // set terminal attibutes
        die("tcsetattr");
    };
}

char editorReadKey() // wait for key press and return it
{
   int nread;
   char c;
   while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
       if (nread == -1 && errno != EAGAIN) {
           die ("read");
       }
   }
   return c;
}

int getCursorPosition(int *rows, int *cols)
{
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4 ) {
        return -1;
    }

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }

    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

    return 0;
}

int getWindowSize(int *rows, int *cols) // get the terminal size
{
    struct winsize ws;

    // move cursor to the bottom right, C:right, B:down, 999 ensures its bottom right
    if (1 || ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if  (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) {
            return -1;
        }
        return getCursorPosition(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*** append buffer ***/

// create a dynamic string type
struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len)
{
    // allocate memory the size of the current string + the appending string
    char *new = realloc(ab->b, ab->len + len);

    if (new == NULL) return;
    // copy the string s after the end of the current data in buffer and update pointer
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab)
{
    // destruct memory used by abuf
    free(ab->b);
}

/*** input ***/

// cursor
void editorMoveCursor(char key)
{
    switch (key) {
        case 'a':
            E.cx--;
        case 'd':
            E.cx++;
        case 'w':
            E.cy--;
        case 's':
            E.cy++;
            break;
    }
}

void editorProcessKeypress() // func for mapping keypress to editor operation
{
    char c = editorReadKey();

    switch (c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;

        case 'w':
        case 's':
        case 'a':
        case 'd':
            editorMoveCursor(c);
            break;
    }
}

/*** output ***/
void editorDrawRows(struct abuf *ab) // draw tildes on screen with rows
{
    int y;
    for (y = 0; y < E.screenrows; y++) {
        if (y == E.screenrows / 3) {
            // Welcome message
            char welcome[80];
            int welcomelen = snprintf(welcome, sizeof(welcome),
                "Kelo editor -- version %s", KILO_VERSION);
            if (welcomelen > E.screencols) welcomelen = E.screencols;
            // centering, devide width by 2
            int padding = (E.screencols - welcomelen) / 2;
            if (padding) {
                abAppend(ab, "~", 1);
                padding--;
            }
            while (padding--) abAppend(ab, " ", 1);
            abAppend(ab, welcome, welcomelen);
        } else {
            abAppend(ab,"~", 1);
        }

        abAppend(ab, "\x1b[K", 3); // clear lines 1 at a time
        // write tildes on last line by making it an exception
        if (y < E.screenrows - 1) {
            abAppend(ab, "\r\n", 2);
        }
    }
}

void editorRefreshScreen()
{
    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6); // hide cursor when repainting
    // abAppend(&ab, "\x1b[2J", 4);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25l", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/*** init ***/

void initEditor() // initialize all the fields in the E struct
{
    E.cx = 0;
    E.cy = 0;

    if (getWindowSize(&E.screenrows, &E.screencols) == -1) {
        die("getWindowSize");
    }
}

int main()
{
    enableRawMode(); // fully gets us into raw mode
    initEditor();

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}
