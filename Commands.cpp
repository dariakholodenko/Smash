#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <set>
#include <time.h>
#include <errno.h>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include <fcntl.h>
#include "Commands.h"

using namespace std;
set<string> built_in_commands {"chprompt", "showpid", "pwd" ,"cd", "jobs",
                                  "kill", "fg", "bg", "quit"};

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
  // if the cmd_line line does not end with & then return
  if (cmd_line[idx] != '&') {
    return;
  }
  // replace the & (background sign) with space and then remove all tailing spaces.
  cmd_line[idx] = ' ';
  // truncate the cmd_line line string up to the last non-space character
  cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

//====================Commands Implementation===========================
Command::Command(const char* cmd_line) : cmd_line(cmd_line) {
    num_args = _parseCommandLine(cmd_line, args);
}

Command::~Command() {
    for (int i = 0; i < num_args; ++i) {
        free(args[i]);
    }
}

//====================BuiltIn Commands Implementation===================
BuiltInCommand::BuiltInCommand(const char* cmd_line) 
	: Command(cmd_line) {
    //see piazza @49_
    for (int i = 0; i < num_args; ++i) {
        _removeBackgroundSign(args[i]);
    }
}

ShowPidCommand::ShowPidCommand(const char* cmd_line) 
										: BuiltInCommand(cmd_line) {}

void ShowPidCommand::execute() {
    SmallShell& shell = SmallShell::getInstance();
    std::cout<< "smash pid is "<< shell.getPid() << std::endl;
    int pid = getpid();
    if (pid < 0){
        perror("smash error: getpid failed");
    } else{

	}
}

GetCurrDirCommand::GetCurrDirCommand(const char* cmd_line) 
										: BuiltInCommand(cmd_line) {} 

void GetCurrDirCommand::execute() {
    char buffer[MAX_COMMAND_LENGTH];
    if(getcwd(buffer,sizeof(buffer))!= NULL) {
         std::cout<< buffer << std::endl;
      } else{
        perror("smash error: getcwd failed");
    }
}

ChangePromptCommand::ChangePromptCommand(const char* cmd_line, SmallShell *cur_shell)
		: BuiltInCommand(cmd_line) {
    shell = cur_shell;
}

void ChangePromptCommand::execute() {
    if (num_args == 1) {
        if (shell->getPrompt() != "smash") {
            shell->setNewPrompt("smash");
        }
    } else if (num_args == 2) {
        shell->setNewPrompt(args[1]);
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
        if (strcmp(args[1],"-") == 0){
            // go to last directory left with cd
            //cd was not used
            if (shell->getLastPwd().empty()){
                perror("smash error: cd: OLDPWD not set");
            } else {
                //cd was used
                if (chdir(shell->getLastPwd().c_str()) < 0){
                    perror("smash error: chdir failed");
                } else{
                    shell->setLastPwd(buf);
                }
            }
        } else if (strcmp(args[1],"..") == 0){
            //go back to father
            if (chdir("..") < 0){
                perror("smash error: chdir failed");
            } else{
                shell->setLastPwd(buf);
            }
        }
        else{
            //regular change
            shell->setLastPwd(buf);
            if (chdir(args[1]) < 0){
                //reset last pd for failure at the moment because I save it before the change
                perror("smash error: chdir failed");
                shell->setLastPwd("");
            }
        }

    }else if (num_args > 2){
			perror("smash error: cd: too many arguments");
		}
}

//=====================External Commands Implementation=================

ExternalCommand::ExternalCommand(const char *cmd_line, 
	SmallShell* shell, JobsList *jobs): Command(cmd_line), shell(shell) {
    jl = jobs;
}
												
void ExternalCommand::execute() {

	char* arg = (char*)malloc((sizeof(cmd_line)+1)/sizeof(char));
	strcpy(arg, cmd_line);
	bool isBG = _isBackgroundComamnd(cmd_line);
	 _removeBackgroundSign(arg);
	
	char* const paramlist[] = {"bash", "-c", arg, NULL};
	int pid = fork();
	
	int tempjobId = -1;
	
	if(pid == -1) {
		perror("smash error: Failed forking a child cmd_line\n");
	}
	
	if(pid == 0) {
        if (setpgrp() == -1){
            perror("smash error: setpgrp failed");
        }
        int cpid = getpid();
        if (cpid < 0){
            perror("smash error: getpid failed");
        }

		setpgrp();
		execvp("/bin/bash", paramlist);
		
		perror("smash error: execution failed\n");
		if (kill(cpid, SIGKILL) < 0){
            perror("smash error: kill failed");
        }
	}
	else {
		if(isBG) {
			tempjobId = jl->addJob(cmd_line, pid, 0);
			//cout << "job id = " << tempjobId << endl;
			//jl->printJobsList();
			
			if(tempjobId == -1) {
				perror("smash error: coudn't add a job to list");
			}
		
//			if(waitpid(pid, NULL, WNOHANG) < 0) {
//				perror("smash error: waitpid failed");
//			}
//            return;
			//else {
			//	if(tempjobId != -1) {
			//	//	jl->removeJobById(tempjobId, cmd_line);
			//	}
			//}			
			
		}
		else {
			shell->setCurrentFGCmd(arg, pid);
			
			//WUNTRACED in case the child gets stopped.
			if(waitpid(pid, NULL, WUNTRACED) < 0){
				perror("smash error: waitpid failed");
			}
			else {
				shell->setCurrentFGCmd(nullptr, -1);
			}
		}
	}
	
	free(arg);
	arg = NULL;
}											
							
