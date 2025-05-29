/* ICCS227: Project 1: icsh
 * Name: Dominique Bachmann
 * StudentID: 6581162
 */

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/wait.h"
#include "signal.h"
#define MAX_CMD_BUFFER 255
#include "kirby.h"
#include "fcntl.h"  
#include "errno.h"  
#include "job.h"

pid_t FgPid = -1;
int LastExitCode = 0;

void printPrompt() {
    check_background_jobs();  
    write(STDOUT_FILENO, "icsh $ ", 7);
    fflush(stdout);
}

void ctrlC(int sig) {
    if (FgPid > 0) {
        kill(-FgPid, SIGINT); 
    } else {
        write(STDOUT_FILENO, "\nicsh $ ", 8);
        fflush(stdout);
    }
}
 
void ctrlZ(int sig) {
    if (FgPid > 0) {
        kill(-FgPid, SIGTSTP); 
    } else {        
        write(STDOUT_FILENO, "\nicsh $ ", 8);
        fflush(stdout);
    }
}

static void sigchld_handler(int sig) {
    int status;
    pid_t pid;
    int any_done = 0;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        int idx = find_job_by_pid(pid);
        if (idx < 0) {
            continue;
        }

        dprintf(STDOUT_FILENO,
                "\n[%d]+  Done                    %s\n",
                jobs[idx].job_id,
                jobs[idx].command);

        remove_job(idx);
        any_done = 1;
    }

    if (any_done) {
        dprintf(STDOUT_FILENO, "icsh $ ");
        fflush(NULL);
    }
}








