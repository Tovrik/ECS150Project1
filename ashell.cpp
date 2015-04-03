#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <unistd.h>
#include <sys/stat.h>
#include <deque>
using namespace std;

//start directory
char startDirectory[100];
//char array for current working directory absolute path
char currentDirectory[100];
//queue of char arrays that will correspond to history of commands
deque<char*> history;
//vector of char arrays
stack<string> delimitedCurrDirectory;

//gets current directory absolute path name
void getCurrentDirectory(){
	getcwd(currentDirectory, 100);
}

void delimit(char *filepath, int size){
	for(int i = 0; i < size; i++) {
		if(filepath[i] == '/')
			push
	}

}

void getCommand(){

	ssize_t read(int fildes, void *buf, size_t nbyte);

}

void printPermissions(){
    struct stat fileStat;

	
	printf( (S_ISDIR(fileStat.st_mode)) ? "d" : "-");
    printf( (fileStat.st_mode & S_IRUSR) ? "r" : "-");
    printf( (fileStat.st_mode & S_IWUSR) ? "w" : "-");
    printf( (fileStat.st_mode & S_IXUSR) ? "x" : "-");
    printf( (fileStat.st_mode & S_IRGRP) ? "r" : "-");
    printf( (fileStat.st_mode & S_IWGRP) ? "w" : "-");
    printf( (fileStat.st_mode & S_IXGRP) ? "x" : "-");
    printf( (fileStat.st_mode & S_IROTH) ? "r" : "-");
    printf( (fileStat.st_mode & S_IWOTH) ? "w" : "-");
    printf( (fileStat.st_mode & S_IXOTH) ? "x" : "-");
}


// Location currLoc();
// void cd();
// void ls();
// void pwd();
// void history();
// void exit();


int main (int argc, char** argv) {
	getCurrentDirectory();
	for(int i = 0; i < 100; i++ )
		cout << currentDirectory[i];

	cout << endl;
    return 0;
}



// get current working directory: char *getcwd(char *buf, size_t size);



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

