/**************************************************************************************************
 * Name: Allyson Aoki                                                                             *
 * Assignment 3                                                                                   *
 * CS 374                                                                                         *
 **************************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>

#define MAX_ARG 512
#define MAX_INPUT 2049

/* ***********************************************************************************************
*  1. Provide a prompt for running commands                                                      *
*  2. Handle blank lines and comments, which are lines beginning with the # character            *
*  3. Provide expansion for the variable $$                                                      *
*  4. Execute 3 commands: exit, cd, and status via code built into the shell                     *
*  5. Execute other commands by creating new processes using a function from the exec family     *
*     of functions                                                                               *
*  6. Support input and output redirection                                                       *
*  7. Support running commands in foreground and background processes                            *
*  8. Implement custom handlers for 2 signals, SIGINT and SIGTSTP                                *
**************************************************************************************************/


/*************************************************************************************************
 * global variable that checks if a process is running in the foreground or background           *
 *************************************************************************************************/
int is_background = 1;


/*************************************************************************************************
 * handles the SIGINT signal and terminates execution                                            *
 *************************************************************************************************/
void handle_SIGINT(int signo){
    //do nothing for the parent shell
    if (getpid() != 0)
        return;

    //print message for foreground child process termination
    if (signo == SIGINT){
        if (is_background == 1){
            printf("terminated by signal %d\n", signo);
            fflush(stdout);
            return;
        }
    }
}


/************************************************************************************************
 * Handles signals for SIGINT background and allows/stop background processes and relays        *
 * that info to the user                                                                        *
 ************************************************************************************************/
void handle_SIGTSTP(){
    //allows background processes
    if (is_background == 1){
        is_background = 0;
        printf("Entering foreground-only mode (& is now ignored)\n");
        printf(": ");
    }
    //only allows foreground processes
    else{
        is_background = 1;
        printf("Exiting foreground-only mode (& is now allowed)\n");
        printf(": ");
    }
    fflush(stdout);
}



/************************************************************************************************
 *   goes through each character of a command to search for $$ to perform variable expansion    *
 ************************************************************************************************/
void expand_variable(char* command[]){
    int i, j;
    for (i = 0; command[i]; i++){
        for (j = 0; command[i][j]; j++){
            if(command[i][j] == '$' && command[i][j+1] == '$'){
                // replace $$ with the process ID
                command[i][j] = '\0';
                int new_length = snprintf(NULL, 0, "%s%d", command[i], getpid());
                char* new_command = malloc(new_length + 1);
                snprintf(new_command, new_length + 1, "%s%d", command[i], getpid());
                free(command[i]);
                command[i] = new_command;
            }
        }
    }
}


/************************************************************************************************
 *  gets user input                                                                             *
 ************************************************************************************************/
char* get_input(){
    //get input
    printf(": ");
    fflush(stdout);
    char* input = malloc(MAX_INPUT * sizeof(char));
    //read user input
    fgets(input, MAX_INPUT, stdin);
    input[strlen(input) - 1] = '\0';
    return input;
}


/***********************************************************************************************
 *  parses user input to get command, background status, and input/output redirection          *
 ***********************************************************************************************/
void get_command(char* command[], int* background, char input_file[], char output_file[]){
    char* user_input = get_input();
    // check for blank line
    if (strcmp(user_input, "") == 0){
        //if the user input is blank, set the first argument to '#' to mark it as a comment
        //command[0][0] = '#';
        return;
    }
    const char delim[2] = " ";
    char *argument = strtok(user_input, delim);
    int i = 0;
    while (argument){
        if (strcmp(argument, "<") == 0){
            // if the argument is '<', set the next argument as the input file
            argument = strtok(NULL, delim);
            strcpy(input_file, argument);
        }
        else if (strcmp(argument, ">") == 0){
            // if the argument is '>', set the next argument as the output file
            argument = strtok(NULL, delim);
            strcpy(output_file, argument);
        }
        else if (strcmp(argument, "&") == 0)
            *background = 1; // set background flag if '&' is present
        else{
            // copy the argument to the command array and perform variable expansion
            command[i] = strdup(argument);
            expand_variable(command);
        }
        i++;
        argument = strtok(NULL, delim);
    }
    free(user_input);
}


/***********************************************************************************************
 *  handles changing directories                                                               *
 ***********************************************************************************************/
void change_directories(char* path){
    if (path == NULL){
        // if no path is provided, change to the home directory
        char* home_dir = getenv("HOME");
        if (home_dir != NULL)
            chdir(home_dir);
        else   
            printf("HOME environment variable not set.\n");
    }
    else{
        // if a path is provided, attempt to change to that directory
        if (chdir(path) == -1){
            printf("Cannot open directory: %s\n", path);
            fflush(stdout);
        }
    }
}

/***********************************************************************************************
 * handles displaying the exit status or termination signal of the last ran foreground process *
 ***********************************************************************************************/
void show_status(int* status){
    if (WIFEXITED(*status))
        printf("exit value %d\n", WEXITSTATUS(*status));
    else 
        printf("terminated by signal %d\n", WTERMSIG(*status));
}

