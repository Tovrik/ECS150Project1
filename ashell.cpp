#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <deque>
#include <stack>
using namespace std;

bool exitStatus = 1;
//start directory
char command[100];
//start directory
char startDirectory[100];
//char array for current working directory absolute path
char currentDirectory[100];
//queue of char arrays that will correspond to history of commands
deque<string> listedHistory;
//vector of char arrays
stack<string> delimitedCurrDirectory;


//gets current directory absolute path name
void getCurrentDirectory(){
	memset(currentDirectory, '\0', 100);
	getcwd(currentDirectory, 100);
}

void delimit(){
	string temp;
	for(int i = 0; i < 100; i++) {
		if(currentDirectory[i] == '/' && i != 0 ) {
			delimitedCurrDirectory.push(temp);
			temp = "";
		}
		else if(currentDirectory[i] == '\0') {
			delimitedCurrDirectory.push(temp);
			break;
		}
		else if( currentDirectory[i] == ' ')
			temp = temp + "\\ ";
		else {
			temp = temp + currentDirectory[i];
		}
	}
}

void writePrompt() {
	getCurrentDirectory();
	delimit();
	string temp = delimitedCurrDirectory.top();
	int size = temp.length() + 6;
	char prompt[size + 6];
	prompt[0] = '/'; prompt[1] = '.'; prompt[2] = '.'; prompt[3] = '.'; prompt[4] = '/';
	for(int i = 0; i < temp.length(); i++) {
		prompt[5+i] = temp[i];
	}
	prompt[temp.length() + 5] = '>';
	write(1, prompt, size);
}


void cd() {

}

void ls() {

}


void pwd() {
	getCurrentDirectory();
	write(1, currentDirectory, 100);
	write(1, "\n", 2);
}

void addToHistory(string currCommand) {
	listedHistory.push_front(currCommand);
	if(listedHistory.size() > 10)
		listedHistory.pop_back();

}

void history() {
  deque<string>::iterator it = listedHistory.begin();
  	while (it != listedHistory.end()) {
  		string tmpString = *it;
  		int size = tmpString.length();
  		char tmpChar[size + 1];
  		strcpy(tmpChar, tmpString.c_str());
  		tmpChar[size] = '\0';
  		write(1, tmpChar, size + 1);
  		*it++;
  	}

}


void getCommand(){

	writePrompt();
	read(0, command, 100);
	string temp(command);
	//exits the program
	if(temp == "exit\n") exitStatus = 0;
	else if(temp == "pwd\n") pwd();
	else if(temp == "ls\n") ls();
	else if(temp == "cd\n") cd();
	else if(temp == "history\n") history();
	addToHistory(command);
}

// void printPermissions(){
//     struct stat fileStat;


// 	printf( (S_ISDIR(fileStat.st_mode)) ? "d" : "-");
//     printf( (fileStat.st_mode & S_IRUSR) ? "r" : "-");
//     printf( (fileStat.st_mode & S_IWUSR) ? "w" : "-");
//     printf( (fileStat.st_mode & S_IXUSR) ? "x" : "-");
//     printf( (fileStat.st_mode & S_IRGRP) ? "r" : "-");
//     printf( (fileStat.st_mode & S_IWGRP) ? "w" : "-");
//     printf( (fileStat.st_mode & S_IXGRP) ? "x" : "-");
//     printf( (fileStat.st_mode & S_IROTH) ? "r" : "-");
//     printf( (fileStat.st_mode & S_IWOTH) ? "w" : "-");
//     printf( (fileStat.st_mode & S_IXOTH) ? "x" : "-");
// }


// Location currLoc();

// void exit();


int main (int argc, char** argv) {
	getCurrentDirectory();
	// for(int i = 0; i < 100; i++ )
	// 	cout << currentDirectory[i];
	// cout << endl;

	while(exitStatus){
		getCommand();
	}
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






// NITTAS BULLSHIT:
// read(STDIN_FILENO, Buffer, size); //STDIN_FILENO = 0 generally
// write(1, Buffer, size);
// "\b \b"
// 0x08 and 0x7F treat them the same

