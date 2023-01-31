/*** includes ***/
#include <asm-generic/ioctls.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f) // define the ctrl key

/*** data ***/
struct editorConfig // get the size of the terminal to draw row num
{
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
       if (nread == -1 && errno != EAGAIN) die ("read");
   }
   return c;
}

int getWindowSize(int *rows, int *cols) // get the terminal size
{
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 1 || ws.ws_col == 0) {
        return -1;
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*** input ***/
void editorProcessKeypress() // func for mapping keypress to editor operation
{
    char c = editorReadKey();

    switch (c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
    }
}

/*** output ***/
void editorDrawRows()
{
    int y;
    for (y = 0; y < 24; y++) {
        write(STDOUT_FILENO, "~\r\n", 3);
    }
}


void editorRefreshScreen()
{
    write(STDOUT_FILENO, "\x1b[2J", 4); // \x1b:esc, j:erase display, 2:entire screen
    write(STDOUT_FILENO, "\x1b[H", 3); // H:cursor position

    editorDrawRows();

    write(STDOUT_FILENO, "\x1b[H", 3);
}

/*** main ***/
int main()
{
    enableRawMode(); // fully gets us into raw mode

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}
