#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";
const size_t MAX_COMMAND_LENGTH = 255;

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

//====================Commands Implementation===========================
Command::Command(const char* cmd_line) : cmd_line(cmd_line) {
    num_args = _parseCommandLine(cmd_line, args);
}

Command::~Command() {}

//====================BuiltIn Commands Implementation===================
BuiltInCommand::BuiltInCommand(const char* cmd_line) 
	: Command(cmd_line) {
    //see piazza @49_
    _removeBackgroundSign(args[0]);
}

ShowPidCommand::ShowPidCommand(const char* cmd_line) 
										: BuiltInCommand(cmd_line) {} 

GetCurrDirCommand::GetCurrDirCommand(const char* cmd_line) 
										: BuiltInCommand(cmd_line) {} 


ChangePromptCommand::ChangePromptCommand(const char* cmd_line, SmallShell *cur_shell)
		: BuiltInCommand(cmd_line) {
    shell = cur_shell;
}

void ChangePromptCommand::execute() {
    if (num_args == 1) {
        if (SmallShell::getInstance().getPrompt() != "smash") {
            SmallShell::getInstance().setNewPrompt("smash");
        }
    } else if (num_args == 2) {
        SmallShell::getInstance().setNewPrompt(args[1]);
    }
}

void GetCurrDirCommand::execute() {
    char buffer[MAX_COMMAND_LENGTH];
    if(getcwd(buffer,sizeof(buffer))!= NULL) {
         std::cout<< buffer << std::endl;
      } else{
        perror("smash error: getcwd failed");
    }
}

void ShowPidCommand::execute() {
    int pid = getpid();
    if (pid < 0){
        perror("smash error: getpid failed");
    } else{
        std::cout<< "smash pid is "<< getpid() << std::endl;
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
            if (SmallShell::getInstance().getLastPwd().empty()){
                perror("smash error: cd: OLDPWD not set");
            } else {
                //cd was used
                if (chdir(SmallShell::getInstance().getLastPwd().c_str()) < 0){
                    perror("smash error: chdir failed");
                } else{
                    SmallShell::getInstance().setLastPwd(buf);
                }
            }
        }else{
            //save old directory
            //regular change
            SmallShell::getInstance().setLastPwd(buf);
            if (chdir(args[1]) < 0){
                //reset last pd for failure at the moment because I save it before the change
                perror("smash error: chdir failed");
                SmallShell::getInstance().setLastPwd("");
            }
        }

    }else if (num_args > 2){
        perror("smash error: cd: too many arguments");
    }
}

//=====================External Commands Implementation=================

ExternalCommand::ExternalCommand(const char* cmd_line) 
												: Command(cmd_line) {}
												
void ExternalCommand::execute() {
	
	char* arg = (char*)malloc((sizeof(cmd_line)+1)/sizeof(char));
	strcpy(arg, cmd_line);
	
	char* const paramlist[] = {"bash", "-c", arg, NULL};
	
	int pid = fork();
	
	if(pid == -1) {
		perror("smash error: Failed forking a child command\n");
	}
	
	if(pid == 0) {
		setpgrp();
		execvp("/bin/bash", paramlist);
		
		perror("smash error: executioin failed\n");
		kill(getpid(), SIGKILL);
	}
	else {
		waitpid(pid, NULL, WUNTRACED); //WUNTRACED in case the child gets stopped.
	}
	
	free(arg);
}										

//===========================SmallShell=================================

SmallShell::SmallShell() : prompt("smash"), lastPwd("") {}

SmallShell::~SmallShell() {
// TODO: add your implementation
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/

Command * SmallShell::CreateCommand(const char* cmd_line) {
	
	string cmd_s = _trim(string(cmd_line));
	string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

	if (firstWord.compare("chprompt") == 0) {
        return new ChangePromptCommand(cmd_line, this);
	}
	if (firstWord.compare("showpid") == 0) {
        return new ShowPidCommand(cmd_line);
	}
	else if (firstWord.compare("pwd") == 0) {
        return new GetCurrDirCommand(cmd_line);
	}
    else if (firstWord.compare("cd") == 0) {
        return new ChangeDirCommand(cmd_line, this);
  }
  //else if ...
  //.....
  else {
    return new ExternalCommand(cmd_line);
  }
  
  return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
	
	Command* cmd = CreateCommand(cmd_line);
	
	if(cmd != nullptr) {
        cmd->execute();
	}
	//Please note that you must fork smash process for some commands (e.g., external commands....)
}

string SmallShell::getPrompt() {
	return prompt;
}

void SmallShell::setNewPrompt(string new_prompt) {
	prompt = new_prompt;
}

string SmallShell::getLastPwd() {
    return lastPwd;
}

void SmallShell::setLastPwd(string new_last_pwd) {
    lastPwd = new_last_pwd;
}
