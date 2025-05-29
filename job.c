#include "job.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>



Job jobs[MAX_JOBS];
int jobCount = 0;
int nextJobId = 1;

void add_job(pid_t pid, const char *cmd, int is_bg) {
    if (jobCount >= MAX_JOBS) {
        fprintf(stderr, "Too many jobs\n");
        return;
    }
    Job *j = &jobs[jobCount];
    j->job_id = nextJobId++;
    j->pid = pid;
    strncpy(j->command, cmd, MAX_CMD_BUFFER - 1);
    j->command[MAX_CMD_BUFFER - 1] = '\0';
    j->status = JOB_RUNNING;
    j->is_background = is_bg;
    if (is_bg) {
        printf("[%d] %d\n", j->job_id, pid);
        fflush(stdout);
    }
    jobCount++;
}





void remove_job(int idx){
    if(idx < 0 || idx >= jobCount) 
        return;
    for (int i = idx; i < jobCount - 1; i++) {
        jobs[i] = jobs[i + 1];
    }
    jobCount--;
}

int find_job_by_pid(pid_t pid) {
    for (int i = 0; i < jobCount; i++) {
        if (jobs[i].pid == pid) {
            return i;
        }
    }
    return -1;
}
int find_job_by_id(int job_id) {
    for (int i = 0; i < jobCount; i++) {
        if (jobs[i].job_id == job_id) {
            return i;
        }
    }
    return -1;
}



//
int get_most_recent_job() {
    if (jobCount == 0) return -1;
    
    int recent_idx = 0;
    for (int i = 1; i < jobCount; i++) {
        if (jobs[i].job_id > jobs[recent_idx].job_id) {
            recent_idx = i;
        }
    }
    return recent_idx;
}

void check_background_jobs(void) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        int idx = find_job_by_pid(pid);
        if (idx < 0) continue;
        dprintf(STDOUT_FILENO,
                "\n[%d]+  Done    %s\n",
                jobs[idx].job_id,
                jobs[idx].command);
        remove_job(idx);
    }
}
    

void handle_jobs_command(void) {
    check_background_jobs(); 
    
    if (jobCount == 0) {
        return; 
    }
    
    // int most_recent_idx = get_most_recent_job();
    
    for (int i = 0; i < jobCount; i++) {
        Job *j = &jobs[i];
        const char *status_str = (j->status == JOB_RUNNING) ? "Running" : "Stopped";
        
        // printf("[%d]%s  %-8s                %s%s\n",
        //        j->job_id,
        //        marker,
        //        status_str,
        //        j->command,
        //        j->is_background ? " &" : "");
    printf("[%d]  %-8s  %s%s\n",
        j->job_id,
        status_str,
        j->command,
        (j->status == JOB_RUNNING && j ->is_background) ? " &" : "");    
        
    }
}

// Built-in "fg %n" command
void handle_fg_command(const char *args) {
    extern pid_t FgPid;
    extern int LastExitCode;

    const char *arg_start = args + 2; //Skip the word"fg"
    while (*arg_start == ' ') arg_start++; //Skip spaces
    
    int id;
    if (*arg_start == '%') {
        id = atoi(arg_start + 1);
    } else if (*arg_start >= '1' && *arg_start <= '9') {
        id = atoi(arg_start);
    } else {
        int recent_idx = get_most_recent_job();
        if (recent_idx < 0) {
            fprintf(stderr, "fg: no current job\n");
            return;
        }
        id = jobs[recent_idx].job_id;
    }

    int idx = find_job_by_id(id);
    if (idx < 0) {
        fprintf(stderr, "fg: %d: no such job\n", id);
        return;
    }
    
    Job *j = &jobs[idx];
    printf("%s\n", j->command);
    fflush(stdout);

    if (j->status == JOB_STOPPED) {
        kill(-j->pid, SIGCONT); //send it to process group
    }
    
    //move job to fg
    j->is_background = 0;
    j->status = JOB_RUNNING;
    FgPid = j->pid;
    
    
    tcsetpgrp(STDIN_FILENO, j->pid);
    
    // Wait for the job to complete or be stopped
    int status;
    pid_t result = waitpid(j->pid, &status, WUNTRACED);
    
    // Take back terminal control
    tcsetpgrp(STDIN_FILENO, getpid());
    
    // Reset foreground PID
    FgPid = -1;

    if (result > 0) {
        if (WIFEXITED(status)) {
            // Process exited normally
            LastExitCode = WEXITSTATUS(status);
            remove_job(idx);
        } else if (WIFSIGNALED(status)) {
            // Process was killed by signal
            LastExitCode = 128 + WTERMSIG(status);
            remove_job(idx);
        } else if (WIFSTOPPED(status)) {
            // Process was stopped (Ctrl+Z) 
            j->status = JOB_STOPPED;
            j->is_background = 1; // Stopped jobs are considered background
            printf("\n[%d]  Stopped                 %s\n", j->job_id, j->command);
            fflush(stdout);
            LastExitCode = 0; // Stopped, not an error
        }
    }
}

void handle_bg_command(const char *args) {
    const char *arg_start = args + 2; // 
    while (*arg_start == ' ') arg_start++; 
    
    int id;
    if (*arg_start == '%') {
        id = atoi(arg_start + 1);
    } else if (*arg_start >= '1' && *arg_start <= '9') {
        id = atoi(arg_start);
    } else {
        // No argument - use most recent stopped job
        int recent_idx = -1;
        for (int i = 0; i < jobCount; i++) {
            if (jobs[i].status == JOB_STOPPED) {
                if (recent_idx < 0 || jobs[i].job_id > jobs[recent_idx].job_id) {
                    recent_idx = i;
                }
            }
        }
        if (recent_idx < 0) {
            fprintf(stderr, "bg: no stopped jobs\n");
            return;
        }
        id = jobs[recent_idx].job_id;
    }
    
    int idx = find_job_by_id(id);
    if (idx < 0) {
        fprintf(stderr, "bg: %d: no such job\n", id);
        return;
    }
    
    Job *j = &jobs[idx];
    if (j->status != JOB_STOPPED) {
        fprintf(stderr, "bg: job %d already in background\n", id);
        return;
    }
    
    kill(-j->pid, SIGCONT);
    j->status = JOB_RUNNING;
    j->is_background = 1;
    printf("[%d]+ %s &\n", j->job_id, j->command);
    fflush(stdout);
}

