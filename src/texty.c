/*** includes ***/
#include <unistd.h> // read, write
#include <termios.h> // .. 
#include <stdlib.h> // atexit()
#include <ctype.h>
#include <stdio.h>
#include <errno.h>

/*** defines ***/

// bitwise-ANDs and char with 0001-1111, setting the upper 3 bits of the char to 0 
// this mirros what Ctrl does in the terminal, sets 5 and 6 bit to 0
#define CTRL_KEY(ch) ((ch) & 0x1f)



/*** data ***/
// original terminal attributes
struct termios orig_termios;


/*** terminal ***/

void die(const char* s) {
    perror(s);
    exit(1);
}


void disableRawMode() {
    int res = tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    if (res == -1) {
        die("tcsetattr");
    }
}

void enableRawMode() {

    int resGet = tcgetattr(STDIN_FILENO, &orig_termios);
    if (resGet == -1)
        die ("tcgetattr");
    
    // call function at exit of the program
    atexit(disableRawMode);

    struct termios raw = orig_termios;
    
    // turn off terminal features by bitwise-AND. Local flags:
    //  ECHO: each key printed to terminal
    //  ICANON: canonical mode, where you have to wait the user to press Enter
    //  ISIG: Ctrl-C (sends SIGINT, terminate), Ctrl-Z (sends SIGTSTP, suspend to background)
    //  IEXTEN: on some systems Ctrl-V, Ctrl-O (macOS)
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    
    // turn off .. input flags:
    //  IXON: Ctrl-S, Ctrl-Q (Ctrl-S make you stop send output, Ctrl-Q tells to resume sending input ('XON' because these are control characters)
    //  ICRNL: translates \r (Ctrl-M) to \n (CR: carriage return, NL: newline). After turning off, Ctrl-M is 13 and Enter is also 13
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);

    // turn off. output flags:
    //  OPOST: translates \n (13) into \r\n (10)
    raw.c_oflag &= ~(OPOST);
    
    // sets character size to 8bits per byte (probably already set by default)
    raw.c_cflag |= (CS8); 
    
    raw.c_cc[VMIN]  = 0; // min number of bytes before read() returns
    raw.c_cc[VTIME] = 1; // max amount of time before read() retunrs: 1 == 1/10 s == 100ms

    // changes after flush
    int resSet = tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    if (resSet == -1)
        die("tcsetattr");
}


// reads a char from STDIN and returns it
char editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN)
            die("read");
    }

    return c;
}



/*** input ***/

// handles keypress
void editorProcessKeypress(){

    char c = editorReadKey();
    switch(c){
        case CTRL_KEY('q'):
            exit(0);
            break;
        default:
            // if is control character (non-printable) ascii 0-31, 127
            // due to changes bitwise OPOST, we have to add \r to go to the left side of the screen
            if (iscntrl(c)){
                printf("%d\r\n", c);
            }
            else { // ascii 32-126
                printf("%d ('%c')\r\n", c, c);
            }
            break;
    }
}

/*** output ***/
void editorRefreshScreen(){

    // \x1b : escape(char), 27(int)
    write(STDOUT_FILENO,"\x1b[2J", 4);
}

/*** init ***/
int main() {

    enableRawMode();
    
    while (1) {
        editorProcessKeypress();
    }
    
    return 0;
}
