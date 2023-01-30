#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <termios.h>

struct termios orig_termios;

void disableRawMode() // disable raw mode on exit
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode() // turn off echoing terminal input
{
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode); // when terminal exists

    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON); // disable signals
    raw.c_oflag &= ~(OPOST); // new line
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG); // read bytes instead of line, disable signals
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1; // time to wait for read
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw); // set terminal attibutes
}

int main()
{
    enableRawMode();

    char c;
    // read 1 byte from standard input to c, q to quit
    while (1) {
        char c = '\0'; // c retains the value 0 when no input
        read(STDIN_FILENO, &c, 1);
        if (iscntrl(c)) { //  check if 'c' is a control character
            printf("%d\r\n", c); // print out keypresses, move \n down+left
        } else {
            printf("%d ('%c')\r\n", c, c); // print out ascii and actual char
        }
        if (c == 'q') break;
    }
    return 0;
}
