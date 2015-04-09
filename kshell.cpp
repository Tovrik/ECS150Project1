#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <dirent.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <vector>
#include <iostream>

using namespace std;

void ResetCanonicalMode(int fd, struct termios *savedattributes)
{
    tcsetattr(fd, TCSANOW, savedattributes);
}


void SetNonCanonicalMode(int fd, struct termios *savedattributes)
{
    struct termios TermAttributes;
    // Make sure stdin is a terminal.
    if(!isatty(fd)) {
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


static void clearCurrentAndWriteNewBuffer(string old_buf,
                                          string new_buf,
                                          string line_prefix) {
    string write_buf = "\r" + string(line_prefix.size() + old_buf.size(), ' ')
                       + "\r" + line_prefix + new_buf;
    write(STDOUT_FILENO, write_buf.c_str(), write_buf.size());
}


static void scrollUpDown(string *buffer,
                         vector<string> *history,
                         int8_t *history_index,
                         string line_prefix,
                         bool scrollUp = true) {

    if (*history_index == history->size()) {
        history->push_back(*buffer);
    } else if (*history_index == history->size() - 1) {
        history->erase(history->begin() + *history_index);
        history->insert(history->begin() + *history_index, *buffer);
    }
    if (scrollUp) {
        (*history_index)--;
        if (*history_index < 0) {
            *history_index = history->size() - 1;
        }
    } else {
        (*history_index)++;
        if (*history_index > history->size() - 1) {
            *history_index = 0;
        }
    }

    clearCurrentAndWriteNewBuffer(*buffer,
                                  history->at(*history_index),
                                  line_prefix);
    *buffer = history->at(*history_index);
}


static string trimEdges(string input) {
    size_t left = input.find_first_not_of(" \n\r\t");
    size_t right = input.find_last_not_of(" \n\r\t");
    if (left == string::npos || right == string::npos) {
        return "";
    }
    return input.substr(left, right - left + 1);
}


vector<string> parseCommand(string input) {
    vector<string> command;
    size_t edge = input.find_first_of(" |\t", 0);
    if (edge == string::npos) {
        command.push_back(input);
        return command;
    }
    command.push_back(input.substr(0, edge));
    size_t old_edge = edge;
    edge = input.find_first_of(" |\t", edge + 1);
    while (edge != string::npos) {
        command.push_back(input.substr(old_edge + 1, edge - old_edge));
        old_edge = edge;
        edge = input.find_first_of(" |\t", edge + 1);
    }
    command.push_back(input.substr(old_edge + 1, input.size() - old_edge));
    for (size_t i = 0; i < command.size(); i++) {
        if (trimEdges(command[i]) == "") {
            command.erase(command.begin() + i);
            i--;
        }
    }
    return command;
}


void updatePrefix(string current_dir, string *line_prefix) {
    size_t last_delim = current_dir.find_last_of("/");
    string post_dir = current_dir.substr(last_delim + 1,
        current_dir.size() - last_delim - 1);
    *line_prefix = "[kevin@host " + post_dir + "]$ ";
}


int main(int argc, char *argv[])
{
    struct termios SavedTermAttributes;
    SetNonCanonicalMode(STDIN_FILENO, &SavedTermAttributes);

    string current_dir = getcwd(NULL, 0);
    string line_prefix;
    updatePrefix(current_dir, &line_prefix);

    char RXChar;
    string buffer = "";
    vector<string> history;
    uint8_t combo_key = 0;
    int8_t history_index = 0;
    bool line_begin = false;
    while (true) {
        if (line_begin == false) {
            write(STDOUT_FILENO, line_prefix.c_str(), line_prefix.size());
            line_begin = true;
        }
		ssize_t ret = read(STDIN_FILENO, &RXChar, 1);
        if (ret != 1) {
            cerr << "Reading input failed with: " << strerror(errno) << endl;
            return EXIT_FAILURE;
        }
        else if (RXChar == 0x7F) {
            // backspace
            if (buffer.size() > 0) {
                buffer.erase(buffer.size() - 1);
                write(STDOUT_FILENO, "\b \b", 3);
            } else {
                // otherwise ring the bell
                write(STDOUT_FILENO, "\a", 1);
            }
        } else if (RXChar == 0x1b && combo_key == 0) {
            /**
             * detecting an up or down arrow requires a a combination of chars:
             * \ + [ + A -- left arrow key
             * \ + [ + B -- right arrow key
             *
             * this is the '\' of the combination key
             */
            combo_key = 1;
        } else if (RXChar == 0x5b && combo_key == 1) {
            // the '[' of the combination key
            combo_key = 2;
        } else if (RXChar == 0x41 && combo_key == 2) {
            // up arrow, this is the 'A' of the combination key
            combo_key = 0;
            scrollUpDown(&buffer, &history, &history_index, line_prefix);

        } else if (RXChar == 0x42 && combo_key == 2) {
            // down arrow, this is the 'B' of the combination key
            combo_key = 0;
            scrollUpDown(&buffer, &history, &history_index, line_prefix,
                false);

        } else if (combo_key == 0) {
            write(STDOUT_FILENO, &RXChar, 1);
            if (RXChar == 0x0a) {
                line_begin = false;
                // delete the empty entry in history if we scrolled to input
                if (!history.empty()) {
                    if (trimEdges(history.back()) == "") {
                        history.pop_back();
                    }
                }
                // if we are at an entry from history move that to the front
                if (history_index != history.size() &&
                        buffer == history[history_index]) {
                    history.erase(history.begin() + history_index);
                }
                // parse input
                if (trimEdges(buffer) != "") {
                    history.push_back(buffer);
                    if (history.size() > 10) {
                        history.erase(history.begin());
                    }
                    vector<string> command = parseCommand(trimEdges(buffer));
                    if (command.size() == 1 && command[0] == "exit") {
                        break;
                    } else if (command.size() == 1 && command[0] == "history") {
                        // history
                        cout << "History of past commands, max 10: " << endl;
                        for (size_t i = 0; i < history.size(); i++) {
                            cout << i << ": " << history[i] << endl;
                        }
                        line_begin = false;
                    } else if (command.size() == 1 && command[0] == "pwd") {
                        cout << getcwd(NULL, 0) << endl;
                    } else if (command.size() == 1 && command[0] == "ls") {
                        struct dirent **entries;
                        int stat = scandir(current_dir.c_str(), &entries,
                            NULL, NULL);
                        if (stat != -1) {
                            for (int i = 0; i < stat; i++) {
                                cout << entries[i]->d_name << endl;
                            }
                        } else {
                            cerr << command[1] << ": " << strerror(errno) << endl;
                        }
                    } else if (command.size() == 2 && command[0] == "cd") {
                        int ret = chdir(command[1].c_str());
                        if (ret != -1) {
                            current_dir = getcwd(NULL, 0);
                            updatePrefix(current_dir, &line_prefix);
                        } else {
                            cerr << command[1] << ": " << strerror(errno) << endl;
                        }

                    }
                    history_index = history.size();
                }
                buffer = "";
            } else {
                buffer += RXChar;
            }
        } else {
            combo_key = 0;
        }
    }
    ResetCanonicalMode(STDIN_FILENO, &SavedTermAttributes);
    return EXIT_SUCCESS;
}