//===========================Built-in Implementation=================================

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

//===========================Jobs List Implementation=================================
JobsList::JobsList() {
    //init
    for (int i = 0; i < MAX_COMMANDS + 1; ++i) {
        jobs_list[i].jobId = 0;
        jobs_list[i].cmd_line = "";
        jobs_list[i].pid = 0;
        jobs_list[i].startTime = 0;
        jobs_list[i].isStopped = false;
    }
    num_entries = 0;
}

JobsList::~JobsList() {

}

int JobsList::addJob(const char *cmd, int pid, bool isStopped) {
    if (num_entries == MAX_COMMANDS) perror("smash error: job list is full");

    for (int i = 1; i < MAX_COMMANDS + 1; ++i) {
        //find first empty space
        if (jobs_list[i].jobId == 0){
            jobs_list[i].jobId = i;
            jobs_list[i].cmd_line = cmd;
            jobs_list[i].pid = pid;
            jobs_list[i].startTime = time(nullptr);
            jobs_list[i].isStopped = isStopped;
            num_entries++;
            return i;
        }
    }
    return -1;
}

static void printIdErrorMessage(int jobId, string commandType) {
    string message = "smash error: ";
    string str = ": job-id ";
    string  sjobid = to_string(jobId);
    string  end = " does not exist";
    message += commandType += str  += sjobid += end;
    perror(message.c_str());
}

JobsList::JobEntry *JobsList::getJobById(int jobId) {
    if (jobId < 1 || num_entries == 0) {
        return nullptr;
    }
    if (jobs_list[jobId].jobId == 0){
        return nullptr;
    } else{
        return &jobs_list[jobId];
    }
}

void JobsList::removeJobById(int jobId, string commandType) {
    if (jobs_list[jobId].jobId == 0){
        printIdErrorMessage(jobId, commandType);

    } else{
        //Reset
        jobs_list[jobId].jobId = 0;
        jobs_list[jobId].cmd_line = "";
        jobs_list[jobId].pid = 0;
        jobs_list[jobId].startTime = 0;
        jobs_list[jobId].isStopped = false;
        num_entries--;
    }
}

void JobsList::printJobsList() {
    //empty
    if (num_entries == 0) return;
    for (int i = 0; i < MAX_COMMANDS + 1; ++i) {
        if (jobs_list[i].jobId != 0){
            cout << &jobs_list[i];
        }
    }
}

ostream &operator<<(ostream &out, const JobsList::JobEntry *je) {
    out << "[" << je->jobId << "]" << " " << je->cmd_line << ": "
        << je->pid << " " << difftime(time(nullptr), je->startTime);
    if (je->isStopped){
        out << " (stopped)";
    }
    out << endl;
    return out;
}

JobsList::JobEntry *JobsList::getLastJob(int *lastJobId) {
    for (int i = MAX_COMMANDS; i > 0 ; --i) {
        if (jobs_list[i].jobId != 0){
            *lastJobId = i;
            return &jobs_list[i];
        }
    }
    return nullptr;
}

/*int JobsList::getLastJobPid() {
    for (int i = MAX_COMMANDS; i > 0 ; --i) {
        if (jobs_list[i].jobId != 0){
            return jobs_list[i].pid;
        }
    }
    return -1;
}*/

JobsList::JobEntry *JobsList::getLastStoppedJob(int *jobId) {
    for (int i = MAX_COMMANDS; i > 0 ; --i) {
        if (jobs_list[i].jobId != 0 && jobs_list->isStopped){
            *jobId = i;
            return &jobs_list[i];
        }
    }
    return nullptr;
}

