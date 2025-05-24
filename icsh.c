/* ICCS227: Project 1: icsh
 * Name:
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

pid_t FgPid = -1;
int LastExitCode = 0;
 




void ctrlC(int sig) {
    if (FgPid > 0) {
        kill(FgPid, SIGINT);
    } else {
        write(STDOUT_FILENO, "\nicsh $ ", 8);
        fflush(stdout);
     }
}
 
void ctrlZ(int sig) {
    if (FgPid > 0) {
        kill(FgPid, SIGTSTP);
    }else {        
        write(STDOUT_FILENO, "\nicsh $ ", 8);
        fflush(stdout);
    }
}
 
void runExternalCommand(char *inputLine) {
    if (inputLine == NULL || inputLine[0] == '\0') 
        return;
     
    char *saveptr;
    char *args[MAX_CMD_BUFFER];
    int arg_count = 0;
 
    //Created a copy of the inputLine for tokenizations
    char input_copy[MAX_CMD_BUFFER];
    strncpy(input_copy, inputLine, MAX_CMD_BUFFER - 1);
    input_copy[MAX_CMD_BUFFER - 1] = '\0';
    
    char *token = strtok_r(input_copy, " ", &saveptr);
    while (token && arg_count < MAX_CMD_BUFFER-1) {
        args[arg_count++] = token;
        token = strtok_r(NULL, " ", &saveptr);
    }
    args[arg_count] = NULL;
 
    if (arg_count == 0) return;

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
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        
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
        // Parent process
        FgPid = pid; 
        int status;
        waitpid(pid, &status, WUNTRACED);
        FgPid = -1;   
 
        if (WIFEXITED(status))
            LastExitCode = WEXITSTATUS(status);
        else if (WIFSIGNALED(status))
            LastExitCode = 128 + WTERMSIG(status);
        else if (WIFSTOPPED(status)) {
            LastExitCode = 128 + WSTOPSIG(status);
            printf("[%d]+  Stopped\t\t%s\n", pid, inputLine);
        }
    }
}
 
void printPrompt() {
    write(STDOUT_FILENO, "icsh $ ", 7);
    fflush(stdout);
}
 
int main(int argc, char *argv[]) {


    //signal handlers
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = ctrlC;
    sa.sa_flags = SA_RESTART;  
    sigaction(SIGINT, &sa, NULL);
    
    sa.sa_handler = ctrlZ;
    sigaction(SIGTSTP, &sa, NULL);
    
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
            continue; // Skip empty lines
        }


        if (strcmp(buffer, "!!") == 0) {
            if (latest[0] == '\0') {
                if (input == stdin) {
                    continue;
                }
                continue;
            }
            strcpy(execBuffer, latest);
            if (input == stdin) {
               printf("%s\n", execBuffer);
            }

        }else{
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


        // if(strncmp(execBuffer, "echo ", 5) == 0) {
        //     if (strcmp(execBuffer + 5, "$?") == 0) {
        //     printf("%d\n", LastExitCode);
        //     } else {
        //     printf("%s\n", execBuffer + 5);
        //     LastExitCode = 0;
        //     }
        //     continue;
        // }
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
