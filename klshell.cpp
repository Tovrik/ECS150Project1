#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <termios.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <algorithm>
#include <string>
#include <utility>
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


static inline void string_out(string output_string, int fd = STDOUT_FILENO)
{
    write(fd, output_string.c_str(), output_string.size());
}


static inline void invalid_arguments(string command_name)
{
    string error_string = command_name + ": invalid arguments\n";
    string_out(error_string, STDERR_FILENO);
}


struct Window
{
    struct winsize w;

    void updateWinsize()
    {
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    }

    int getWindowColumns()
    {
        updateWinsize();
        return w.ws_col;
    }

    int getWindowLines()
    {
        updateWinsize();
        return w.ws_row;
    }
} w;


struct KLShell
{
    pid_t pid;
    pid_t child_pid;
    string current_dir;
    string prefix_left_brace;
    string prefix_right_brace;
    string input_prefix;
    string line_prefix;
    vector<string> history;
    int8_t history_index;
    size_t history_limit;
    string buffer;
    string buffer_left;
    string buffer_right;
    int16_t autocomplete_index; // potentially limited
    string original_auto_search;
    string original_auto_buffer_begin;
    size_t backsearch_index;
    string backsearch_query;
    string backsearch_prefix;
    string backsearch_prefix_fail;

    KLShell()
    {
        // default options
        pid = 0;
        child_pid = 0;
        current_dir = "";
        prefix_left_brace = "{";
        prefix_right_brace = "}";
        input_prefix = "$";
        line_prefix = "";
        history_index = 0;
        history_limit = 10;
        buffer = "";
        buffer_left = "";
        buffer_right = "";
        autocomplete_index = 0;
        original_auto_search = "";
        original_auto_buffer_begin = "";
        backsearch_index = 0;
        backsearch_query = "";
        backsearch_prefix = "back-search:";
        backsearch_prefix_fail = "failing-back-search:";
    }

} sh;


static inline void ring_bell()
{
    write(STDOUT_FILENO, "\a", 1);
}


/* clear current input line and re-write with new one, appending line prefix */
static void clearCurrentAndWriteNewBuffer(string old_buf,
                                          string new_buf,
                                          string line_prefix,
                                          int rewind_count = -1)
{
    string write_buf = "\r" + string(line_prefix.size() + old_buf.size(), ' ')
                       + "\r" + line_prefix + new_buf;
    string_out(write_buf);
    if (rewind_count != -1) {
        for (int i = 0; i < rewind_count; i++) {
            string_out("\b");
        }
    }
}


/* columnate input as wide as terminal */
static void printWideColumns(vector<string> input)
{
    for (int i = 0; i < input.size(); i++) {
        string_out(input[i]);
        string_out("\n");
    }
    return;

    const string column_divider = "  ";
    int columns_available = w.getWindowColumns();

    int max_columns_needed = input.size();
    vector<int> column_indices;

    for (int attempt_col_count = max_columns_needed;
            attempt_col_count > 1;
            attempt_col_count--) {
        /**
         * the only available column configurations are those where:
         * [item count] % ([item count] / [columns]) is 0 or 1
         *
         *
         * like:
         * 1 2 3 4 5
         * 6 7 8 9
         * A B C D
         * 13 % (13 / 5) = 1
         *
         * 1 2 3
         * 4 5 6
         * 6 % (6 / 3) = 0
         *
         * 1 2 3 4 5
         * 6 7 8 9 A
         * B C D E
         * 14 % (14 / 5) = 1
         *
         * 1 2 3 4 5 6
         * 7 8 9 A B C
         * D E F G H
         * 17 % (17 / 6) = 1
         *
         * 1 2 3 4 5 6 7 8
         * 9 A B C D E F G
         * H I J K L M N O
         * 24 % (24 / 8) = 0
         *
         * 1 2 3 4 5 6 7 8
         * 9 A B C D E F G
         * H I J K L M N
         * 23 % (23 / 8) = 1
         *
         * we can filter all the non 0s and 1s out, then just check the
         * total size against the window width (column count)
         */
        int check = input.size() % (input.size() / attempt_col_count);
        if (check != 0 && check != 1) {
            continue;
        }
    }
}


/* cycle up and down through history */
static void scrollUpDown(bool scrollUp = true, bool allowScrollWrap = false)
{
    if (sh.history_index == sh.history.size()) {
        sh.history.push_back(sh.buffer);
    } else if (sh.history_index == sh.history.size() - 1) {
        sh.history.erase(sh.history.begin() + sh.history_index);
        sh.history.insert(sh.history.begin() + sh.history_index,
            sh.buffer);
    }
    if (scrollUp) {
        sh.history_index--;
        if (sh.history_index < 0) {
            if (allowScrollWrap) {
                sh.history_index = sh.history.size() - 1;
            } else {
                sh.history_index = 0;
                ring_bell();
            }
        }
    } else {
        sh.history_index++;
        if (sh.history_index > sh.history.size() - 1) {
            if (allowScrollWrap) {
                sh.history_index = 0;
            } else {
                sh.history_index = sh.history.size() - 1;
                ring_bell();
            }
        }
    }

    clearCurrentAndWriteNewBuffer(sh.buffer,
                                  sh.history[sh.history_index],
                                  sh.line_prefix);
    sh.buffer = sh.history[sh.history_index];
}