void runExternalCommand(char *inputLine) {
    if (inputLine == NULL || inputLine[0] == '\0') 
        return;
    
    // Save original command for job tracking
    // char originalCommand[MAX_CMD_BUFFER];
    // strncpy(originalCommand, inputLine, MAX_CMD_BUFFER - 1);
    // originalCommand[MAX_CMD_BUFFER - 1] = '\0';

    int is_background = 0;
    size_t len = strlen(inputLine);


    if (len > 0 && inputLine[len - 1] == '&') {
        is_background = 1;
        inputLine[--len] = '\0'; // Remove the '&'
        while (len > 0 && (inputLine[len - 1] == ' ' || inputLine[len - 1] == '\t')) {
            inputLine[--len] = '\0'; // Remove trailing spaces or tabs
        }
    }

    char originalCommand[MAX_CMD_BUFFER];
    strncpy(originalCommand, inputLine, MAX_CMD_BUFFER - 1);
    originalCommand[MAX_CMD_BUFFER - 1] = '\0';

    //parse args and handle redirection
    char *saveptr;
    char *args[MAX_CMD_BUFFER];
    int arg_count = 0;
 
    char input_copy[MAX_CMD_BUFFER];
    strncpy(input_copy, inputLine, MAX_CMD_BUFFER - 1);
    input_copy[MAX_CMD_BUFFER - 1] = '\0';
    
    char *token = strtok_r(input_copy, " ", &saveptr);
    while (token && arg_count < MAX_CMD_BUFFER-1) {
        args[arg_count++] = token;
        token = strtok_r(NULL, " ", &saveptr);
    }
    args[arg_count] = NULL;
    
    //ls aliases
    if (args[0] && strcmp(args[0], "la") == 0) {
    args[0] = "ls"; 
    args[1] = "-la";
    args[2] = NULL; 
    arg_count = 2;
    } else if (args[0] && strcmp(args[0], "ll") == 0) {
        args[0] = "ls"; 
        args[1] = "-l";
        args[2] = NULL; 
        arg_count = 2;

    } else if (args[0] && strcmp(args[0], "l") == 0) {
        args[0] = "ls"; 
        args[1] = "-l";
        args[2] = NULL; 
        arg_count = 2;
    }   
 
    if (arg_count == 0) return;

    // handle redirection parsing 
    char *input_file = NULL;
    char *output_file = NULL;
    int new_arg_count = 0;
    
    for (int i = 0; i < arg_count; i++) {
        if (strcmp(args[i], "<") == 0) {
            if (i + 1 < arg_count) {
                input_file = args[i + 1];
                i++; 
            }
        } else if (strcmp(args[i], ">") == 0) {
            if (i + 1 < arg_count) {
                output_file = args[i + 1];
                i++; 
            }
        } else {
            args[new_arg_count++] = args[i];
        }
    }
    args[new_arg_count] = NULL; 

    if(args[0] == NULL) {
        fprintf(stderr, "Error No COMMAND! Specified\n");
        return;
    }
    if (input_file == NULL && strchr(inputLine, '<')) {
        fprintf(stderr, "Error: Missing redirection file.\n");
        LastExitCode = 1;
        return;
    }
    if (output_file == NULL && strchr(inputLine, '>')) {
        fprintf(stderr, "Error: Missing output redirection file.\n");
        LastExitCode = 1;
        return;
    }
 
    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork Failed");
        LastExitCode = 1;
    } else if (pid == 0) {
        
        setpgid(0, 0);

        if (!is_background) {
            tcsetpgrp(STDIN_FILENO, getpid());
        }

        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);

        // Handle redirection
        if (input_file) {
            int fd = open(input_file, O_RDONLY);
            if (fd < 0) {
                perror("Input file error");
                _exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        
        if (output_file) {
            int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror("Output file error");
                _exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        
        execvp(args[0], args);
        fprintf(stderr, "Bad Command! \n");
        _exit(1);
    } else {    
        setpgid(pid, pid);
        
        if (is_background) {
            add_job(pid, originalCommand, 1);
            LastExitCode = 0;
        } else {
            // Fg job, wait for completion
            FgPid = pid;
            tcsetpgrp(STDIN_FILENO, pid);

            int status;
            pid_t result = waitpid(pid, &status, WUNTRACED);
            
            //Take back terminal control
            tcsetpgrp(STDIN_FILENO, getpid());
            FgPid = -1;
            
            if (result == -1) {
                perror("waitpid failed");
                LastExitCode = 1;
            } else if (result > 0) {
                if (WIFEXITED(status)) {
                    LastExitCode = WEXITSTATUS(status);
                } else if (WIFSIGNALED(status)) {
                    LastExitCode = 128 + WTERMSIG(status);
                } else if (WIFSTOPPED(status)) {
                    add_job(pid, originalCommand, 0);
                    int idx = find_job_by_pid(pid);
                    if (idx >= 0) {
                        jobs[idx].status = JOB_STOPPED;
                        jobs[idx].is_background = 1; // Stopped jobs are background
                        printf("[%d]+  Stopped                 %s\n", 
                               jobs[idx].job_id, jobs[idx].command);
                        fflush(stdout);
                    } 
                    LastExitCode = 0;
                }
            } 
        }
    }
}
int main(int argc, char *argv[]) {
    pid_t shell_pgid = getpid();

    setpgid(shell_pgid, shell_pgid);        
    tcsetpgrp(STDIN_FILENO, shell_pgid);    
    signal(SIGTTIN, SIG_IGN);                
    signal(SIGTTOU, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);                


    //signal handlers
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = ctrlC;
    sa.sa_flags = SA_RESTART;  
    sigaction(SIGINT, &sa, NULL);
    
    sa.sa_handler = ctrlZ;
    sigaction(SIGTSTP, &sa, NULL);

    struct sigaction sa_chld;
    memset(&sa_chld, 0, sizeof(sa_chld));
    sa_chld.sa_handler   = sigchld_handler;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags     = SA_RESTART;   
    sigaction(SIGCHLD,   &sa_chld, NULL);
    
    StartMsg();
    char buffer[MAX_CMD_BUFFER];
    char execBuffer[MAX_CMD_BUFFER];
    char latest[MAX_CMD_BUFFER] = "";
    FILE *input = stdin;



    if (argc == 2) {
        input = fopen(argv[1], "r");
        if (!input) {
            perror("Failed to open script file");
            return 1;
        }
    }
 
    while (1) {
        if (input == stdin) {
            printPrompt();
        }
        
        if (!fgets(buffer, MAX_CMD_BUFFER, input)) {
            if (feof(input)) break;
            if (ferror(input)) {
                clearerr(input);
                continue;
            }
        }

        buffer[strcspn(buffer, "\n")] = '\0';

        if(buffer[0] == '\0') {
            continue; 
        }


        if (strncmp(buffer, "!!", 2) == 0) {
            if (latest[0] == '\0') {
                continue; 
            }
            const char *lastC = buffer + 2;
            execBuffer[0] = '\0'; 

            strncat(execBuffer, latest, MAX_CMD_BUFFER - 1);
            strncat(execBuffer, lastC, MAX_CMD_BUFFER - strlen(execBuffer) - 1);

            if (input == stdin) {
                printf("%s\n", execBuffer);
            }
        } 
        else 
        {
            strcpy(latest, buffer);
            strcpy(execBuffer, buffer);
        }
        


        if (strncmp(execBuffer, "exit", 4) == 0 &&
            (execBuffer[4] == '\0' || execBuffer[4] == ' ')) {
            int code = 0;
            sscanf(execBuffer + 4, " %d", &code);
            code &= 0xFF;
            if (input == stdin) {
                printf("bye\n");
            }
            if (input != stdin)
                fclose(input);
            exit(code);
        }
        if(strncmp(execBuffer, "jobs", 4) == 0 && 
            (execBuffer[4] == '\0' || execBuffer[4] == ' ')) {
            handle_jobs_command();
            continue;
        }
        if (strncmp(execBuffer, "fg", 2) == 0 && 
            (execBuffer[2] == '\0' || execBuffer[2] == ' ')) {
            handle_fg_command(execBuffer);
            continue;
        }
        if (strncmp(execBuffer, "bg", 2) == 0 &&
            (execBuffer[2] == '\0' || execBuffer[2] == ' ')) {
            handle_bg_command(execBuffer);
            continue;
        }


        if (strncmp(execBuffer, "echo", 4) == 0) {
            if (strchr(execBuffer, '<') || strchr(execBuffer, '>')) {
                runExternalCommand(execBuffer);
                continue;
            }
            if (execBuffer[4] == '\0') {
                printf("\n");
                LastExitCode = 0;
            } else if (execBuffer[4] == ' ') {
                if (strcmp(execBuffer + 5, "$?") == 0) {
                    printf("%d\n", LastExitCode);
                } else {
                    printf("%s\n", execBuffer + 5);
                }
                LastExitCode = 0;
            } else {
                printf("bad command\n");
                LastExitCode = 1;
            }
            continue;
        }

    
        runExternalCommand(execBuffer);
    }
    if (input != stdin) {
    fclose(input);
    }

    return 0;
}
