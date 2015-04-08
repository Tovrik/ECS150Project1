#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>      // std::stringstream, std::stringbuf
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "noncanmode.c"
#include <deque>
#include <stack>
#include <termios.h>

using namespace std;

int historyLocation = 0;

char deleted1 = 0x08;
char deleted2 = 0x7F;
char escape   = 0x1B;
char lbracket = 0x5B;
char upArrow  = 0x41;
char downArrow= 0x42;

bool exitStatus = 1;
char command[100];

//char buffer
char currChar[1];
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
	string temp = "";
	for (int i = 0; i < 100; i++) {
		// first char is alwats '/'
		if (i == 0) {
			delimitedCurrDirectory.push("/");
		}
		if (currentDirectory[i] == '/') {
			delimitedCurrDirectory.push(temp);
			temp = "";
		}
		else if (currentDirectory[i] == '\0') {
			delimitedCurrDirectory.push(temp);
			break;
		}
		else if (currentDirectory[i] == ' ') {
			temp = temp + "\\ ";
		}
		else {
			temp = temp + currentDirectory[i];
		}
	}
}

void writePrompt() {
	//add /home/stpeters... stack <= 2

	getCurrentDirectory();
	delimit();
	string temp = delimitedCurrDirectory.top();
	int size = temp.length();
	if (delimitedCurrDirectory.size() == 1) {
		size += 2;
		temp = temp + "> ";
	}
	else if (delimitedCurrDirectory.size() == 2) {
		size += 8;
		temp = "/home/" + temp + "> ";
	}
	else {
		size += 7;
		temp = "/.../" + temp + "> ";
	}
	char prompt[size];
	strcpy(prompt, temp.c_str());
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
	listedHistory.push_back(currCommand);
	if(listedHistory.size() > 10){
		listedHistory.pop_front();
	}
}

void history() {
	deque<string>::iterator it = listedHistory.begin();
	int count = 0;
	while (it != listedHistory.end()) {
		string tmpString = *it;
		//stringstream to convert into to string
		stringstream ss;
		ss << count;
		string number = ss.str();
		//concatenate number and string
		tmpString = number + " " + tmpString;
		int size = tmpString.length();
		char tmpChar[size + 1];
		strcpy(tmpChar, tmpString.c_str());
		tmpChar[size] = '\0';
		write(1, tmpChar, size + 1);
		*it++;
		count++;
	}
}

void upInHistory() {
	if(listedHistory.size() > 0 && historyLocation >= 0) {
		string tmpCommand = listedHistory.at(listedHistory.size() - historyLocation - 1);
		int len = tmpCommand.length();
		//this contains a return char that we need to delete
		write(1, tmpCommand.c_str(), len);
		historyLocation++;

	}
	else {
		write(1, "\a", 2);
	}
}

void downInHistory() {
	if(listedHistory.size() > 0 && historyLocation > 0) {
		string tmpCommand = listedHistory.at(listedHistory.size() - historyLocation - 1);
		int len = tmpCommand.length();
		write(1, tmpCommand.c_str(), len);
		historyLocation--;
	}
	else {
		write(1, "\a", 2);
	}
}

void clearLine(int commandLength) {
	for(int i = commandLength; i > 0; i--){
		write(1, "\b \b", 3);
	}
}

void checkArrow() {
	read(0, currChar, 1);
	if(currChar[0] == lbracket) {
		read(0, currChar, 1);
		if(currChar[0] == upArrow) {
			upInHistory();
		}
		else if (currChar[0] == downArrow) {
			downInHistory();
		}
	}
}

void getCommand(){
	int myindex = 0;
	//clear command before next iteration
	memset(command, '\0', 100);
	do {
		read(0, currChar, 1);

		if((currChar[0] == deleted1 || currChar[0] == deleted2) && myindex > 0) {
			write(1, "\b \b", 3);
			myindex--;
		}
		else if(isprint(currChar[0]) || currChar[0] == '\n' ) {
			command[myindex] = currChar[0];
			myindex++;
			write(1, currChar, 1);
		}
		else if(currChar[0] == escape) {
			clearLine(myindex);
			checkArrow();
			myindex = 0;
		}
	} while (currChar[0] != '\n');
	string temp(command);
	addToHistory(command);
	// exits the program
	if(temp == "exit\n") exitStatus = 0;
	else if(temp == "pwd\n") pwd();
	else if(temp == "ls\n") ls();
	else if(temp == "cd\n") cd();
	else if(temp == "history\n") history();
}

// void printPermissions(){
//     struct stat fileStat;


//  printf( (S_ISDIR(fileStat.st_mode)) ? "d" : "-");
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
	struct termios SavedTermAttributes;
	SetNonCanonicalMode(0, &SavedTermAttributes);

	getCurrentDirectory();

	while(exitStatus){
		writePrompt();
		getCommand();
	}
	ResetCanonicalMode(0, &SavedTermAttributes);
	return 0;
}




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

// e(1, Buffer, size);
// "\b \b"
// 0x08 and 0x7F treat them the same

