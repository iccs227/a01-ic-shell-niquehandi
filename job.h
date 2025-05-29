#ifndef JOB_H
#define JOB_H

#include <sys/types.h>
#include <signal.h>

#define MAX_CMD_BUFFER 255
#define MAX_JOBS 64

typedef enum { JOB_RUNNING, JOB_STOPPED, JOB_DONE } JobStatus;

typedef struct {
    int job_id;
    pid_t pid;
    char command[MAX_CMD_BUFFER];
    JobStatus status;
    int is_background;
} Job;


extern Job jobs[MAX_JOBS];
extern int jobCount;
extern int nextJobId;

//Job table operations
void add_job(pid_t pid, const char *cmd, int is_bg);
void remove_job(int index);
int find_job_by_pid(pid_t pid);
int find_job_by_id(int job_id);

//Background job monitoring
void check_background_jobs(void);


//job control commands
void handle_jobs_command(void);
void handle_fg_command(const char *args);
void handle_bg_command(const char *args);

#endif //JOB_H
