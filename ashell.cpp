#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>      // std::stringstream, std::stringbuf
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "noncanmode.c"
#include <deque>
#include <stack>
#include <termios.h>
#include <dirent.h>
#include <vector>

#define BUFFER_SIZE 1024

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
char command[BUFFER_SIZE];
//broken down command by spaces
vector<char*> delimitedCommand;
//arg array
vector<char*> argVector;
//char buffer
char currChar[1];
//start directory
char startDirectory[BUFFER_SIZE];
//char array for current working directory absolute path
char currentDirectory[BUFFER_SIZE];
//queue of char arrays that will correspond to history of commands
deque<string> listedHistory;
//vector of char arrays
stack<string> delimitedCurrDirectory;


//gets current directory absolute path name
void getCurrentDirectory(){
	memset(currentDirectory, '\0', BUFFER_SIZE);
	getcwd(currentDirectory, BUFFER_SIZE);
}

void delimit(){
	string temp = "";
	for (int i = 0; i < BUFFER_SIZE; i++) {
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

void delimitCommand(){
	// for(int k = 0; k < delimitedCommand.size(); k++) {
	// 	memset(delimitedCommand.at(k), '\0', BUFFER_SIZE);
	// }
	delimitedCommand.clear();
	string tmp = "";
	// int count = 0;
	for(int i = 0; i < strlen(command); i++) {
		if(command[i] == ' ' || command[i] == '\n'){
			char *tempChar = new char [BUFFER_SIZE];
			// if (command[i] == '\n') tmp += command[i];
			for (int j = 0; j < tmp.length(); j++) {
				tempChar[j] = tmp[j];
			}
			delimitedCommand.push_back(tempChar);
			// count++;
			tmp = "";
			if (command[i] == '\n') break;
		}
		else if (isprint(command[i]))
		{
			tmp += command[i];
		}
	}
	// command list has to be terminated by a NULL
	// delimitedCommand.push_back(NULL);

	// print for checking
	// for (int i = 0; i < delimitedCommand.size(); ++i)
	// {
	// 	write(1, delimitedCommand[i], strlen(delimitedCommand[i]));
	// 	write(1, "\n", 1);	
	// }
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
	// int ret = chdir(com)
}

// The ls command you will be doing internally needs to do the following:
// 1. open the directory specified (if none specified open up ./) use the opendir from dirent.h 
// 2. for each entry in the directory:
//   2.1 read in the entry using readdir (actually readdir will return NULL if at the end so might be while loop)
//   2.2 get the permissions of the entry using the ->d_name and a call to stat (see the stat system call also don't forget to concatenate the directory name before calling stat)
//   2.3 output the permissions rwx, etc then output the entry name ->d_name
// 3. close the directory using closedir
void ls() {
	struct dirent **entries;
	struct stat permissions;
	int status = scandir(currentDirectory, &entries, NULL, NULL);
	if(status != -1) {
		for(int i = 0; i < status; i++){
			string currDir(currentDirectory);
			string browsedFile(entries[i]->d_name);
			string filename = currDir + "/" + browsedFile;
			string perms = "";
			stat(filename.c_str(), &permissions);

			int tmp = permissions.st_mode;
			if(S_ISDIR(tmp)) perms += "d";
			else perms += "-";
			if((tmp & S_IRUSR) == S_IRUSR) perms += "r";
			else perms += "-";
			if((tmp & S_IWUSR) == S_IWUSR) perms += "w";
			else perms += "-";
			if((tmp & S_IXUSR) == S_IXUSR) perms += "x";
			else perms += "-";
			if((tmp & S_IRGRP) == S_IRGRP) perms += "r";
			else perms += "-";
			if((tmp & S_IWGRP) == S_IWGRP) perms += "w";
			else perms += "-";
			if((tmp & S_IXGRP) == S_IXGRP) perms += "x";
			else perms += "-";
			if((tmp & S_IROTH) == S_IROTH) perms += "r";
			else perms += "-";
			if((tmp & S_IWOTH) == S_IWOTH) perms += "w";
			else perms += "-";
			if((tmp & S_IXOTH) == S_IXOTH) perms += "x ";
			else perms += "- ";


			write(1, perms.c_str(), 11);
			write(1, browsedFile.c_str(), 50);
			write(1, "\n", 1);
			free(entries[i]);
		}

	}
}


void pwd() {
	getCurrentDirectory();
	write(1, currentDirectory, BUFFER_SIZE);
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
	memset(command, '\0', BUFFER_SIZE);

}

void execute(string temp) {
	if(temp == "exit\n") exitStatus = 0;
	else if(temp == "pwd\n") pwd();
	else if(temp == "ls\n") ls();
	// else if(temp == "cd\n") cd();
	else if(temp == "history\n") history();
	else {
		// have to convert vector to array to be able to pass into execvp. instead of copying 
		// vector i just created a pointed to a char * and set it to front of vector.
		char** argArray = &argVector[0];
		execvp(argArray[0], argArray);
	}
}

// handles the forking/piping/duping of commands
void checkCommandType() {
	string temp(command);
	if (!strcmp(argVector[0], "cd") || !strcmp(argVector[0], "exit"))
	{
		execute(temp);
	}
	// no redirection requires no dup2() or pipe()
	else if (temp.find("|") == string::npos && temp.find("<") == string::npos && temp.find(">") == string::npos) {
		// http://timmurphy.org/2014/04/26/using-fork-in-cc-a-minimum-working-example/
		pid_t pid = fork();
		// child process
		if (pid == 0) {
			execute(temp);
			// exit 0 from child process
			exit(0);
		}
		// parent process
		else if (pid > 0) {
			// wait for children to complete then return. PROBLEM: exit doesn't work because it only
			// changes exitStatus in child process. global vars aren't shared between processes so 
			// we need to return the exitStatus from the child processes i.e. "exit(0)". Also, if the
			// command contains a "&" at the end we dont need to wait b/c it's running in bg.
			// if (strcmp(argVector.back(), "&")) 
			// {
				wait(NULL);
			// }
			return;
		}
	}
}

// arg array needed for passing into execvp()
void makeArgVector() {
	for (int i = 0; i < delimitedCommand.size(); ++i)
	{
		// add to arg Array if not "<" or ">" or "|"
		if (string(delimitedCommand[i]).find("|") == string::npos && string(delimitedCommand[i]).find("<") == string::npos && string(delimitedCommand[i]).find(">") == string::npos) {
			argVector.push_back(delimitedCommand[i]);
		}
	}
	// command list has to be terminated by a NULL
	argVector.push_back(NULL);
}

// need to handle case where piping/redirection doesnt have spaces "a.txt<b.txt"
void checkCommandArgs () {
	string temp(command);
	addToHistory(command);
	delimitCommand();
	makeArgVector();
	checkCommandType();
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
	memset(command, '\0', BUFFER_SIZE);
	argVector.clear();
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
	checkCommandArgs();
}



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