void JobsList::killAllJobs() {
    for (int i = 0; i < MAX_COMMANDS + 1; ++i) {
        if (jobs_list[i].jobId != 0){
            cout << jobs_list[i].pid << ": "
            << jobs_list[i].cmd_line;
            if (kill(jobs_list[i].pid, SIGKILL) < 1){
                perror("smash error: kill failed");
            }
        }
    }
}

void JobsList::removeFinishedJobs() {
    int wstatus;
    for (int i = 0; i <  MAX_COMMANDS + 1 ; ++i) {
        if (jobs_list[i].jobId != 0){
            //test signal to see pid is still running in background
            int wpid = waitpid(jobs_list[i].pid, &wstatus , WNOHANG);
//            cout <<"pid: " << wpid << " status: " << WIFEXITED(wstatus) << endl ;
            if (wpid == 0) continue;
            if ((wpid == jobs_list[i].pid && WIFEXITED(wstatus))
            || (wpid == -1 && errno == ESRCH)){
                perror("smash error: waitpid failed");
                removeJobById(i,"");
            }
//            if (waitpid(jobs_list[i].jobId, nullptr, WNOHANG) < 0){
//                perror("smash error: waitpid failed");
//                return;
//            }
//            int status = kill(jobs_list[i].pid, 0);
//            if (status == -1 && errno == ESRCH){
//                //this means pid is no longer running
//                removeJobById(i,"");
//            }
        }
    }

}

JobsCommand::JobsCommand(const char *cmd_line, JobsList *jobs):
        BuiltInCommand(cmd_line) {
    jl = jobs;
}

void JobsCommand::execute() {
    jl->removeFinishedJobs();
    jl->printJobsList();
}

//===========================Kill cmd_line Implementation=================================

KillCommand::KillCommand(const char *cmd_line, JobsList *jobs):
        BuiltInCommand(cmd_line){
    jl = jobs;
}

void KillCommand::execute() {
    //argument checks
    if (num_args != 3){
        perror("smash error: kill: invalid arguments");
        return;
    }
    string str_sig_num = args[1];
    int n =str_sig_num.length();
    //sig num should be -N or -NN where N-NN is 0-31
    if (n < 2 || n > 3){
        perror("smash error: kill: invalid arguments");
        return;
    }else{
        if (str_sig_num[0] != '-'){
            perror("smash error: kill: invalid arguments");
            return;
        } else{
            str_sig_num.erase(0,1);
        }

    }
    int jobId = stoi(args[2]);
    if (jobId < 1 || jobId > 100 ||
            stoi(str_sig_num) < 0 || stoi(str_sig_num) > 31){
        printIdErrorMessage(jobId, "kill");
        return;
    }
    //getJobById returns null if can't find job id for any reason
    JobsList::JobEntry* je = jl->getJobById(jobId);
    if (je == nullptr){
        printIdErrorMessage(jobId, "kill");
        return;
    }
    //send kill syscall and check for success
    if (kill(je->pid, stoi(str_sig_num)) < 0){
        perror("smash error: kill failed");
        return;
    } else{
        cout << "signal number " << str_sig_num
        << "was sent to pid " << je->pid << endl;
    }
}
//===========================Foreground and Background commands Implementation=================================

ForegroundCommand::ForegroundCommand(const char *cmd_line, JobsList *jobs)
    :BuiltInCommand(cmd_line) {
    jl = jobs;
}

//handle both fg and bg but with a bit of difference
// if numargs != 2 no need to send args_1
void FGBGAUX(int num_args, JobsList* jl,
             string cmd_line, string commandType, string args_1 = ""){
    JobsList::JobEntry* je = nullptr;
    string message = "smash error: ";
    string str = ": ";
    string  empty = " jobs list is empty";
    string  args = " invalid arguments";
    message += commandType += str;
    //bring job with maximal jobID to foreground
    if (num_args == 1){
        int  id = 0;
        if (commandType == "fg"){
            je = jl->getLastJob(&id);
        } else if (commandType == "bg"){
            je = jl->getLastStoppedJob(&id);
        }
        if (je == nullptr){
            perror((message += empty).c_str());
            return;
        }
    } else if (num_args == 2){
        //bring job with args[1] jobID to foreground
        je = jl->getJobById(stoi(args_1));
        if (je == nullptr){
            printIdErrorMessage(stoi(args_1), commandType);
            return;
        }
    } else{
        perror((message += args).c_str());
        return;
    }
    //assuming je is found
    cout << cmd_line << " : " << je->pid;
    if (kill(je->pid, SIGCONT) < 0){
        perror("smash error: kill failed");
        return;
    }
    if (commandType == "fg"){
        jl->removeJobById(je->jobId, "fg");
        //WUNTRACED in case the process gets stopped
        if (waitpid(je->pid, NULL, WUNTRACED) < 0){
            perror("smash error: waitpid failed");
        }
    } else if (commandType == "bg"){
        je->isStopped = false;
    }
}

