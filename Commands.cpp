//
// Created by oreno on 29-Nov-21.
//

#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"

using namespace std;
static const string & WHITESPACE = " ";

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

string _ltrim(const std::string& s)
{
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s)
{
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s)
{
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char* cmd_line, char** args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for(std::string s; iss >> s; ) {
        args[i] = (char*)malloc(s.length()+1);
        memset(args[i], 0, s.length()+1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;

    FUNC_EXIT()
}

bool _isBackgroundComamnd(const char* cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char* cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

// TODO: Add your implementation for classes in Commands.h

SmallShell::SmallShell() {
// TODO: add your implementation
    shell_name = new char[6];
    strcpy(shell_name,"smash");
    lastPwd = nullptr;
}

SmallShell::~SmallShell() {
// TODO: add your implementation
    delete[] shell_name;
    if(lastPwd != nullptr){
        delete[] lastPwd;
    }
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command * SmallShell::CreateCommand(const char* cmd_line) {
    // For example:

  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

  if (firstWord.compare("chprompt") == 0) {
    return new ChangePromptCommand(cmd_line, this);
  }
  else if (firstWord.compare("cd") == 0) {
    return new ChangeDirCommand(cmd_line,this);
  }
//  else if ...
//  .....
//  else {
//    return new ExternalCommand(cmd_line);
//  }
    return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
    // TODO: Add your implementation here
    // for example:
     Command* cmd = CreateCommand(cmd_line);
    if (cmd != nullptr){
        cmd->execute();
    }
    // Please note that you must fork smash process for some commands (e.g., external commands....)
}

void SmallShell::changeShellName(const char *new_name) {
    delete[] shell_name;
    string new_name_s = _trim(string(new_name));
    int n = new_name_s.size();
    shell_name = new char[n];
    strcpy(shell_name,new_name);

}

char *SmallShell::getShellName() {
    return shell_name;
}

char **SmallShell::getLastPwd() {
    return lastPwd == nullptr? nullptr: &lastPwd;
}

void SmallShell::changeLastPwd(const char *new_last_pwd) {
    //cd was not used
    string new_pwd_s = _trim(string(new_last_pwd));
    int n = new_pwd_s.size();
    if (lastPwd == nullptr){
        lastPwd = new char[n];
        strcpy(lastPwd,new_last_pwd);

    }
    else{
        delete[] lastPwd;
        lastPwd = new char[n];
        strcpy(lastPwd,new_last_pwd);
    }
}

Command::Command(const char *cmd_line) {
    num_args = _parseCommandLine(cmd_line, args);
}

BuiltInCommand::BuiltInCommand(const char *cmd_line) : Command(cmd_line) {

}



ChangePromptCommand::ChangePromptCommand(const char *cmd_line, SmallShell *cur_shell)
        : BuiltInCommand(cmd_line){
    shell = cur_shell;

}

ChangePromptCommand::~ChangePromptCommand() {

}

void ChangePromptCommand::execute() {
    if (num_args == 1) {
        if (strcmp(SmallShell::getInstance().getShellName(), "smash") != 0) {
            SmallShell::getInstance().changeShellName("smash");
        }
    } else if (num_args == 2) {
        SmallShell::getInstance().changeShellName(args[1]);
    }
}



ChangeDirCommand::ChangeDirCommand(const char *cmd_line, SmallShell *pshell)
        : BuiltInCommand(cmd_line){
    shell = pshell;
}

ChangeDirCommand::~ChangeDirCommand() {

}

void ChangeDirCommand::execute() {

    char buf[COMMAND_ARGS_MAX_LENGTH];
    getcwd(buf,COMMAND_ARGS_MAX_LENGTH);
    if (!buf) {
        perror("smash error: getcwd failed");
    }

    if (num_args == 2){
        // go to last directory
        if (strcmp(args[1],"-") == 0){
            //cd was not used
            if (SmallShell::getInstance().getLastPwd() == nullptr){
                perror("smash error: cd: OLDPWD not set");
            } else {
                //cd was used
                if (chdir(*SmallShell::getInstance().getLastPwd()) < 0){
                    perror("smash error: chdir failed");
                } else{
                    SmallShell::getInstance().changeLastPwd(buf);
                }
            }
        }else{
            //save old directory
            //regular change
            SmallShell::getInstance().changeLastPwd(buf);
            if (chdir(args[1]) < 0){
                //reset last pd for failure at the moment because I save it before the change
                perror("smash error: chdir failed");
                SmallShell::getInstance().changeLastPwd(nullptr);
            }
        }

    }else if (num_args > 2){
        perror("smash error: cd: too many arguments");
    }
}
