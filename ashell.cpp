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

char deleted1 = 0x08;
char deleted2 = 0x7F;
char escape   = 0x1B;
char lbracket = 0x5B;
char upArrow  = 0x41;
char downArrow= 0x42;

bool exitStatus = 1;
int historyLocation = 0;
int myindex = 0;

// command buffer
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
	int count = 0;
	while (count < listedHistory.size()) {
		//stringstream to convert into to string
		stringstream ss;
		ss << count;
		string number = ss.str();
		//concatenate number and string
		string out = number + " " + listedHistory.at(count);
		int size = out.length();
		char outArray[size + 1];
		strcpy(outArray, out.c_str());
		outArray[size] = '\0';
		write(1, outArray, size + 1);
		count++;
	}
}

void clearLine(int commandLength) {
	for(int i = commandLength; i > 0; i--){
		write(1, "\b \b", 3);
	}
}

void checkCommand () {
	string temp(command);
	addToHistory(command);
	// exits the program
	if(temp == "exit\n") exitStatus = 0;
	else if(temp == "pwd\n") pwd();
	else if(temp == "ls\n") ls();
	else if(temp == "cd\n") cd();
	else if(temp == "history\n") history();
}

void upInHistory() {
	if(historyLocation >= 0 && historyLocation < listedHistory.size()) {
		clearLine(myindex);
		// -1 because deque is 0 indexed
		string tmpCommand = listedHistory.at(listedHistory.size() - 1 - historyLocation);
		int len = tmpCommand.length();
		//this contains a return char that we need to delete. I just wrote len-1 instead of len -shai.
		write(1, tmpCommand.c_str(), len-1);
		strcpy(command,tmpCommand.c_str());
		myindex = len-1;
		historyLocation++;
	}
	else {
		write(1, "\a", 1);
	}
}

void downInHistory() {
	if(historyLocation > 0 && historyLocation <= listedHistory.size()) {
		int len;
		clearLine(myindex);
		string tmpCommand;
		if (historyLocation < 2) {
			tmpCommand = "";
			myindex = 0;
			len = 1;
		}
		else {		
			tmpCommand = listedHistory.at(listedHistory.size() - historyLocation + 1);
			len = tmpCommand.length();
			myindex = len-1;

		}
		write(1, tmpCommand.c_str(), len-1);
		strcpy(command,tmpCommand.c_str());
		historyLocation--;
	}
	else {
		write(1, "\a", 1);
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
	myindex = 0;
	historyLocation = 0;
	//clear command before next iteration
	memset(command, '\0', 100);
	do {
		read(0, currChar, 1);

		if((currChar[0] == deleted1 || currChar[0] == deleted2) && myindex > 0) {
			write(1, "\b \b", 3);
			command[myindex] = '\0';
			myindex--;
		}
		else if(isprint(currChar[0]) || currChar[0] == '\n') {
			command[myindex] = currChar[0];
			myindex++;
			write(1, currChar, 1);
		}
		else if(currChar[0] == escape) {
			checkArrow();
		}
	} while (currChar[0] != '\n');
	checkCommand();
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






// NITTAS COMMENTS:
// read(STDIN_FILENO, Buffer, size); //STDIN_FILENO = 0 generally
// write(1, Buffer, size);
// "\b \b"
// 0x08 and 0x7F treat them the same

