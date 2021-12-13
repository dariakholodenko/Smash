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
	: Command(cmd_line) {}

ShowPidCommand::ShowPidCommand(const char* cmd_line) 
										: BuiltInCommand(cmd_line) {} 

void ShowPidCommand::execute() {
    SmallShell& shell = SmallShell::getInstance();
    std::cout<< "smash pid is "<< shell.getPid() << std::endl;
    int pid = getpid();
    if (pid < 0){
        perror("smash error: getpid failed");
    } else{
        //std::cout<< "smash pid is "<< getpid() << std::endl;
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
    } else if (num_args >= 2) {
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

    char* buf = getcwd(nullptr ,COMMAND_ARGS_MAX_LENGTH);
    if (buf == nullptr) {
        perror("smash error: getcwd failed");
        return;
    }

    if (num_args == 2){
        if (strcmp(args[1],"-") == 0){
            // go to last directory left with cd
            //cd was not used
            if (shell->getLastPwd().empty()){
                cerr << "smash error: cd: OLDPWD not set\n";
                return;
            } else {
                //cd was used
                if (chdir(shell->getLastPwd().c_str()) < 0){
                    perror("smash error: chdir failed");
                    return;
                } else{
                    shell->setLastPwd(buf);
                }
            }
        } else if (strcmp(args[1],"..") == 0){
            //go back to father
            if (chdir("..") < 0){
                perror("smash error: chdir failed");
                return;
            } else{
                shell->setLastPwd(buf);
            }
        }
        else{
            //regular change
            if (chdir(args[1]) < 0){
                //reset last pd for failure at the moment because I save it before the change
                perror("smash error: chdir failed");
//                shell->setLastPwd("");
                return;
            }
            shell->setLastPwd(buf);
        }

    }else if (num_args > 2){
        cerr << "smash error: cd: too many arguments\n";
        return;
    }
}

//=====================External Commands Implementation=================

ExternalCommand::ExternalCommand(const char *cmd_line, 
	SmallShell* shell, JobsList *jobs): Command(cmd_line), shell(shell) {
    jl = jobs;
}
												
void ExternalCommand::execute() {
  
	bool isBG = _isBackgroundComamnd(cmd_line);
	
	char* arg = (char*)malloc(strlen(cmd_line)+1);
	strcpy(arg, cmd_line);
	
	 _removeBackgroundSign(arg);
	 string ext_args = (string(arg));
  
  //this way we don't get warnings about convertion
	/*char* const bash = strdup("bash");
	char* const flag = strdup("-c");
	char* const args = strdup(ext_args.c_str());
	char* const paramlist[] = {bash, flag, args, NULL};*/
	
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
		execlp("/bin/bash", "bash", "-c", ext_args.c_str());
		
		perror("smash error: executioin failed\n");

		if (kill(cpid, SIGKILL) < 0){
            perror("smash error: kill failed");
        }
	}
	else {

		if(isBG) {
			tempjobId = jl->addJob(cmd_line, pid, 0);
			
			if(tempjobId == -1) {
				perror("smash error: coudn't add a job to list");
			}
		

			if(waitpid(pid, NULL, WNOHANG) < 0) {
				perror("smash error: waitpid failed");
			}		
			
		}
		else {
			shell->setCurrentFGCmd(args[0], pid, -1);
			
			//WUNTRACED in case the child gets stopped.
			if(waitpid(pid, NULL, WUNTRACED) < 0){
				perror("smash error: waitpid failed");
			}
			else {
				shell->setCurrentFGCmd(nullptr, -1, -1);
			}
		}
	}
	
	/*free(bash);
	free(flag);
	free(args);*/
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
JobsList::JobsList():jobs_list(){
}

JobsList::~JobsList() {
}

int JobsList::addJob(const char *cmd, int pid, bool isStopped, int jid) {
	removeFinishedJobs();
    if (jobs_list.size() == MAX_COMMANDS) return -1;
    if (isStopped && jid != 0){
        for (auto i = jobs_list.begin(); i != jobs_list.end(); ++i) {
            if(i->jobId < jid){
                continue;
            }
            //found first element with bigger job id than stopped one, insert before it
            jobs_list.emplace(i, JobEntry(jid, cmd, pid, time(nullptr), isStopped));
            return jid;
        }
    }
    if (jobs_list.empty()){
        jobs_list.emplace_back(JobEntry(1, cmd, pid, time(nullptr), isStopped));
        return 1;
    }
    jobs_list.emplace_back(JobEntry(jobs_list.back().jobId + 1, cmd, pid,
                                    time(nullptr), isStopped));

    return jobs_list.back().jobId + 1;
}

static void printIdErrorMessage(int jobId, string commandType) {
    string message = "smash error: ";
    string str = ": job-id ";
    string  sjobid = to_string(jobId);
    string  end = " does not exist";
    message += commandType += str  += sjobid += end;
    cerr << message.c_str() << endl;
}

JobsList::JobEntry *JobsList::getJobById(int jobId) {
    if (jobId < 1 || jobs_list.empty()) {
        return nullptr;
    }
    int n = jobs_list.size();
    for (int i = 0; i < n; ++i) {
        if (jobs_list[i].jobId == jobId){
            return &jobs_list[i];
        }
    }
    return nullptr;
}

void JobsList::removeJobById(int jobId, string commandType) {
    for (auto i = jobs_list.begin(); i != jobs_list.end(); i++) {
        if (i->jobId == jobId){
            jobs_list.erase(i);
            return;
        }
    }
    //command not found
    printIdErrorMessage(jobId, commandType);
    return;
}

void JobsList::printJobsList() {
    //empty
    if (jobs_list.empty()) return;
    int  n = jobs_list.size();
    for (int i = 0; i <n; ++i) {
        cout << &jobs_list[i];
    }
}

ostream &operator<<(ostream &out, const JobsList::JobEntry *je) {
    out << "[" << je->jobId << "]" << " " << je->cmd_line << " : "
        << je->pid << " " << difftime(time(nullptr), je->startTime) << " secs";
    if (je->isStopped){
        out << " (stopped)";
    }
    out << endl;
    return out;
}

JobsList::JobEntry *JobsList::getLastJob(int *lastJobId) {
    if (jobs_list.empty()){
        return nullptr;
    }
    if (lastJobId != nullptr){
		*lastJobId = jobs_list.back().jobId;	
	}
	
    return &jobs_list.back();
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
    int n = jobs_list.size();
    for (int i = n-1; i >= 0 ; --i) {
        if (jobs_list[i].isStopped){
			if(jobId != nullptr){
				*jobId = i;
			}
            return &jobs_list[i];
        }
    }
    return nullptr;
}

void JobsList:: killAllJobs() {
    int n = jobs_list.size();
    for (int i = 0; i < n; ++i) {
        cout << jobs_list[i].pid << ": "
             << jobs_list[i].cmd_line << endl;
        if (kill(jobs_list[i].pid, SIGKILL) < 0){
            perror("smash error: kill failed");
        }
    }
}

void JobsList::removeFinishedJobs() {
    int wstatus;
    int n = jobs_list.size();
    for (int i = 0; i <  n ; ++i) {
        //test signal to see pid is still running in background
        int wpid = waitpid(jobs_list[i].pid, &wstatus , WNOHANG);
        if (wpid == 0) continue;
        if ((wpid == jobs_list[i].pid &&
             (WIFEXITED(wstatus) || WIFSIGNALED(wstatus)))
            || (wpid == -1 && errno == ESRCH)){
            //perror("smash error: waitpid failed");
            removeJobById(jobs_list[i].jobId,"");
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
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }
    string str_sig_num = args[1];
    int n =str_sig_num.length();
    //sig num should be -N or -NN where N-NN is 0-31
    if (n < 2 || n > 3){
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }else{
        if (str_sig_num[0] != '-'){
            cerr << "smash error: kill: invalid arguments" << endl;
            return;
        } else{
            str_sig_num.erase(0,1);
        }

    }
    int jobId = 0;
    int sig_num = 0;
    try{
		jobId = stoi(args[2]);
		sig_num = stoi(str_sig_num);
		if (jobId < 1 || jobId > 100){
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
				<< " was sent to pid " << je->pid << endl;
		}
	}
	catch (const std::exception& e) {
		cerr << "smash error: kill: invalid arguments" <<endl;
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
    string  no_stopped = " there is no stopped jobs to resume";
    string  args = " invalid arguments";
    message += commandType;
    message += str;
    SmallShell& smash = SmallShell::getInstance();
    const char * t_cmd_line = nullptr;
    int t_pid = 0,t_jid = 0;
    int status;
    if (num_args == 1){
        if (commandType == "fg"){
            //bring job with maximal jobID to foreground
            je = jl->getLastJob(nullptr);
            if (je == nullptr){
                cerr << (message += empty).c_str() << endl;
                return;
            }
            t_cmd_line = je->cmd_line.c_str();
            t_pid = je->pid;
            t_jid = je->jobId;
            cout << t_cmd_line << " : " << t_pid << endl;
            smash.setCurrentFGCmd(t_cmd_line, t_pid ,t_jid);
            jl->removeJobById(t_jid,"fg");
            if (kill(t_pid,SIGCONT) < 0){
                perror("smash error: kill failed");
                return;
            }
            if (waitpid(t_pid, &status, WUNTRACED) < 0){
                perror("smash error: waitpid failed");
                return;
            }
            return;
        } else if (commandType == "bg"){
            je = jl->getLastStoppedJob(nullptr);
            if (je == nullptr){
                cerr << (message += no_stopped).c_str()
                     << endl;
                return;
            }
            cout << (je->cmd_line + " : ").c_str() << je->pid << endl;
            if (kill(je->pid,SIGCONT) < 0){
                perror("smash error: kill failed");
                return;
            }
            je->isStopped = false;
            return;
        }

    } else if (num_args == 2){
        //bring job with args[1] jobID to foreground
        je = jl->getJobById(stoi(args_1));
        if (je == nullptr){
            printIdErrorMessage(stoi(args_1), commandType);
            return;
        }
        if (commandType == "bg"){
            if (!je->isStopped){
                cerr << (message).c_str() << "job-id "
                     << je->jobId << " is already running in the background"<< endl;
            } else{
                cout << (je->cmd_line + " : ").c_str() << je->pid << endl;
                if (kill(je->pid,SIGCONT) < 0){
                    perror("smash error: kill failed");
                }
                je->isStopped = false;
            }

        } else{
            //fg
            t_cmd_line = je->cmd_line.c_str();
            t_pid = je->pid;
            t_jid = je->jobId;
            smash.setCurrentFGCmd(t_cmd_line, t_pid ,t_jid);
            cout << t_cmd_line << " : " << t_pid << endl;
            jl->removeJobById(t_jid, commandType);
            if (kill(t_pid,SIGCONT) < 0){
                    perror("smash error: kill failed");
                    return;
            }
            if (waitpid(t_pid, &status, WUNTRACED) < 0){
                perror("smash error: waitpid failed");
                return;
            }
            return;
        }

    }else{
        cerr << (message += args).c_str() << endl;
        return;
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
//===========================Quit command Implementation=================================

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
            << " jobs:\n";
            jl->killAllJobs();
        }
    }
    //if kill was not sent
    exit(1);
}


//======================Head Implementation===============

HeadCommand::HeadCommand(const char* cmd_line) 
										: BuiltInCommand(cmd_line) {
	if(num_args == 1) {
		isFailed = true;
		cerr << "smash error: head: not enough arguments\n";
		return;
	}
	if(num_args > 3) {
		isFailed = true;
		cerr << "smash error: head: too much arguments\n";
		return;
	}
	else if(num_args > 2) {
		path = string(args[2]);
		
		string temp = string(args[1]);
		temp = _trim(temp);
		
		if(temp.length() > 1 && temp.at(0) == '-') {
			
			num_lines = abs(stoi(temp));
		}
		else {
			isFailed = true;
			cerr << "smash error: head: invalid arguments\n";
			return;
		}
	}
	else {
		path = string(args[1]);
		path = _trim(path);
	}
}

int HeadCommand::readLine(int fd, string* buffer) {
	string str("");
	char c;
	int status = 1;
	
	do {
		status = read(fd, &c, sizeof(char));
		if(status == -1) {
			perror("smash error: read failed");
			return -1;
		}
		
		if((size_t)status < sizeof(char)) { //we've reached EOF
			break;
		}
		
		str.push_back(c);
		
	}
	while(c != '\n');
	
	*buffer = str;
	return status;
}

void HeadCommand::execute() {
	
	if(isFailed == true) {
		return;
	}
	
	
	int fd = open(path.c_str(), O_RDONLY);
	
	if(fd == -1) {
		perror("smash error: open failed");
		return;
	}

	int cnt = 0;
	int status;
	do {
		string buffer;
		status = readLine(fd, &buffer);
		if(status == -1){
			close(fd);
			return;
		}
		
		cout << buffer;
		//write(1, buffer.c_str(), buffer.length());
		
		if(status == 0) {
			close(fd);
			return;
		}
		
		cnt++;
	}
	while(cnt < num_lines);
	
	close(fd);
}

//======================RedirectionCommand Implementation===============

RedirectionCommand::RedirectionCommand(const char* cmd_line
, bool isAppend, SmallShell* shell)
: Command(cmd_line), isAppend(isAppend), shell(shell) {
	
	bool isBg;
	string cmd(cmd_line);
	
	if(_isBackgroundComamnd(cmd_line) == true) {
		isBg = true;
		char* cmd_c = (char*)malloc(cmd.length()+1);
		strcpy(cmd_c, cmd.c_str());
		_removeBackgroundSign(cmd_c);
		cmd = string(cmd_c);
		
		free(cmd_c);
		cmd_c = nullptr;		
	}
	
	size_t l = cmd.find_first_of(">");
	int offset = 1;
	
	if(isAppend == true) {
		offset = 2;
	}
	
	red_cmd_args.push_back(cmd.substr(0, l));
	red_cmd_args.push_back(cmd.substr(l+offset, cmd.length()));
	
	if(red_cmd_args[0].empty() || red_cmd_args[1].empty()) {
		isFailed = true;
	}
	else {
		red_cmd_args[0] = _trim(red_cmd_args[0]);
		red_cmd_args[1] = _trim(red_cmd_args[1]);
		
		if(isBg == true) {
			red_cmd_args[0].append("&");
		}
	}
	
}

void RedirectionCommand::execute() {
	
	if(isFailed == true) {
		cerr << "smash: redirection failed" << endl;
		return;
	}
	
	int fd;
	int save_out;
	
	if(isAppend == true) {
		fd = open(red_cmd_args[1].c_str(), O_WRONLY | O_CREAT 
													| O_APPEND, 0666);
	}
	else {
		fd = open(red_cmd_args[1].c_str(), O_WRONLY | O_CREAT 
													| O_TRUNC, 0666);
	}
	
	if(fd == -1) {
		perror("smash error: open failed");
		return;
	}
	
	save_out = dup(fileno(stdout));
	
	if(save_out == -1) {
		perror("smash error: dup failed");
		close(fd);
		return;
	}
	
	if(dup2(fd, fileno(stdout)) == -1) {
		perror("smash error: dup2 failed");
		close(fd);
		//close(save_out);
		return;
	}	
	
	Command* command = shell->CreateCommand(red_cmd_args[0].c_str());
	command->execute();
	
	delete command;
	command = nullptr;

	fflush(stdout);
	close(fd);
	
	if(dup2(save_out, fileno(stdout)) == -1) {
		perror("smash error: dup2 failed");
		close(save_out);
		return;
	}
	
	close(save_out);
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
        if (cmd_s.empty()){
        return nullptr;
    }
    jobsList->removeFinishedJobs();
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
    if (firstWord.back() == '&'){
        firstWord.pop_back();
        if (built_in_commands.find(firstWord.c_str()) == built_in_commands.end()){
            firstWord.push_back('&');
        }
    }

	size_t idx = ((string)cmd_line).find_first_of('>');
	size_t idy = cmd_s.find_first_of('|');


	if(idx != std::string::npos) {
		bool isAppend = false;
		if(cmd_line[idx+1] == '>') {
			isAppend = true;
		}
		return new RedirectionCommand(cmd_line, isAppend, this);
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
    else if(firstWord.compare("head") == 0) {
		return new HeadCommand(cmd_line);
	}
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
}

void SmallShell::setCurrentFGCmd(const char* cmd, int pid, int jid) {
    current_fg_cmd = cmd;
    current_fg_cmd_pid = pid;
    current_fg_cmd_jid = jid;
}

int SmallShell::getCurrentFGCmdPid() {
	return current_fg_cmd_pid;
}

void SmallShell::addStoppedJob() {
    jobsList->addJob(current_fg_cmd, current_fg_cmd_pid, true ,
                     current_fg_cmd_jid);
}