/* move left and right on current input line */
static void scrollLeftRight(bool scrollLeft = true)
{
    size_t cursor_index = sh.buffer.size() - sh.buffer_right.size();
    if (scrollLeft) {
        if (sh.buffer_right == "") {
            sh.buffer_left = sh.buffer;
        }
        if (cursor_index == 0) {
            ring_bell();
            return;
        }
        sh.buffer_right.insert(0, 1,
            sh.buffer_left[sh.buffer_left.size() - 1]);
        sh.buffer_left.erase(sh.buffer_left.size() - 1, 1);
        clearCurrentAndWriteNewBuffer(sh.buffer,
                                      sh.buffer,
                                      sh.line_prefix,
                                      sh.buffer_right.size());
    } else {
        if (cursor_index == sh.buffer.size()) {
            ring_bell();
            return;
        }
        sh.buffer_left.push_back(sh.buffer_right[0]);
        sh.buffer_right.erase(0, 1);
        clearCurrentAndWriteNewBuffer(sh.buffer,
                                      sh.buffer,
                                      sh.line_prefix,
                                      sh.buffer_right.size());
    }
}


/* remove whitespace on left/right edges */
static string trimEdges(string input)
{
    size_t left = input.find_first_not_of(" \n\r\t");
    size_t right = input.find_last_not_of(" \n\r\t");
    if (left == string::npos || right == string::npos) {
        return "";
    }
    return input.substr(left, right - left + 1);
}


/* convert int to string for printing */
static inline string intToString(int input)
{
    string total = "";
    string limit;
    while (true) {
        limit = '0' + input%10;
        total.insert(0, limit);
        if (input < 10) {
            break;
        }
        input /= 10;
    }
    return total;
}


/* update input line prefix */
static void updatePrefix()
{
    uid_t uid = geteuid();
    struct passwd *pd = getpwuid(uid);
    string user = "user";
    if (pd) {
        user = pd->pw_name;
    }
    char hostname[255];
    gethostname(hostname, 255);

    size_t last_delim = sh.current_dir.find_last_of("/");
    string post_dir = sh.current_dir.substr(last_delim + 1,
        sh.current_dir.size() - last_delim - 1);
    if (post_dir == "") {
        post_dir = "/";
    }

    sh.line_prefix = sh.prefix_left_brace +
                     user + "@" + hostname + " " + post_dir +
                     sh.prefix_right_brace + sh.input_prefix + " ";
}


