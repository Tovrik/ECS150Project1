#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <unistd.h>
#include <queue>
using namespace std;

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
