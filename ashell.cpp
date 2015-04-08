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
		std::stringstream ss;
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

void backspace() {
	char tmp[3] = {'\b', ' ', '\b'};
	write(1, tmp, 3);
}


// void uparrow() {
//  read(0, command, 100);
//  string temp(command);
//  if(temp == "0x2")
//     read(0, command, 100);
//     if(temp == "0x2")
//          cout << "UP ARROW MOFO";

// }

void getCommand(){
	int myindex = 0;
	//clear command before next iteration
	memset(command, '\0', 100);
	char deleted1 = 0x08;
	char deleted2 = 0x7F;
	do {
		read(0, &currChar, 1);

		if(currChar[0] == '\b' || currChar[0] == deleted1 || currChar[0] == deleted2) {  // or 0x7F) {
			backspace();
		}
		command[myindex] = currChar[0];
		myindex++;
		write(1, &currChar, 1);

	} while (currChar[0] != '\n');
	string temp(command);
	// exits the program
	addToHistory(command);
	if(temp == "exit\n") exitStatus = 0;
	else if(temp == "pwd\n") pwd();
	else if(temp == "ls\n") ls();
	else if(temp == "cd\n") cd();
	else if(temp == "history\n") history();
	// else if(temp == "0x1") uparrow();
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