void ForegroundCommand::execute() {
    if (num_args == 2){
        FGBGAUX(num_args, jl, cmd_line, "fg", args[1]);
    } else{
        FGBGAUX(num_args, jl, cmd_line, "fg");
    }
}

BackgroundCommand::BackgroundCommand(const char *cmd_line, JobsList *jobs)
    :BuiltInCommand(cmd_line){
    jl = jobs;
}

void BackgroundCommand::execute() {
    if (num_args == 2){
        FGBGAUX(num_args, jl, cmd_line, "bg", args[1]);
    } else{
        FGBGAUX(num_args, jl, cmd_line, "bg");
    }
}


//===========================Quit cmd_line Implementation=================================

QuitCommand::QuitCommand(const char *cmd_line, JobsList *jobs):
    BuiltInCommand(cmd_line) {
    jl = jobs;
}

void QuitCommand::execute() {
    jl->removeFinishedJobs();
    if (num_args > 1){
        //kill argument may be passed
        if (strcmp(args[1], "kill") == 0){
            cout << "smash: sending SIGKILL signal to " << jl->getNumEntries()
            << " jobs";
            jl->killAllJobs();
        }
    }
    //if kill was not sent
    exit(1);
}

//======================RedirectionCommand Implementation===============

RedirectionCommand::RedirectionCommand(const char* cmd_line
			, bool isAppend): Command(cmd_line), isAppend(isAppend) {
	
	if(strcmp(args[num_args], "&")) {
		strcpy(path, args[num_args-1]);
	}
	else {
		strcpy(path, args[num_args]);
	}
}

void RedirectionCommand::execute() {
	int file;

	if(isAppend == true) {
		file = open(path, O_WRONLY | O_CREAT | O_APPEND, 0666);
	}
	else {
		file = open(path, O_WRONLY | O_CREAT , 0666);
	}

	if(file == -1) {
		perror("smash: open failed");
	}

	int fd = dup2(STDOUT_FILENO, file);

	if(fd == -1) {
		perror("smash: dup2 failed");
	}

	//inner command execution - Under constraction

	if(close(file) == -1) {
		perror("smash: close failed");
	}

	if(close(fd) == -1) {
		perror("smash: close failed");
	}
}

//======================PipeCommand Implementation===============

PipeCommand::PipeCommand(const char *cmd_line, SmallShell* shell,
                         bool isError, int pos) :
    Command(cmd_line) , isError(isError) {
    cur_shell = shell;
    string s = cmd_line;
    cmd_1 = s.substr(0, pos);
    cmd_2 = s.substr(pos+1,string::npos);
}

void PipeCommand::execute() {

    cmd_1 = _trim(string(cmd_1));
    cmd_2 = _trim(string(cmd_2));
    Command* cmd1 = nullptr;
    Command* cmd2 = nullptr;
    string firstWordCmd1 = cmd_1.substr(0, cmd_1.find_first_of(" \n"));
    string firstWordCmd2 = cmd_2.substr(0, cmd_2.find_first_of(" \n"));
    if (built_in_commands.find(firstWordCmd1) != built_in_commands.end()){
        cmd1 = cur_shell->CreateCommand(cmd_1.c_str());
    }
    if (built_in_commands.find(firstWordCmd2) != built_in_commands.end()){
        cmd2 = cur_shell->CreateCommand(cmd_2.c_str());
    }

    int fd[2];
    if (pipe(fd) < 0){
        perror("smash error: pipe failed");
        return;
    }
    int child_1 = fork();
    if (child_1 < 0){
        perror("smash error: fork failed");
        return;
    }
    if (child_1 == 0) {
        if(setpgrp() == -1){
            perror("smash error: setpgrp failed");
            return;
        }
        // first child
        if (dup2(fd[1],1) < 0){
            perror("smash error: dup2 failed");
            return;
        }
        if (close(fd[0]) < 0){
            perror("smash error: close failed");
            return;
        }
        if (close(fd[1]) < 0){
            perror("smash error: close failed");
            return;
        }
        //cmd 1 is not built in
        if(cmd1 == nullptr) {
            char* const paramlist_1[] = {"bash", "-c",
                                         (char* const)cmd_1.c_str(), NULL};
            if (execv("/bin/bash", paramlist_1) == -1){
                perror("smash error: execv failed");
            }

        }else{
            //cmd 1 is built in
            cmd1->execute();
            exit(0);
        }
    }
    int child_2 = fork();
    if (child_2 < 0){
        perror("smash error: fork failed");
        return;
    }
    if (child_2 == 0) {
        if(setpgrp() == -1){
            perror("smash error: setpgrp failed");
            return;
        }
        // second child
        if (dup2(fd[0],0) < 0){
            perror("smash error: dup2 failed");
            return;
        }
        if (close(fd[0]) < 0){
            perror("smash error: close failed");
            return;
        }

        if (close(fd[1]) < 0){
            perror("smash error: close failed");
            return;
        }
        //cmd 2 is not built in
        if(cmd2 == nullptr) {
            char* const paramlist_2[] = {"bash", "-c",
                                         (char* const)cmd_2.c_str(), NULL};
            if (execv("/bin/bash", paramlist_2) == -1){
                perror("smash error: execv failed");
            }

        }else{
            //cmd 2 is built in
            cmd2->execute();
            exit(0);
        }
    }
    else{
        if (close(fd[0]) < 0){
            perror("smash error: close failed");
            return;
        }
        if (close(fd[1]) < 0){
            perror("smash error: close failed");
            return;
        }
        if (waitpid(child_1, nullptr,0) == -1){
            perror("smash error: waitpid failed");
        }
        if (waitpid(child_2, nullptr,0) == -1){
            perror("smash error: waitpid failed");
        }
        if (cmd1 != nullptr){
            delete cmd1;
            cmd1 = nullptr;
        }
        if (cmd2 != nullptr){
            delete cmd2;
            cmd2 = nullptr;
        }
    }

}

