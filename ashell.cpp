#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <unistd.h>
#include <queue>
using namespace std;

// store file path in stack of strings leading with a '/'


//char array for current working directory absolute path
char currentDirectory[100];
//queue of char arrays that will correspond to history of commands
queue<char*> history;


//gets current directory absolute path name
void getCurrentDirectory(){
	getcwd(currentDirectory, 100);
}

void getCommand(){

}



// Location currLoc();
// void cd();
// void ls();
// void pwd();
// void history();
// void exit();


int main (int argc, char** argv) {

    return 0;
}



// get current working directory: char *getcwd(char *buf, size_t size);



//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
// -----DATA TYPES-----
// int historyLocation      - current index in history (valid between -1 and 9)

// char[] currentDirectory  - the string of the current path
// char[] startingDirectory - the string of the initial path used to go back to when 'exit'
// deque<string> history    - history of commands
// stack<string> filepath   - currentDirectory delimited by '/' for easy filepath traversal




// -----FUNCTIONS------
// execute: 'execl()'

// pipes: 'pipe()'

// IO redirection:

// cd: check if path is valid, if it is then push each '/name' to stack and pop off stack if '/..'

// ls: check if path is valid, if it is then print contents of directory. if no path specified then
// print contents of currentDirectory. referenced: http://stackoverflow.com/questions/10323060/printing-file-permissions-like-ls-l-using-stat2-in-c

// pwd: print out currentDirectory path

// history: each command (valid or invalid) will be pushed to front of a deque. 
// commands past 9 will be popped off the back. global var stored to keep track
// of where you are in the history. If at -1, write blank line. If at -2 stay
// at blank line and output alert sound ('\a')

// exit: return 0 in main()

