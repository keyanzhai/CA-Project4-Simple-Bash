#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <assert.h>
#include "parse.c"

// Global variables
cmd* command_arr[PIPELINE]; // the command array
char infile[1024+1]; /* the redirection in file name*/
char outfile[1024+1]; /* the redirection out file name */
int append = 0; // if redirection out is >>
int backgnd = 0; /* if background command, 1 for background, 0 for non-background */
int cmd_count = 0; // the number of commands in cmdLine

/* Get the prompt, but do not print */
void printPrompt(char* prompt)
{
    char hostname[100]; /* host name */
    char cwd[100]; /* current working directory */
    struct passwd *my_info; /* used for getpwuid() */
    char* root; /* $ or # */

    /* get host name */
    gethostname(hostname, sizeof(hostname));

    /* get current working directory */
    getcwd(cwd, sizeof(cwd));

    /* get the struct */
    my_info = getpwuid(geteuid());

    /* get the prompt */
    if (geteuid() == 0) /* if rooted */
        root = "#";
    else /* if not rooted */
        root = "$";

    /* login name: my_info->pw_name */
    /* home (~): my_info->pw_dir */
    /* abbreviate as ~ */
    if (strncmp(cwd, my_info->pw_dir, strlen(my_info->pw_dir)) == 0) /* If the cwd contains home (~), abbreviate it as ~ */
        printf("%s@%s:~%s%s ", my_info->pw_name, hostname, cwd+strlen(my_info->pw_dir), root);
    else
        printf("%s@%s:%s%s ", my_info->pw_name, hostname, cwd, root);
}

/* Input a command,
 * return 1 if it's a built-in command
 * otherwise return 0
 *
 * */
int isBuiltInCommand(cmd* command) {
    /* 4 built-in commands: cd, jobs, exit, kill */
    if ((strcmp(command->cmd_name, "cd")==0) || (strcmp(command->cmd_name, "jobs")==0) || (strcmp(command->cmd_name, "exit")==0) || (strcmp(command->cmd_name, "kill")==0))
        return 1;
    else
        return 0;
}

/* execute the built-in command */
void executeBuiltInCommand(cmd* command)
{
    if (strcmp(command->cmd_name, "cd")==0) {
        struct passwd *my_info; /* used for getpwuid() */
        char dir[100];
        /* get the struct */
        my_info = getpwuid(geteuid());

        if (command->num_args == 0)
            chdir(my_info->pw_dir);

        else if (command->num_args == 1) {
            if (command->args[1][0] == '~') {
                sprintf(dir, "%s%s", my_info->pw_dir, command->args[1]+1);
                chdir(dir);
            }
            else
                chdir(command->args[1]);
        }
    }
    else if (strcmp(command->cmd_name, "jobs")==0) {
        
    }
    else if (strcmp(command->cmd_name, "exit")==0) {
        exit(0);
    }
    else if (strcmp(command->cmd_name, "kill")==0) {

    }
}

/* calls execvp */
void executeCommand(cmd* command){
    //int i;
    //for (int i = 0; i < cmd_count; i++)
    //{
        /* code */
    //}
    
    execvp(command->cmd_name,command->args);
}

int isBackgroundJob(cmd* command){
    return backgnd;
}

int main (int argc, char *argv[])
{
    assert(argc == 2);
    FILE* test_file;
    test_file = fopen(argv[1], "r");
    while (1) {
        /* int childPid;  Used later when executing command */
        char prompt[100]; /* the prompt */
        char cmdLine[1024+1]; /* the command line */
    
        /* printPrompt(prompt); get the prompt */
      
        if (fgets(cmdLine, 1024, test_file) == NULL) {
            break;
        }

        if (cmdLine[strlen(cmdLine)-1] == '\n')
            cmdLine[strlen(cmdLine)-1] = '\0';
        
        /* If there is nothing input, continue to next loop */
        if (strlen(cmdLine) == 0)
            continue;


        parseCommand(cmdLine); /* parse the command line, get the command array */
        
        // Execute the commands in cmdLine
        if (isBuiltInCommand(command_arr[0])) {
            executeBuiltInCommand(command_arr[0]);
        } else {
            int childPid = 0;
            childPid = fork();
            if (childPid == 0) { // child process, call process A
                // Single command, no pipe
                if (cmd_count == 1) {
                    // Execute the command (including redirection)
                    if (infile[0] != '\0'){
                        int fdin = open(infile, O_RDONLY);
                        dup2(fdin,STDIN_FILENO);
                        close(fdin);
                    }

                    if (outfile[0] != '\0') {
                        int fdout;
                        if (append = 1)
                            fdout = open(outfile, O_RDWR|O_CREAT|O_APPEND,0777);
                        else
                            fdout = open(outfile, O_RDWR|O_CREAT|O_TRUNC,0777);
                        dup2(fdout, STDOUT_FILENO)==-1;
                        close(fdout);
                        executeCommand(command_arr[0]);
                    }else{
                        executeCommand(command_arr[0]);
                    }
                }

                // handle pipe (only 2 commands)
                else if (cmd_count == 2) { 
                    
                    // cmd1 | cmd2

                    // Create a pipe
                    int pfd[2];
                    if (pipe(pfd) == -1) {
                        perror("pipe");
                        exit(EXIT_FAILURE);
                    }

                    // Create process B1, execute cmd1
                    int B1Pid = 0;
                    B1Pid = fork();
                    if (B1Pid == 0) {
                        close(pfd[0]); // close the read end of the pipe
                        dup2(pfd[1], STDOUT_FILENO); // Redirect stdout as the write end
                        close(pfd[1]);

                        // Execute cmd1, write the output to the pipe
                        executeCommand(command_arr[0]);
                    }

                    // Create process B2, execute cmd2
                    int B2Pid = 0;
                    B2Pid = fork();
                    if (B2Pid == 0) {
                        close(pfd[1]); // close the write end of the pipe
                        dup2(pfd[0], STDIN_FILENO); // Redirect stdout as the write end
                        close(pfd[0]);

                        // Execute cmd2, take input from the pipe
                        executeCommand(command_arr[1]);
                    }

                    // In process A
                    close(pfd[0]); // close the read end
                    close(pfd[1]); // close the write end

                    // wait for the two child process B1 and B2
                    wait(NULL);
                    wait(NULL);
                }

            } else {
                if (isBackgroundJob(command_arr[0])){
                    // record in list of background jobs
                    signal(SIGCHLD, SIG_IGN);
                } else {
                    int stat = 0;
                    waitpid (childPid,&stat,0);
                }
            }
        } 

        /* Free the memory of command */
        for (int i = 0; i < PIPELINE; i++) {
            free(command_arr[i]);
        }
    }

    fclose(test_file);
}