//===========================SmallShell=================================

SmallShell::SmallShell()
		: current_fg_cmd(nullptr), current_fg_cmd_pid(-1),
										prompt("smash"), lastPwd("") {
    jobsList = new JobsList();
    pid = getpid();
    if (pid < 0){
        perror("smash error: getpid() failed");
    }
}

SmallShell::~SmallShell() {
	delete jobsList;
	jobsList = nullptr;
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/

Command * SmallShell::CreateCommand(const char* cmd_line) {

	string cmd_s = _trim(string(cmd_line));
	string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

	size_t idx = ((string)cmd_line).find_first_of('>');
    size_t idy = cmd_s.find_first_of('|');

	if(idx != std::string::npos) {
		bool isAppend = false;
		if(cmd_line[idx+1] == '>') {
			isAppend = true;
		}
		return new RedirectionCommand(cmd_line, isAppend);
	}
    if(idy != std::string::npos) {
        bool isError = false;
        if(cmd_line[idy+1] == '&') {
            isError = true;
        }
        return new PipeCommand(cmd_line, this, isError, idy);
    }
	else if (firstWord.compare("chprompt") == 0) {
        return new ChangePromptCommand(cmd_line, this);
	}
	else if (firstWord.compare("showpid") == 0) {
        return new ShowPidCommand(cmd_line);
	}
	else if (firstWord.compare("pwd") == 0) {
        return new GetCurrDirCommand(cmd_line);
	}
    else if (firstWord.compare("cd") == 0) {
        return new ChangeDirCommand(cmd_line, this);
    }
    else if (firstWord.compare("jobs") == 0) {
        return new JobsCommand(cmd_line, jobsList);
    }
    else if (firstWord.compare("kill") == 0) {
        return new KillCommand(cmd_line, jobsList);
    }
    else if (firstWord.compare("fg") == 0) {
        return new ForegroundCommand(cmd_line, jobsList);
    }
    else if (firstWord.compare("bg") == 0) {
        return new BackgroundCommand(cmd_line, jobsList);
    }
    else if (firstWord.compare("quit") == 0) {
        return new QuitCommand(cmd_line, jobsList);
    }
        //else if ...
  //.....
  else {
    return new ExternalCommand(cmd_line, this, jobsList);
  }

  return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {

    Command* cmd = CreateCommand(cmd_line);


    if(cmd != nullptr) {
        cmd->execute();
    }

    delete cmd;
    cmd = nullptr;
    //Please note that you must fork smash process for some commands (e.g., external commands....)
}

/*int SmallShell::getLastJobPid() {
	return jobsList->getLastJobPid();
}*/

void SmallShell::setCurrentFGCmd(const char * cmd, int pid) {
    current_fg_cmd = cmd;
    current_fg_cmd_pid = pid;
}

int SmallShell::getCurrentFGCmdPid() {
    return current_fg_cmd_pid;
}

void SmallShell::addStoppedJob() {
    jobsList->addJob(current_fg_cmd, current_fg_cmd_pid, 1);
}
