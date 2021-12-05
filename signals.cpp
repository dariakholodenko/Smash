//
// Created by oreno on 29-Nov-21.
//

#include <iostream>
#include <signal.h>
#include <unistd.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlZHandler(int sig_num) { 
    cout << "smash: got ctrl-Z\n";
    SmallShell& smash = SmallShell::getInstance();
    int pid = smash.getCurrentFGCmdPid();
    
    if(pid != -1) { 
		kill(pid, SIGSTOP);
		smash.addStoppedJob();
		smash.setCurrentFGCmd(nullptr, -1);
		cout << "smash: process " << pid << " was stopped\n";
	}
}

void ctrlCHandler(int sig_num) {
    cout << "smash: got ctrl-C\n";
    SmallShell& smash = SmallShell::getInstance();
    int pid = smash.getCurrentFGCmdPid();
    
    if(pid != -1) { 
		kill(pid, SIGKILL);
		cout << "smash: process " << pid << " was killed\n";
	}
	else { //temporarily for debug as alternative for regular ctr-c
		pid = getpid();
		kill(pid, SIGKILL); 
	}
}

void alarmHandler(int sig_num) { 
	//TODO implement sig_alarm
}