/* break input string into substrings based on whitespace */
static vector<string> parseCommand(string input)
{
    vector<string> command;
    size_t edge = input.find_first_of(" |\t", 0);
    if (edge == string::npos) {
        command.push_back(input);
        return command;
    }
    command.push_back(input.substr(0, edge));
    size_t old_edge = edge;
    edge = input.find_first_of(" \t", edge + 1);
    while (edge != string::npos) {
        command.push_back(input.substr(old_edge + 1, edge - old_edge - 1));
        old_edge = edge;
        edge = input.find_first_of(" \t", edge + 1);
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


typedef pair< string, vector<string> > cmdArgs;


/* break commands into vector of commands + arguments */
static vector<cmdArgs> breakCommand(vector<string> input)
{
    vector<cmdArgs> commands;
    string cmd = "";
    vector<string> args;
    for (size_t i = 0; i < input.size(); i++) {
        if (input[i] != "|") {
            if (cmd == "") {
                cmd = input[i];
            } else {
                args.push_back(input[i]);
            }
        } else {
            commands.push_back(cmdArgs(cmd, args));
            cmd = "";
            args.clear();
        }
    }
    if (cmd != "") {
        commands.push_back(cmdArgs(cmd, args));
    }
    return commands;
}


/**
 * iterate through args in cmdArgs for a redirect
 * if so, pop it out of args and put it into redirect_out with a specific format
 *
 * format: type: "<" ">" ">>"
 *         file descriptor: "STDIN_FILENO" "STDOUT_FILENO" "STDERR_FILENO"
 *         filename
 *     or "error" in the first slot if there's a problem parsing
 *
 */
static void detectRedirect(vector< vector<string> > *redirects_out,
                           cmdArgs *input)
{
    vector<string> list;
    bool have_stdin = false;
    bool have_stdout = false;
    bool have_stderr = false;

    for (int i = 0; i < input->second.size(); ) {
        size_t in_pos = input->second[i].find("<");
        size_t out_pos = input->second[i].find(">");
        if (in_pos != string::npos) {
            if (in_pos == 0) {
                if (!have_stdin) {
                    string filename = input->second[i].substr(1,
                        input->second[i].size());
                    if (filename == "") {
                        if (i < (int)input->second.size() - 1) {
                            filename = input->second[i + 1];
                            input->second.erase(input->second.begin() + i + 1);
                        }
                    }
                    if (filename != "") {
                        if (input->second[i].find("<", 1) == string::npos) {
                            have_stdin = true;
                            list.clear();
                            list.push_back("<");
                            list.push_back("STDIN_FILENO");
                            list.push_back(filename);
                            redirects_out->push_back(list);
                            input->second.erase(input->second.begin() + i);
                            continue;
                        }
                    }
                }
            }
            list.clear();
            redirects_out->clear();
            list.push_back("error");
            redirects_out->push_back(list);
            break;

        } else if (out_pos != string::npos) {
            if (out_pos == 0 || out_pos == 1) {
                bool append = false;
                if (input->second[i].find(">", out_pos + 1) == out_pos + 1) {
                    append = true;
                    out_pos++;
                }
                string filename = input->second[i].substr(out_pos + 1,
                    input->second[i].size());
                if (filename == "" &&
                        (out_pos == 0 || out_pos == 1 && append)) {
                    if (i < (int)input->second.size() - 1) {
                        filename = input->second[i + 1];
                        input->second.erase(input->second.begin() + i + 1);
                    }
                }
                if (filename != "") {
                    if (out_pos == 0 || out_pos == 1 && append ||
                            input->second[i][0] == '1') {
                        if (!have_stdout) {
                            if (input->second[i].find(">", out_pos + 1) ==
                                    string::npos) {
                                have_stdout = true;
                                list.clear();
                                if (!append) {
                                    list.push_back(">");
                                } else {
                                    list.push_back(">>");
                                }
                                list.push_back("STDOUT_FILENO");
                                list.push_back(filename);
                                redirects_out->push_back(list);
                                input->second.erase(input->second.begin() + i);
                                continue;
                            }
                        }
                    } else if (input->second[i][0] == '2'){
                        if (!have_stderr) {
                            if (input->second[i].find(">", out_pos + 1) ==
                                    string::npos) {
                                have_stderr = true;
                                list.clear();
                                if (!append) {
                                    list.push_back(">");
                                } else {
                                    list.push_back(">>");
                                }
                                list.push_back("STDERR_FILENO");
                                list.push_back(filename);
                                redirects_out->push_back(list);
                                input->second.erase(input->second.begin() + i);
                                continue;
                            }
                        }
                    }
                }
            }
            list.clear();
            redirects_out->clear();
            list.push_back("error");
            redirects_out->push_back(list);
            break;

        } else {
            i++;
        }
    }
}


/* returns string of single alphabetical chars representing options */
static string optionParser(vector<string> *input)
{
    string result = "";
    vector<string> only_options;
    for (int i = 0; i < input->size(); ) {
        if (input->at(i).find("--") == 0) {
            // LONG OPTION NOT SUPPORTED
            result = "ERROR";
            return result;
        } else if (input->at(i).find("-") == 0) {
            only_options.push_back(input->at(i));
            input->erase(input->begin() + i);
            continue;
        }
        i++;
    }
    for (int i = 0; i < only_options.size(); i++) {
        result += only_options[i].substr(1, only_options[i].size());
    }
    sort(result.begin(), result.end());
    if (result.size() == 1 || result.empty()) {
        return result;
    }
    for (int i = 0; i < result.size() - 1; i++) {
        if (result[i] == result[i + 1]) {
            result = "ERROR";
            break;
        }
    }
    return result;
}


/* follow are the special shell built-ins */
static int command_history(cmdArgs current_command)
{
    string options = optionParser(&current_command.second);
    if (options == "ERROR" || !options.empty()) {
        invalid_arguments(current_command.first);
        return 1;
    }

    string output_string = "History of past commands, max " +
        intToString(sh.history_limit) + ": \n";
    string_out(output_string);
    for (size_t i = 0; i < sh.history.size(); i++) {
        output_string = intToString(i) + ": " + sh.history[i] + "\n";
        string_out(output_string);
    }
    return 0;
}


static int command_pwd(cmdArgs current_command)
{
    string options = optionParser(&current_command.second);
    if (options == "ERROR" || !options.empty()) {
        invalid_arguments(current_command.first);
        return 1;
    }
    string output_string = sh.current_dir + "\n";
    string_out(output_string);
    return 0;
}


static inline string ls_rwx_conv(int value)
{
    switch (value) {
    case 0:
        return "---";
    case 1:
        return "--x";
    case 2:
        return "-w-";
    case 3:
        return "-wx";
    case 4:
        return "r--";
    case 5:
        return "r-x";
    case 6:
        return "rw-";
    case 7:
        return "rwx";
    }
}


static inline string ls_get_permissions_string(string path,
                                               bool printPermissions)
{
    string output_string = "";
    if (printPermissions) {
        struct stat st;
        lstat(path.c_str(), &st);
        if (S_ISDIR(st.st_mode)) {
            output_string = "d";
        } else {
            output_string = "-";
        }
        int val = (st.st_mode & S_IRWXU) >> 6;
        output_string += ls_rwx_conv(val);
        val = (st.st_mode & S_IRWXG) >> 6;
        output_string += ls_rwx_conv(val);
        val = (st.st_mode & S_IRWXO) >> 6;
        output_string += ls_rwx_conv(val);
        output_string += " ";
    }

    output_string += path;
    output_string += "\n";
    return output_string;
}


static void ls_write_full_string(string path, bool findHidden, bool printList)
{
    vector<string> files;
    struct dirent **entries;
    int count = scandir(path.c_str(), &entries, NULL, alphasort);
    if (count != -1) {
        for (size_t i = 0; i < count; i++) {
            string path = entries[i]->d_name;
            if (path.find(".") == 0 && !findHidden) {
                continue;
            }
            if (!printList) {
                files.push_back(path);
            } else {
                string output_string = ls_get_permissions_string(
                    path, printList);
                string_out(output_string);
            }
            free(entries[i]);
        }
        if (!printList) {
            printWideColumns(files);
        }
    } else {
        string error_string = string("ls: ") + strerror(errno) + "\n";
        string_out(error_string, STDERR_FILENO);
    }
}


static int command_ls(cmdArgs current_command)
{
    bool option_hidden = false; // -l
    bool option_list = false; // -a
    string options = optionParser(&current_command.second);
    if (options == "ERROR") {
        invalid_arguments(current_command.first);
        return 1;

    } else {
        if (options.find("a") != string::npos) {
            options.erase(options.begin() + options.find("a"));
            option_hidden = true;
        }
        if (options.find("l") != string::npos) {
            options.erase(options.begin() + options.find("l"));
            option_list = true;
        }
        if (!options.empty()) {
            invalid_arguments(current_command.first);
            return 1;
        }
    }
    if (current_command.second.empty()) {
        ls_write_full_string(sh.current_dir, option_hidden, option_list);
        return 0;
    }
    for (size_t i = 0; i < current_command.second.size(); i++) {
        chdir(sh.current_dir.c_str());
        struct stat st;
        int ret = stat(current_command.second[i].c_str(), &st);
        if (ret == -1) {
            string error_string = "ls: no such file \"" +
                current_command.second[i] + "\"\n";
            string_out(error_string);
        } else if (S_ISREG(st.st_mode)) {
            string output_string = ls_get_permissions_string(
                current_command.second[i], option_list);
            string_out(output_string);
        } else {
            if (current_command.second.size() > 1) {
                string output_string = current_command.second[i] + ":\n";
                string_out(output_string);
            }
            chdir(current_command.second[i].c_str());
            string path = getcwd(NULL, 0);
            ls_write_full_string(path, option_hidden, option_list);
        }
        if (i != current_command.second.size() - 1) {
            write(STDOUT_FILENO, "\n", 1);
        }
    }
    chdir(sh.current_dir.c_str());
    return 0;
}


static int command_cd(cmdArgs current_command)
{
    string options = optionParser(&current_command.second);
    if (options == "ERROR" || !options.empty()) {
        invalid_arguments(current_command.first);
        return 1;
    }
    int ret;
    if (current_command.second.size() != 0){
        ret = chdir(current_command.second[0].c_str());
    } else {
        uid_t uid = geteuid();
        struct passwd *pd = getpwuid(uid);
        string dest = "/home/";
        if (pd) {
            dest = pd->pw_dir;
        }
        ret = chdir(dest.c_str());
    }

    if (ret != -1) {
        sh.current_dir = getcwd(NULL, 0);
        updatePrefix();
    } else {
        string error_string = current_command.first + ": " +
            strerror(errno) + "\n";
        string_out(error_string, STDERR_FILENO);
    }
    return 0;
}


/* close file descriptors that aren't stdin, stdout, or stderr */
static inline void closeNotSTDFD(int fd)
{
    if (fd == 0) {
        return;
    }
    if (fd != STDIN_FILENO && fd != STDOUT_FILENO && fd != STDERR_FILENO) {
        close(fd);
    }
}


/* for duplicating a fd in subprocess for redirecting input/output */
static int dupFD(int src, int dest)
{
    if (src == -1) {
        string error_string = string("Error opening file: ") +
            strerror(errno) + "\n";
        string_out(error_string, STDERR_FILENO);
        return 1;
    }
    int ret_d = dup2(src, dest);
    if (ret_d != dest) {
        closeNotSTDFD(src);
        string error_string = string("Broken pipe: ") + strerror(errno) + "\n";
        string_out(error_string, STDERR_FILENO);
        return 1;
    }
    closeNotSTDFD(src);
    return 0;
}


/**
 * Return value of 1 indicates exit/break
 * Return value of 0 indicates normal return
 */
static int runCommand(string buffer)
{
    vector<string> input = parseCommand(buffer);
    if (input.size() == 1 && input[0] == "exit") {
        return 1;
    }

    vector<cmdArgs> commands = breakCommand(input);
    cmdArgs current_command;
    int fd[2];
    int piped_fd = 0;
    int redirect_fd_stdin = 0, redirect_fd_stdout = 0, redirect_fd_stderr = 0;
    bool piping = false, send_piped_content = false;
    bool dup_fd_stdin = false, dup_fd_stdout = false, dup_fd_stderr = false;
    while (!commands.empty()) {
        current_command = commands[0];
        commands.erase(commands.begin());


        /**
         * detect if there are redirects
         * if we are redirecting input, it must be the first command
         * else if we are redirect output, it must be the last command
         */
        vector< vector<string> > redirects;
        detectRedirect(&redirects, &current_command);
        dup_fd_stdin = false;
        dup_fd_stdout = false;
        dup_fd_stderr = false;
        if (!redirects.empty()) {
            if (redirects[0][0] == "error") {
                string_out("Ambiguous redirects.\n", STDERR_FILENO);
                break;
            }
            bool badRedirect = false;
            string filename = "";
            for (size_t i = 0; i < redirects.size(); i++) {
                if (redirects[i][1] == "STDIN_FILENO") {
                    // redirect stdin
                    if (piping) {
                        string_out("Ambiguous input redirect.\n",
                            STDERR_FILENO);
                        badRedirect = true;
                        break;
                    }
                    filename = redirects[i][2];
                    if (filename.size() == 2 && filename.find("&") == 0) {
                        redirect_fd_stdin = atoi(filename.substr(1,1).c_str());
                    } else {
                        redirect_fd_stdin = open(filename.c_str(), O_RDONLY);
                    }
                    dup_fd_stdin = true;

                } else if (redirects[i][1] == "STDOUT_FILENO" ||
                        redirects[i][1] == "STDERR_FILENO") {
                    // redirect std/stderr
                    if (!commands.empty()) {
                        string_out("Ambiguous output redirect.\n",
                            STDERR_FILENO);
                        badRedirect = true;
                        break;
                    }
                    if (redirects[i][1] == "STDOUT_FILENO") {
                        filename = redirects[i][2];
                        if (filename.size() == 2 && filename.find("&") == 0) {
                            redirect_fd_stdin =
                                atoi(filename.substr(1,1).c_str());
                        } else {
                            if (redirects[i][0] == ">>") {
                                redirect_fd_stdout = open(filename.c_str(),
                                    O_CREAT | O_APPEND | O_WRONLY,
                                    S_IRUSR | S_IWUSR);
                            } else {
                                redirect_fd_stdout = open(filename.c_str(),
                                    O_CREAT | O_TRUNC | O_WRONLY,
                                    S_IRUSR | S_IWUSR);
                            }
                        }
                        dup_fd_stdout = true;

                    } else {
                        filename = redirects[i][2];
                        if (filename.size() == 2 && filename.find("&") == 0) {
                            redirect_fd_stdin =
                                atoi(filename.substr(1,1).c_str());
                        } else {
                            if (redirects[i][0] == ">>") {
                                redirect_fd_stderr = open(filename.c_str(),
                                    O_CREAT | O_APPEND | O_WRONLY,
                                    S_IRUSR | S_IWUSR);
                            } else {
                                redirect_fd_stderr = open(filename.c_str(),
                                    O_CREAT | O_TRUNC | O_WRONLY,
                                    S_IRUSR | S_IWUSR);
                            }
                        }
                        dup_fd_stderr = true;

                    }
                }
            }
            if (badRedirect) {
                break;
            }
        }


        if (!commands.empty()) {
            // then we pipe
            int ret_p = pipe(fd);
            if (ret_p == -1) {
                string error_string = string("Broken pipe: ") +
                    strerror(errno) + "\n";
                string_out(error_string, STDERR_FILENO);
                break;
            }
            piping = true;
        } else {
            piping = false;
        }

        if (current_command.first == "cd") {
            int ret_cd = command_cd(current_command);
            return ret_cd;
        }

        char *args[256];
        size_t args_index = 0;
        args[args_index++] = (char *)current_command.first.c_str();
        for (size_t i = 0; i < current_command.second.size(); i++) {
            args[args_index++] = (char *)current_command.second[i].c_str();
        }
        args[args_index++] = 0;
        int f_pid;
        f_pid = fork();
        if (f_pid == -1) {
            string error_string = string("Couldn't fork process: ") +
                strerror(errno) + "\n";
            string_out(error_string, STDERR_FILENO);
            break;
        } else if (f_pid == 0) {
            // child process
            if (piping) {
                closeNotSTDFD(fd[0]);
                int ret_d = dup2(fd[1], STDOUT_FILENO);
                if (ret_d != STDOUT_FILENO) {
                    string error_string = string("Broken pipe: ") +
                        strerror(errno) + "\n";
                    string_out(error_string, STDERR_FILENO);
                    exit(1);
                }
            }
            if (send_piped_content) {
                send_piped_content = false;
                int ret_d = dup2(piped_fd, STDIN_FILENO);
                if (ret_d != STDIN_FILENO) {
                    string error_string = string("Broken pipe: ") +
                        strerror(errno) + "\n";
                    string_out(error_string, STDERR_FILENO);
                    exit(1);
                }
            }

            /**
             * this shouldn't overlap with the previous
             * if statements via logic elsewhere
             */
            if (dup_fd_stdin) {
                if (dupFD(redirect_fd_stdin, STDIN_FILENO) == 1) {
                    exit(1);
                }
            }
            if (dup_fd_stdout) {
                if (dupFD(redirect_fd_stdout, STDOUT_FILENO) == 1) {
                    exit(1);
                }
            }
            if (dup_fd_stderr) {
                if (dupFD(redirect_fd_stderr, STDERR_FILENO) == 1) {
                    exit(1);
                }
            }


            int ret;
            /* special build-in commands */
            if (current_command.first == "history") {
                ret = command_history(current_command);
            } else if (current_command.first == "pwd") {
                ret = command_pwd(current_command);
            } else if (current_command.first == "ls") {
                ret = command_ls(current_command);
            } else { // everything else
                execvp(args[0], &args[0]);
                string error_string = current_command.first + ": " +
                    strerror(errno) + "\n";
                string_out(error_string, STDERR_FILENO);
                exit(1);
            }
            exit(ret);
        } else {
            // parent process
            sh.child_pid = f_pid;
            if (piping) {
                closeNotSTDFD(fd[1]);
            }
            int r_pid = wait(&f_pid);
            if (piped_fd != 0) {
                closeNotSTDFD(piped_fd);
            }
            if (r_pid != sh.child_pid) {
                // child didn't exec correctly
                sh.child_pid = 0;
                break;
            }
            sh.child_pid = 0;
            if (piping) {
                piped_fd = fd[0];
                send_piped_content = true;
            } else {
                closeNotSTDFD(fd[0]);
            }
        }
    }
    return 0;
}


/* autocompletes by looking in current dir */
void autocomplete()
{
    // we will trim buffer to last word
    string search = "";
    if (sh.autocomplete_index == 0) {
        size_t last = sh.buffer.find_last_of(" ");
        if (last == string::npos) {
            search = sh.buffer;
            sh.original_auto_buffer_begin = "";
        } else {
            search = sh.buffer.substr(last + 1, sh.buffer.size());
            sh.original_auto_buffer_begin = sh.buffer.substr(0, last + 1);
        }
        sh.original_auto_search = search;
        if (search == "") {
            string_out("\n");
            cmdArgs empty;
            command_ls(empty);
            clearCurrentAndWriteNewBuffer(sh.buffer,
                                          sh.buffer,
                                          sh.line_prefix);
            return;
        }
    }

    search = sh.original_auto_search;

    struct dirent **entries;
    int count = scandir(sh.current_dir.c_str(), &entries, NULL, alphasort);
    // scandir filter sucks so I'll write my own...
    vector<string> cmp_entries;
    if (count != -1) {
        for (size_t i = 0; i < count; i++) {
            string temp = entries[i]->d_name;
            free(entries[i]);
            if (temp.find(search) == 0) {
                cmp_entries.push_back(temp);
            }
        }
        // I could write an implementation of longest common substring but nah
        // instead we'll just cycle through possible entries
        if (!cmp_entries.empty()) {
            if (cmp_entries.size() == 1) {
                if (cmp_entries[0] == search) {
                    string old = sh.buffer;
                    sh.buffer = sh.buffer + " ";
                    clearCurrentAndWriteNewBuffer(old,
                                                  sh.buffer,
                                                  sh.line_prefix);
                    return;
                } else {
                    string old = sh.buffer;
                    sh.buffer = sh.original_auto_buffer_begin + cmp_entries[0];
                    clearCurrentAndWriteNewBuffer(old,
                                                  sh.buffer,
                                                  sh.line_prefix);
                }

            } else {
                string old = sh.buffer;
                if (sh.autocomplete_index - 1 > cmp_entries.size() - 1) {
                    sh.autocomplete_index = 1;
                }
                sh.buffer = sh.original_auto_buffer_begin +
                    cmp_entries[sh.autocomplete_index++ - 1];
                clearCurrentAndWriteNewBuffer(old,
                                              sh.buffer,
                                              sh.line_prefix);
            }
            return;
        }
    }
    ring_bell();
}


/* searches history for similar command*/
void backsearch()
{
    bool scrollBack = true;
    string history_item = "";
    if (sh.backsearch_index == 0) {
        scrollBack = false;
        sh.backsearch_index = 1;
    }

    vector<string> results;
    if (!sh.history.empty()) {
        for (int i = sh.history.size() - 1; i >= 0; i--) {
            if (sh.history[i].find(sh.buffer) != string::npos) {
                results.push_back(sh.history[i]);
            }
        }
    }

    string back_prefix = "";
    if (results.empty()) {
        back_prefix = sh.backsearch_prefix_fail;
        sh.backsearch_index = 1;
        history_item = "";
    } else {
        back_prefix = sh.backsearch_prefix;
        if (sh.backsearch_index > results.size()) {
            sh.backsearch_index = 1;
        }
        history_item = results[sh.backsearch_index++ - 1];
    }

    if (sh.backsearch_index > sh.history_limit) {
        sh.backsearch_index = 1;
    }
    if (scrollBack) {
        string_out("\033[F");
    }
    clearCurrentAndWriteNewBuffer(sh.buffer + sh.backsearch_query,
                                  history_item,
                                  sh.line_prefix);
    string_out("\n");
    clearCurrentAndWriteNewBuffer(sh.backsearch_prefix +
                                      sh.backsearch_prefix_fail,
                                  sh.buffer,
                                  back_prefix);

    sh.backsearch_query = history_item;

}


/* redirect sig to child process if it exists */
void sig_handler(int sig)
{
    string old = sh.buffer;
    sh.buffer = "";
    if (sh.child_pid != 0) {
        kill(sh.child_pid, sig);
        string_out("^C\n");
        return;
    }
    string_out("\n");
    clearCurrentAndWriteNewBuffer(old, sh.buffer, sh.line_prefix);
    return;
}




int main(int argc, char *argv[])
{
    struct termios SavedTermAttributes;
    SetNonCanonicalMode(STDIN_FILENO, &SavedTermAttributes);

    // KLShell sh; // now global
    signal(SIGINT, sig_handler);
    sh.pid = getpid();
    sh.current_dir = getcwd(NULL, 0);
    updatePrefix();

    char RXChar;
    bool line_begin = false;
    uint8_t combo_key = 0;
    while (true) {
        if (line_begin == false) {
            write(STDOUT_FILENO,
                sh.line_prefix.c_str(),
                sh.line_prefix.size());
            line_begin = true;
        }
        ssize_t bytes_read = read(STDIN_FILENO, &RXChar, 1);
        if (bytes_read != 1) {
            string error_string = string("Reading input failed with: ") +
                strerror(errno) + "\n";
            string_out(error_string, STDERR_FILENO);
            return EXIT_FAILURE;
        }

        // turn off autocomplete cycling if we're not pushing tab
        if (RXChar != 0x09) {
            sh.autocomplete_index = 0;
        }

        if (RXChar == 0x04) {
            string_out("\nShell closed.\n");
            break;
        } else if (RXChar == 0x7F) {
            // backspace
            if (sh.buffer.size() > 0) {
                if (sh.buffer_right == "") {
                    sh.buffer_left = sh.buffer;
                }
                if (sh.buffer_left.size() > 0) {
                    string old = sh.buffer;
                    sh.buffer_left.erase(sh.buffer_left.size() - 1);
                    sh.buffer = sh.buffer_left + sh.buffer_right;
                    clearCurrentAndWriteNewBuffer(
                        sh.backsearch_prefix + sh.backsearch_prefix_fail + old,
                        sh.buffer,
                        sh.line_prefix,
                        sh.buffer_right.size());
                } else {
                    ring_bell();
                }
                if (sh.backsearch_index != 0) {
                    sh.backsearch_index = 1;
                    backsearch();
                }
            } else {
                // otherwise ring the bell
                ring_bell();
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
            sh.backsearch_index = 0;
            scrollUpDown();

        } else if (RXChar == 0x42 && combo_key == 2) {
            // down arrow, this is the 'B' of the combination key
            combo_key = 0;
            sh.backsearch_index = 0;
            scrollUpDown(false);

        } else if (RXChar == 0x44 && combo_key == 2) {
            // left arrow, this is the 'D' of the combination key
            combo_key = 0;
            if (sh.backsearch_index != 0) {
                sh.backsearch_index = 0;
                string_out("\n");
                clearCurrentAndWriteNewBuffer(sh.backsearch_prefix +
                                                  sh.backsearch_prefix_fail +
                                                  sh.buffer,
                                              "",
                                              "");
                string_out("\033[F");
                clearCurrentAndWriteNewBuffer(sh.buffer,
                                              sh.buffer,
                                              sh.line_prefix);
                continue;
            }
            scrollLeftRight();

        } else if (RXChar == 0x43 && combo_key == 2) {
            // right arrow, this is the 'C' of the combination key
            combo_key = 0;
            if (sh.backsearch_index != 0) {
                sh.backsearch_index = 0;
                string_out("\n");
                clearCurrentAndWriteNewBuffer(sh.backsearch_prefix +
                                                  sh.backsearch_prefix_fail +
                                                  sh.buffer,
                                              "",
                                              "");
                string_out("\033[F");
                clearCurrentAndWriteNewBuffer(sh.buffer,
                                              sh.buffer,
                                              sh.line_prefix);
                continue;
            }
            scrollLeftRight(false);

        } else if (RXChar == 0x09) {
            // tab (autocomplete)
            combo_key = 0;
            if (sh.backsearch_index != 0) {
                sh.backsearch_index = 0;
                string_out("\n");
                clearCurrentAndWriteNewBuffer(sh.backsearch_prefix +
                                                  sh.backsearch_prefix_fail +
                                                  sh.buffer,
                                              "",
                                              "");
                string_out("\033[F");
                clearCurrentAndWriteNewBuffer(sh.buffer,
                                              sh.buffer,
                                              sh.line_prefix);
                continue;
            }
            autocomplete();

        } else if (RXChar == 0x12) {
            // Ctrl + R (back search)
            combo_key = 0;
            backsearch();

        } else if (combo_key == 0) {
            if (RXChar == 0x0a) {
                if (sh.backsearch_index != 0) {
                    if (sh.backsearch_query != "") {
                        sh.buffer = sh.backsearch_query;
                        sh.backsearch_index = 0;
                        string_out("\033[F");
                    } else {
                        sh.backsearch_index = 0;
                        string_out("\033[F");
                        clearCurrentAndWriteNewBuffer(sh.buffer,
                                                      sh.buffer,
                                                      sh.line_prefix);
                    }
                }
                if (sh.buffer_right != "") {
                    clearCurrentAndWriteNewBuffer(sh.buffer,
                                                  sh.buffer,
                                                  sh.line_prefix);
                }
                write(STDOUT_FILENO, &RXChar, 1);
                // delete the empty entry in history if we scrolled to input
                if (!sh.history.empty()) {
                    if (trimEdges(sh.history.back()) == "") {
                        sh.history.pop_back();
                    }
                }
                // if we are at an entry from history move that to the front
                if (sh.history_index != sh.history.size() &&
                        sh.buffer == sh.history[sh.history_index]) {
                    sh.history.erase(sh.history.begin() + sh.history_index);
                }

                // delete any identical history entries
                for (size_t i = 0; i < sh.history.size(); i++) {
                    if (sh.history[i] == sh.buffer) {
                        sh.history.erase(sh.history.begin() + i);
                        sh.history_index--;
                    }
                }

                // parse input
                if (trimEdges(sh.buffer) != "") {
                    sh.history.push_back(sh.buffer);
                    if (sh.history.size() > sh.history_limit) {
                        sh.history.erase(sh.history.begin());
                    }
                    int ret = runCommand(trimEdges(sh.buffer));
                    if (ret == 1) {
                        break;
                    }
                    sh.history_index = sh.history.size();
                }
                line_begin = false;
                sh.buffer = "";
                sh.buffer_left = "";
                sh.buffer_right = "";
            } else {
                if (sh.backsearch_index != 0) {
                    sh.buffer += RXChar;
                    sh.backsearch_index = 1;
                    backsearch();
                } else if (sh.buffer_right != "") {
                    string old = sh.buffer;
                    sh.buffer_left += RXChar;
                    sh.buffer = sh.buffer_left + sh.buffer_right;
                    clearCurrentAndWriteNewBuffer(old,
                                                  sh.buffer,
                                                  sh.line_prefix,
                                                  sh.buffer_right.size());
                } else {
                    write(STDOUT_FILENO, &RXChar, 1);
                    sh.buffer += RXChar;
                }
            }
        } else {
            // this disables other combo keys such as left/right arrow
            combo_key = 0;
        }
    }
    ResetCanonicalMode(STDIN_FILENO, &SavedTermAttributes);
    return EXIT_SUCCESS;
}