/***********************************************************************************************
 *  function to execute a command                                                              *
 ***********************************************************************************************/
void execute_command(char* command[], int* background, int* exit_status, char* input_file, char* output_file, struct sigaction sig_handle){
    int result;
    //fork a child process
    pid_t pid = -1;
    pid = fork();

    switch(pid){
        case -1: //error process
            //displays an error message if child could not be spawned
            perror("Error spawning child\n");
            exit(1);
            break;
    
        case 0: //child process

            //ignore SIGINT signal for the child process
            sig_handle.sa_handler = SIG_IGN;
            sigaction(SIGINT, &sig_handle, NULL);

            //handle input file redirection if specified
            if (strcmp(input_file, "") != 0){
                //open the file for read only
                int in = open(input_file, O_RDONLY);
                if (in == -1){
                    printf("Cannot open %s for input\n", input_file);
                    fflush(stdout);
                    exit(1);
                }
                result = dup2(in, 0);
                //if duplicating the file descriptor was unsuccessful
                if (result == -1){
                    perror("ERROR: input file redirection\n");
                    exit(2);
                }
                fcntl(in, F_SETFD, FD_CLOEXEC);
            }

            //handle output file redirection if specified
            if (strcmp(output_file, "") != 0){
                //open the file for writing
                //create file if it doesn't exist and truncate it to zero length
                int out = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (out == -1){
                    printf("%s: No such file or directory\n", output_file);
                    fflush(stdout);
                    exit(1);
                }
                result = dup2(out, 1);

                //if duplicating the file descriptor was unsuccessful
                if (result == -1){
                    perror("ERROR: output file redirection\n");
                    exit(2);
                }
                //close-on-exec flag for the file descriptor 'out'
                fcntl(out, F_SETFD, FD_CLOEXEC);
            }

            //execute the command using execvp
            if (execvp(command[0], (char* const*)command)){
                printf("%s: command not found\n", command[0]);
                fflush(stdout);
                exit(2);
            }
            break;
        
        default: //parent process
            //check if the command is in the background and print background process information
            if (*background == 1 && is_background == 1){
                pid_t curr_pid = waitpid(pid, exit_status, WNOHANG);
                printf("Background pid is %d\n", pid);
                fflush(stdout);
            }
            else{  
                //wait for the child process to finish in foreground mode
                pid_t curr_pid = waitpid(pid, exit_status, 0);
            }
    
            //check for any terminated child process in the background
            while ((pid = waitpid(-1, exit_status, WNOHANG)) > 0){
                printf("Child %d terminated.\n", pid);
                if (WIFEXITED(*exit_status))
                    //exit value of the child process
                    printf("exit value %d\n", WEXITSTATUS(*exit_status));
                else    
                    //termination signal of the child process
                    printf("Terminated by signal %d.\n", WTERMSIG(*exit_status));
                fflush(stdout);
            }
    }
}

/***********************************************************************************************
 *  main function that calls other functions to form the actions of the small shell            *
 ***********************************************************************************************/
int main(){
    //parse command for arguments, input/output redirection, background execution
    char* arguments[MAX_ARG];
    int background = 0;
    char input_file[MAX_INPUT];
    char output_file[MAX_INPUT];
    int exit_status = 0;

    //clears the input and output files and arguments array
    memset(input_file, '\0', sizeof(input_file));
    memset(output_file, '\0', sizeof(output_file));
    int i;
	    for(i = 0; i < MAX_ARG; i++) {
		    arguments[i] = NULL;
	    }

    //signal handler for SIGINT (^C)
    struct sigaction SIGINT_action;  
    SIGINT_action.sa_handler = handle_SIGINT;
    sigfillset(&SIGINT_action.sa_mask);  //block signals while handler is executing
    SIGINT_action.sa_flags = 0;
    sigaction(SIGINT, &SIGINT_action, NULL);  //register the SIGINT signal handler

    //signal handler for SIGTSTP (^Z)
    struct sigaction SIGTSTP_action;
    SIGTSTP_action.sa_handler = handle_SIGTSTP; 
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = 0;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    while (1){
        get_command(arguments, &background, input_file, output_file);

        //checks for empty command or comments
        if (arguments[0][0] == ' ' || arguments[0][0] == '#'){
            for (i = 0; arguments[i]; i++){
                free(arguments[i]);
                arguments[i] = NULL;
            }
        continue;
        }

        //handle built-in commands or execute external commands
        else if (strcmp(arguments[0], "exit") == 0)
            exit(0);
        else if (strcmp(arguments[0], "cd") == 0)
            change_directories(arguments[1]);
        else if (strcmp(arguments[0], "status") == 0)
            show_status(&exit_status);
        else
            execute_command(arguments, &background, &exit_status, input_file, output_file, SIGINT_action);
        

        //free memory and reset variables
        for (i = 0; arguments[i]; i++){
            free(arguments[i]);
            arguments[i] = NULL;
        }
        background = 0;
        input_file[0] = '\0';
        output_file[0] = '\0';
    }
    return 0;
}
