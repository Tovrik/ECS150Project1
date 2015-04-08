#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <ctype.h>


void ResetCanonicalMode(int fd, struct termios *savedattributes){
    tcsetattr(fd, TCSANOW, savedattributes);
}

void SetNonCanonicalMode(int fd, struct termios *savedattributes){
    struct termios TermAttributes;
    char *name;

    // Make sure stdin is a terminal.
    if(!isatty(fd)){
        fprintf (stderr, "Not a terminal.\n");
        exit(0);
    }

    // Save the terminal attributes so we can restore them later.
    tcgetattr(fd, savedattributes);

    // Set the funny terminal modes.
    tcgetattr (fd, &TermAttributes);
    TermAttributes.c_lflag &= ~(ICANON | ECHO); // Clear ICANON and ECHO.
    TermAttributes.c_cc[VMIN] = 1;
    TermAttributes.c_cc[VTIME] = 0;
    tcsetattr(fd, TCSAFLUSH, &TermAttributes);
}



// int main(int argc, char *argv[]){
//     struct termios SavedTermAttributes;
//     char RXChar;

//     SetNonCanonicalMode(STDIN_FILENO, &SavedTermAttributes);

//     while(1){
//         read(STDIN_FILENO, &RXChar, 1);
//         if(0x04 == RXChar){ // C-d
//             break;
//         }
//         else{
//             if(isprint(RXChar)){
//                 printf("RX: '%c' 0x%02X\n",RXChar, RXChar);
//             }
//             else{
//                 printf("RX: ' ' 0x%02X\n",RXChar);
//             }
//         }
//     }

//     ResetCanonicalMode(STDIN_FILENO, &SavedTermAttributes);
//     return 0;
// }
