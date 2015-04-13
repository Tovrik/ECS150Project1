#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>      // std::stringstream, std::stringbuf
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
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
bool isAmpersand = 0;
int historyLocation = 0;
int myindex = 0;

// command buffer
char command[BUFFER_SIZE];
//broken down command by spaces
vector<char*> delimitedCommand;

vector<char*> delimtedByPipeCommand;
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
    for(int i = 0; i < delimitedCurrDirectory.size(); i++) {
        delimitedCurrDirectory.pop();
    }
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

void delimitCommand(char* command){
    // for(int k = 0; k < delimitedCommand.size(); k++) {
    //  memset(delimitedCommand.at(k), '\0', BUFFER_SIZE);
    // }
    delimitedCommand.clear();
    string tmp = "";
    // int count = 0;
    for(int i = 0; i < strlen(command); i++) {
        if((command[i] == ' ' || command[i] == '\n') && tmp != ""){
            if (command[i] == ' ' && i == strlen(command)-1) {
                break;
            }
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
            if(command[i] != ' ')
                tmp += command[i];
        }
    }
}

void delimitByPipe(){
    delimtedByPipeCommand.clear();
    string tmp = "";

    for (int i = 0; i < strlen(command); i++) {
        if(command[i] == '|' || command[i] == '\n') {
            char *tempChar = new char [BUFFER_SIZE];
            // if (command[i] == '\n') tmp += command[i];
            int j;
            for (j = 0; j < tmp.length(); j++) {
                tempChar[j] = tmp[j];
            }
            tempChar[j] = '\n';
            delimtedByPipeCommand.push_back(tempChar);
            tmp = "";
            if (command[i] == '\n') break;
        }
        else if(isprint(command[i])) {
            tmp += command[i];
        }
    }
}

// arg array needed for passing into execvp()
void makeArgVector() {
    for (int i = 0; i < delimitedCommand.size(); ++i)
    {
        // add to arg Array if not "<" or ">"
        if (string(delimitedCommand[i]).find("<") == string::npos && string(delimitedCommand[i]).find(">") == string::npos) {
            argVector.push_back(delimitedCommand[i]);
        }
    }
    argVector.push_back(NULL);
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
    if(delimitedCommand.size() > 1) {
        // string tmp(delimitedCommand[1]);
        // write(1, delimitedCommand[1], tmp.length());
        int ret = chdir(delimitedCommand[1]);
        if(ret != -1) {
            getCurrentDirectory();
            delimit();
        }
    }
    else {
        int ret = chdir(getenv("HOME"));
        if(ret != -1) {
            getCurrentDirectory();
            delimit();
        }
    }
}


void ls() {
    struct dirent **entries;
    struct stat permissions;
    //returns -1 if error, otherwise populates entries with strings of file names and returns number of files
    int status;
    if(delimitedCommand.size() == 1) {
        status = scandir(currentDirectory, &entries, NULL, NULL);
    }
    else {
        status = scandir(delimitedCommand[1], &entries, NULL, NULL);
    }
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
            write(1, browsedFile.c_str(), browsedFile.length());
            write(1, "\n", 1);
            free(entries[i]);
        }
        free(entries);
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

int execute(string temp) {
    string tmpString(delimitedCommand[0]);
    if(temp == "exit\n") exitStatus = 0;
    else if(tmpString == "pwd") pwd();
    else if(tmpString == "ls") ls();
    else if(tmpString == "cd") cd();
    else if(tmpString == "history") history();
    else {

        char** argArray = &argVector[0];

        execvp(argArray[0], argArray);
        return 1;
    }
    return 0;
}

void redirectIn() {
    char* path_name;
    for (int i = 0; i < delimitedCommand.size(); ++i)
    {
        if (!strcmp(delimitedCommand[i], "<"))
        {
            path_name = delimitedCommand[i+1];
        }
    }
    int fd = open(path_name, O_RDONLY, S_IRWXU | S_IRWXG | S_IRWXO);
    dup2(fd, 0);
    close(fd);
}

