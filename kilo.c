/*** includes ***/
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <termios.h>

/*** data ***/
struct termios orig_termios;

/*** terminal ***/
void die(const char *s) // error check func
{
    perror(s);
    exit(1);
}

void disableRawMode() // disable raw mode on exit
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) {
        die("tcsetattr"); // error check
    };
}

void enableRawMode() // turn off echoing terminal input
{
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        die("tcgetattr"); // error check
    };
    atexit(disableRawMode); // when terminal exists

    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON); // disable signals
    raw.c_oflag &= ~(OPOST); // new line
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG); // read bytes instead of line, disable signals
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1; // time to wait for read

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) { // set terminal attibutes
        die("tcsetattr");
    };
}

/*** main ***/
int main()
{
    enableRawMode(); // fully gets us into raw mode

    char c;
    // read 1 byte from standard input to c, q to quit
    while (1) {
        char c = '\0'; // c retains the value 0 when no input
        read(STDIN_FILENO, &c, 1); // continue to read stdin
        if (iscntrl(c)) { //  check if 'c' is a control character
            printf("%d\r\n", c); // print out keypresses, move \n down+left
        } else {
            printf("%d ('%c')\r\n", c, c); // print out ascii and actual char
        }
        if (c == 'q') break;
    }
    return 0;
}
