#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <string.h>
#include <time.h>
#include <iostream>

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
#define MAX_COMMANDS (100)

using namespace std;

class Command {
// TODO: Add your data members
protected:
	const char* cmd_line;
    int num_args = 0;
    char* args[COMMAND_MAX_ARGS];
 public:
  Command(const char* cmd_line);
  virtual ~Command();
  virtual void execute() = 0;

  const char* getCommandLine(){
      return cmd_line;
  }
  //virtual void prepare();
  //virtual void cleanup();
  // TODO: Add your extra methods if needed
};

class BuiltInCommand : public Command {
 public:
  BuiltInCommand(const char* cmd_line);
  virtual ~BuiltInCommand() {}
};

class ExternalCommand : public Command {
 public:
  ExternalCommand(const char* cmd_line);
  virtual ~ExternalCommand() {}
  void execute() override;
};

class PipeCommand : public Command {
  // TODO: Add your data members
 public:
  PipeCommand(const char* cmd_line);
  virtual ~PipeCommand() {}
  void execute() override;
};

class RedirectionCommand : public Command {
 // TODO: Add your data members
 public:
  explicit RedirectionCommand(const char* cmd_line);
  virtual ~RedirectionCommand() {}
  void execute() override;
  //void prepare() override;
  //void cleanup() override;
};


class SmallShell;

class ChangePromptCommand : public BuiltInCommand {
    SmallShell* shell;

public:
    ChangePromptCommand(const char* cmd_line, SmallShell *cur_shell);
    virtual ~ChangePromptCommand() {}
    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
public:
    SmallShell* shell;
    ChangeDirCommand(const char* cmd_line, SmallShell *pshell);
    virtual ~ChangeDirCommand();
    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
 public:
  GetCurrDirCommand(const char* cmd_line);
  virtual ~GetCurrDirCommand() {}
  void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
 public:
  ShowPidCommand(const char* cmd_line);
  virtual ~ShowPidCommand() {}
  void execute() override;
};

class JobsList;

class QuitCommand : public BuiltInCommand {
// TODO: Add your data members public:
  QuitCommand(const char* cmd_line, JobsList* jobs);
  virtual ~QuitCommand() {}
  void execute() override;
};

class JobsList {
    //assuming only 100 jobs, we'll use an array
 public:
  class JobEntry {
  public:
      JobEntry() = default;
      ~JobEntry() = default;
      int jobId;
      Command* command;
      int pid;
      time_t  startTime;
      bool isStopped;
      friend ostream & operator << (ostream &out, const JobEntry&je);
  };

private:
    //if job id is 0 => no running job at [i] location
    JobEntry jobs_list[MAX_COMMANDS +1];
    int num_entries;
 public:
  JobsList();
  ~JobsList();
  void addJob(Command* cmd, bool isStopped = false);
  void printJobsList();
  void killAllJobs();
  void removeFinishedJobs();
  JobEntry *getJobById(int jobId, string commandType);
  void removeJobById(int jobId, string commandType);
  JobEntry * getLastJob(int* lastJobId);
  JobEntry *getLastStoppedJob(int *jobId);

    // TODO: Add extra methods or modify exisitng ones as needed

};
class JobsCommand : public BuiltInCommand {
    JobsList* jl;
 public:
  JobsCommand(const char* cmd_line, JobsList* jobs);
  ~JobsCommand() {}
  void execute() override;
};

class KillCommand : public BuiltInCommand {
    JobsList* jl;
 public:
  KillCommand(const char* cmd_line, JobsList* jobs);
  virtual ~KillCommand() {}
  void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
 // TODO: Add your data members
 public:
  ForegroundCommand(const char* cmd_line, JobsList* jobs);
  virtual ~ForegroundCommand() {}
  void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
 // TODO: Add your data members
 public:
  BackgroundCommand(const char* cmd_line, JobsList* jobs);
  virtual ~BackgroundCommand() {}
  void execute() override;
};

class HeadCommand : public BuiltInCommand {
 public:
  HeadCommand(const char* cmd_line);
  virtual ~HeadCommand() {}
  void execute() override;
};

class SmallShell {
 private:
  // TODO: Add your data members
  JobsList* jobsList;
  string prompt;
  string lastPwd;
  SmallShell();
 public:
  Command *CreateCommand(const char* cmd_line);
  SmallShell(SmallShell const&)      = delete; // disable copy ctor
  void operator=(SmallShell const&)  = delete; // disable = operator
  static SmallShell& getInstance() // make SmallShell singleton
  {
    static SmallShell instance; // Guaranteed to be destroyed.
    // Instantiated on first use.
    return instance;
  }
  ~SmallShell();
  void executeCommand(const char* cmd_line);
  // TODO: add extra methods as needed
  string getPrompt();
  void setNewPrompt(string new_prompt);
  string getLastPwd();
  void setLastPwd(string new_last_pwd);

};



#endif //SMASH_COMMAND_H_
