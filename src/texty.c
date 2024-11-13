/*** includes ***/
#include <unistd.h> // read, write
#include <termios.h> // .. 
#include <stdlib.h> // atexit()
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h> // iotctl(), winsize, TIOCGWINSZ

/*** defines ***/

// bitwise-ANDs and char with 0001-1111, setting the upper 3 bits of the char to 0 
// this mirros what Ctrl does in the terminal, sets 5 and 6 bit to 0
#define CTRL_KEY(ch) ((ch) & 0x1f)


/*** data ***/
typedef struct {
    int screenRows;
    int screenCols;
    struct termios orig_termios; // original terminal attributes

} EditorConfig;


EditorConfig E;


/*** terminal ***/

void die(const char* s) {
    write(STDOUT_FILENO,"\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3); //   \x1b[1;1H
    
    perror(s);
    exit(1);
}


void disableRawMode() {
    int res = tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios);
    if (res == -1) {
        die("tcsetattr");
    }
}

void enableRawMode() {

    int resGet = tcgetattr(STDIN_FILENO, &E.orig_termios);
    if (resGet == -1)
        die ("tcgetattr");
    
    // call function at exit of the program
    atexit(disableRawMode);

    struct termios raw = E.orig_termios;
    
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


int getWindowSize(int* rows, int* cols){

    struct winsize ws;
   
    // get windowsize via ioctl (request: TIOCGWINSZ Terminal I/O Control Get WINdow Size) and save it in ws
    if (1 || ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 ||
            ws.ws_col == 0){
        
        // not in all systems ioctl will work, so here is a fallback method:
        //  placing cursor at bottom right of the screen and query its position 


        //write(STDOUT_FILENO, "\x1b[999;999H", 3); : will not wwork because its not clear if 999;999 will move us off-screen or it will stop
       
        // C: CursorForward, move cursor to the right, i use a big value so it will reach the edge 
        // B: CursorDown, move it down
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
            
        editorReadKey();

        return -1;


    }

    *rows = ws.ws_row;
    *cols = ws.ws_col; 
    return 0;    
}

/*** input ***/

// handles keypress
void editorProcessKeypress(){

    char c = editorReadKey();
    switch(c){
        case CTRL_KEY('q'):
            write(STDOUT_FILENO,"\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3); //   \x1b[1;1H
            
            exit(0);
            break;
    }
}

/*** output ***/
void editorDrawRows(){
    for (int y = 0; y < E.screenRows; y++){
        write(STDOUT_FILENO, "~\r\n", 3);
    }
}


void editorRefreshScreen(){
    
    // VT100

    // 4 bytes:
    //
    // \x1b : ESC(char), 27(int), 
    // [    : escape sequence always starts with [
    // J    : Erase In Display
    // 2    : option after Erase In Display: entire screen
    // https://vt100.net/docs/vt100-ug/chapter3.html#EDhttps://vt100.net/docs/vt100-ug/chapter3.html#ED
    write(STDOUT_FILENO,"\x1b[2J", 4);
    
    //  H   : Cursor Position, move position to row and column parameters(by default are 1;1) 
    write(STDOUT_FILENO, "\x1b[H", 3); // \x1b[1;1H

    editorDrawRows();
    write(STDOUT_FILENO, "\x1b[H", 3);// after drawing rows, i want my cursor in position 1;1
}

/*** init ***/

// initialize editorConfig (E) fields
void initEditor (){
    if (getWindowSize(&E.screenRows, &E.screenCols) == -1)
        die ("getWindowSize");
}

int main() { 
    enableRawMode();
    initEditor();

    while(1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    
    return 0;
}