void redirectOut() {
    char* path_name;
    for (int i = 0; i < delimitedCommand.size(); ++i)
    {
        if (!strcmp(delimitedCommand[i], ">"))
        {
            path_name = delimitedCommand[i+1];
        }
    }
    int fd = open(path_name, O_WRONLY | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
    dup2(fd, 1);
    close(fd);
}

// To answer your question, no, pipe and dup2 are for redirecting the standard input/output from a child to a parent, or child to child, etc. All input/output is read/written using read() and write() ultimately. Even if you are using printf, or scanf it ultimately calls read or write. The reason for dup2() is to duplicate a file descriptor. Remember the file descriptors are just indices into the array of open files (the OS maintains this array). What dup2 does is copies one of the entries in the array to another location. So if you want to redirect the output of a program to output to a file you can:
// 1. Open the file with open() to get a file descriptor for the file
// 2. Call dup2() to redirect the file descriptor to STDOUT_FILENO (also happens to be file descriptor 1).
// 3. Close the original file descriptor returned by open(). This entry in the array isn't needed anymore.
// 4. Continue executing the program with all standard out going to the new file. Alternatively you can make the program "become" another program through a call to exec(), execvp(), or execlp(), etc.

// The pipe() call creates a pipe. A pipe is just a FIFO what ever is written in one end gets read out the other end. We use piping to redirect the output of one program into the input of another. Lets say you want to dump the contents of main.cpp, and grep the lines that have void in them. From a shell you could use the command:
// cat main.cpp | grep void
// This is what will happen, and outline of how your shell should handle this:
// 1. Parent shell displays command prompt.
// 2. User enters: cat main.cpp | grep void
// 3. Parent shell interprets the input and determines to create children and a pipe.
// 4. Parent creates a pipe so that the children (cat and grep) can use them.
// 5. Parent shell will fork() to create child (cat).
//   1C1. Child will redirect standard out STDOUT_FILENO (or file descriptor 1) to point to the write end of the pipe [1] using dup2().
//   2C1. Child optionally can close pipe ends, it doesn't need those file descriptors anymore.
//   3C1. Child calls execvp() (or some derivative) to become cat with arguments cat, and main.cpp
//   4C1. Child is now cat and output will be directed into the pipe
// 6. Parent shell will fork() again to create child (grep).
//   1C2. Child will redirect standard in STDIN_FILENO (or file descriptor 0) to point to the write end of the pipe [0] using dup2().
//   2C2. Child optionally can close pipe ends, it doesn't need those file descriptors anymore.
//   3C2. Child calls execvp() (or some derivative) to become grep with arguments grep, and void
//   4C2. Child is now grep and input comes from the pipe
// 7. Parent waits for both children to terminate using wait() or waitpid()
// 8. Parent displays command prompt again.

// handles the forking/piping/duping of commands
void checkCommandType() {
    string temp(command);
    vector<int> pids;
    int pipefd[2];
    int previnpipe = 0;
    bool piping = false;
    bool send_piped_content = false;
    if(argVector[0] == NULL) return;
    if (!strcmp(argVector[0], "cd") || !strcmp(argVector[0], "exit"))
    {
        execute(temp);
    }
    else {
        for (int i = 0; i < delimtedByPipeCommand.size(); ++i)
        {
            delimitedCommand.clear();
            argVector.clear();
            string temp2(delimtedByPipeCommand[i]);
            delimitCommand(delimtedByPipeCommand[i]);
            makeArgVector();
            if (delimtedByPipeCommand.size() > 1 && i < delimtedByPipeCommand.size() - 1)
            {
                pipe(pipefd);
                piping = true;
            }
            else {
                piping = false;
            }

            // http://timmurphy.org/2014/04/26/using-fork-in-cc-a-minimum-working-example/
            pid_t pid = fork();
            // child process
            if (pid == 0) {
                // reading file in
                if(temp2.find("<") != string::npos) {
                    redirectIn();
                }
                // outputting to file
                if(temp2.find(">") != string::npos) {
                    redirectOut();
                }
                if(piping)
                {
                    close(pipefd[0]);
                    dup2(pipefd[1], 1);
                }
                if(send_piped_content) {
                    send_piped_content = false;
                    dup2(previnpipe, 0);
                }

                exit(execute(temp2));
            }
            // parent process
            if (pid > 0) {

                if(piping){
                    close(pipefd[1]);
                }
                wait(&pid);
                if(previnpipe != 0) {
                    close(previnpipe);
                }
                if(piping){
                    previnpipe = pipefd[0];
                    send_piped_content = true;
                }
                else{
                    close(pipefd[0]);
                }
            }
        }
    }
}

// need to handle case where piping/redirection doesnt have spaces "a.txt<b.txt"
void checkCommandArgs () {
    string temp(command);
    addToHistory(command);
    delimitByPipe();
    delimitCommand(delimtedByPipeCommand[0]);
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

        if((currChar[0] == deleted1 || currChar[0] == deleted2)) {

            if(myindex > 0) {
                write(1, "\b \b", 3);
                command[myindex] = '\0';
                myindex--;
            }
            else{
                write(1, "\a", 1);
            }
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

