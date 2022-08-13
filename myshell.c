#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>

// this function determines the case we are in and in case it's the '|' or '>' case it return the index too
// since we know that the input is a proper command then we can break after we find the first and only '|' , '>' , '&'
void determine_case_and_index(int count, char **arglist, int arr[]){
    for (int i = 0; i < count; ++i){
        if (arglist[i][0] == '|'){
            arr[0] = 1;
            arr[1] = i;
            break;
        }
        else if (arglist[i][0] == '>'){
            arr[0] = 2;
            arr[1] = i;
            break;
        }
        else if (arglist[i][0] == '&'){
            arr[0] = 3;
            break;
        }
    }
}

// we call this function to make default signals
void reset_sig(){
    struct sigaction new_action;
    new_action.sa_flags = SA_RESTART;
    new_action.sa_handler = SIG_DFL;
    int y = sigaction(SIGINT, &new_action, 0);
    if (y == 0){
        return;
    }
    else{
        fprintf(stderr, "there was a problem with signal handling, ERROR: %s\n", strerror(errno));
    }
}



// we use the same code that we used in recitation 3 with a little modification in the signalsin order to make it
// appropriate to our program
int prepare(void){
    // Structure to pass to the registration syscall
    struct sigaction new_action;
    memset(&new_action, 0, sizeof(new_action));
    // Assign pointer to our handler function
    new_action.sa_handler = SIG_IGN;
    // Setup the flags
    new_action.sa_flags = SA_RESTART;
    // Register the handler
    if (0 != sigaction(SIGINT, &new_action, NULL)){
        fprintf(stderr,"Signal handle registration " "failed. %s\n", strerror(errno));
        return -1;
    }
    struct sigaction s;
    s.sa_flags = SA_NOCLDWAIT;
    if (sigaction(SIGCHLD, &s, 0) != 0){
        fprintf(stderr,"Signal handle registration " "failed. %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

// in this function we take an array of the words that make the command and we run the command as a child process
int process_arglist(int count, char **arglist){
    int pid;
    int temp[2];
    temp[0] = 0;
    temp[1] = 0;

    determine_case_and_index(count, arglist,temp);

    int x = temp[0]; // x is the case that we are in (there are 4 cases)
    int k = temp[1]; // k is the index of '|' or '>' in case we find them

    if (x == 0){ // the normal case where we didn't find '|' or '&' or '>'
        pid = fork();
        if (pid > 0){
            waitpid(pid,NULL,WUNTRACED);
        }
        else if (pid == 0){
            reset_sig();
            execvp(arglist[0],arglist);
            fprintf(stderr, "we got a return from execvp, ERROR: %s\n", strerror(errno));
            exit(1);
        }
        else{
            fprintf(stderr, "there was a problem with fork and it failed, ERROR: %s\n", strerror(errno));
            return 0;
        }
    }

    else if (x == 1){ // this is the piping case
        int y;
        arglist[k] = NULL; // we put NULL instead of the '|' and now arglist has the first command
        char** arglist2 = arglist + k + 1; // this will contain the second command
        int pfds[2];
        y = pipe(pfds);
        if (y == -1){
            fprintf(stderr, "there was a problem with piping, ERROR: %s\n", strerror(errno));
            return 0;
        }
        pid = fork();
        if (pid > 0){
            int newpid = fork();
            if (newpid > 0){
                close(pfds[0]);
                close(pfds[1]);
                waitpid(pid, NULL, WUNTRACED);
                waitpid(newpid, NULL, WUNTRACED);
            }
            else if(newpid == 0){
                reset_sig();
                close(pfds[1]);
                if (dup2(pfds[0],0) == -1){
                    fprintf(stderr, "there was a problem with dup2, ERROR: %s\n", strerror(errno));
                    exit(1);
                }
                close(pfds[0]);
                execvp(arglist2[0],arglist2);
                fprintf(stderr, "we got a return from execvp, ERROR: %s\n", strerror(errno));
                exit(1);
            }
            else {
                fprintf(stderr, "there was a problem with fork and it failed, ERROR: %s\n", strerror(errno));
                return 0;
            }
        }
        else if (pid == 0){
            reset_sig();
            close(pfds[0]);
            if (dup2(pfds[1],1)==-1){
                fprintf(stderr, "there was a problem with dup2, ERROR: %s\n", strerror(errno));
                exit(1);
            }
            close(pfds[1]);
            execvp(arglist[0],arglist);
            fprintf(stderr, "we got a return from execvp, ERROR: %s\n", strerror(errno));
            exit(1);
        }
        else{
            fprintf(stderr, "there was a problem with fork and it failed, ERROR: %s\n", strerror(errno));
            return 0;
        }
    }

    else if(x == 2){
        arglist[k] = NULL;
        int fd = open(arglist[count-1], O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);
        pid = fork();
        if(pid > 0 && waitpid(pid, NULL, WUNTRACED)==-1 && errno!=EINTR && errno != ECHILD){
            fprintf(stderr, "there was a problem wwith waitpid, ERROR: %s\n", strerror(errno));
            return 0;
        }
        if(pid == 0){
            reset_sig();
            dup2(fd, 1) ;
            execvp(arglist[0],arglist);
            fprintf(stderr, "we got a return from execvp, ERROR: %s\n", strerror(errno));
            exit(1);
        }
        if(pid < 0){
            fprintf(stderr, "there was a problem with fork and it failed, ERROR: %s\n", strerror(errno));
            return 0;
        }
        close(fd);
    }

    else{ // this is the case we found '&'
        pid = fork();
        if (pid == 0){
            arglist[count-1] = NULL;
            execvp(arglist[0],arglist);
            fprintf(stderr, "we got a return from execvp, ERROR: %s\n", strerror(errno));
            exit(1);
        }
        if (pid < 0){
            fprintf(stderr, "there was a problem with fork and it failed, ERROR: %s\n", strerror(errno));
            return 0;
        }
    }
    return 1;
}

int finalize(void){
    return 0;
}
