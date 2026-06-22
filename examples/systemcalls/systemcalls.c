#include "systemcalls.h"
#include <stdlib.h>
#include <unistd.h> // For fork() and execv()
#include <sys/types.h> // For pid_t
#include <sys/wait.h> // For wait()
#include <fcntl.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/
	int ret = system(cmd);
    printf("System ret - %d\n", ret);
    if( ret == -1)
    {
        return false;
    }
    return true;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/
	 int status;
    pid_t pid=fork();

    if( pid < 0) // child forking failed
        return false;
    
    // forking success and now process is in child node, these lines will execute in child pid
    if ( pid == 0 ){
        execv( command[0], command);
        perror("execv error"); // This line only executes if execv fails
        exit(EXIT_FAILURE);
    }
    else{ // Parent process wait until child is closed
        printf("Parent is waiting\n");
        if (waitpid(pid, &status, 0) == -1) {
            return false;
        }
        printf("Child is completed\n");
    }
    
    va_end(args);
		
	 return WIFEXITED(status) && WEXITSTATUS(status) == 0; // status should be 0 if child executed successfully.
	 
    return true;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/
	 int status = 0;
    pid_t pid = fork();

    if( pid < 0) // for failed
        return false;


    if( pid == 0 ){ // child process
        int fd = open(outputfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) { 
            _exit(1);
        }
        if (dup2(fd, 1) < 0) { //dup2 command is used to duplicate this file descriptor to current process stdout
            close(fd);
            _exit(1);
        }
        close(fd);
        execv(command[0],command);
    }
    else{ // parent
        printf("Parent is waiting\n");
        if (waitpid(pid, &status, 0) == -1) {
            return false;
        }
        printf("Child is completed\n");
    }
    
    va_end(args);

    return true;
}
