#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdarg.h>

// limits
#define MAX_LINE 80
#define MAX_ARGS 40
#define MAX_HIST 10

/*
#define PROJ_ID  'K'
#define SHD_MEM_PATH "./shm_key"
*/

// globals
volatile sig_atomic_t sig_caught = 0;

typedef struct shared_mem_struct {
    caddr_t  buffer;
    caddr_t *arguments;
    caddr_t  cmd_history;
    size_t  *cmd_length;
    int  cursor;
    int  overflow;
} shared_mem;

// function headers
void read_func(shared_mem*, int const);
int parse_args(caddr_t, caddr_t*, size_t const);

void handler_func(int const sig)
{	
    /* update child proc control variable */
    sig_caught = 1;
}

void *init_shmem(size_t size) {
    /* mmap prot and flags, bitwise 'OR' */
    int protection = PROT_READ | PROT_WRITE;
    int map_flags  = MAP_SHARED | MAP_ANONYMOUS;

    return mmap(NULL, size, protection, map_flags, -1, 0);
}

char *get_history(caddr_t start, int const count) {
    return start + (count * MAX_LINE);
}


void print_history (shared_mem* data) {
    printf("\n\nLast 10 commands:\n");

    int end_pos = data->cursor;
    int current_pos = (data->overflow) ? data->cursor : 0;

    int i = 1;

    do {
        write(STDOUT_FILENO, 
            get_history(data->cmd_history, current_pos), 
            data->cmd_length[current_pos]);
        current_pos = (current_pos < MAX_HIST-1) ? current_pos + 1 : 0;
    } while (current_pos != end_pos);
}


int main(void)
{
    /* declare sigaction struct */
    struct sigaction sigactor;

    /* initialize sigaction struct */
    sigactor.sa_handler = handler_func;
    sigemptyset(&sigactor.sa_mask);
    sigactor.sa_flags = 0;

    /* set up sigaction for SIGINT */
    if (sigaction(SIGINT, &sigactor, NULL) < 0) {
        perror("sigaction");
        _exit(EXIT_FAILURE);
    }


    /* set the buffer to no buffering */
    setvbuf(stdout, NULL, _IONBF, 0);

    /* initialize shared memory, cursor, and overflow, non-persistent */
    shared_mem *data  = (shared_mem*)init_shmem(sizeof(shared_mem));
    data->buffer      = (char*)init_shmem(sizeof(char) * MAX_LINE);
    data->arguments   = (char**)init_shmem(sizeof(char*) * MAX_ARGS);
    data->cmd_history = (char*)init_shmem(sizeof(char*) * MAX_HIST * MAX_LINE);
    data->cmd_length  = (size_t*)init_shmem(sizeof(size_t) * MAX_HIST);
    data->cursor      = 0;
    data->overflow    = 0;

    /* enter shell loop */
    while (1) {

        /* fork process and get child pid */
        int cpid;

        while(!sig_caught) {

            cpid = fork();

            /* child */
            if (cpid == 0) read_func(data, data->cursor);

            /* fork error */
            else if (cpid < 0) {
                perror("Error forking process");
                _exit(EXIT_FAILURE);
            }

            /* parent process begins here */
            else {

                /* variable to store status returned from child*/
                int cstatus;

                /* suspend parent until child exits *
                 * store return status in cstatus   */
                waitpid(cpid, &cstatus, 0);

                /* get status from child process and check for SIGTERM *
                 * SIGTERM is raised by child when someone enters '!q' */
                switch(WTERMSIG(cstatus))
                {

                /* user wants to quit */
                case SIGTERM:
                    // unmap memory
                    _exit(EXIT_SUCCESS);

                /* invalid string length */
                case SIGUSR1: 
                    puts("Please enter a valid string");
                    continue;
                
                case SIGUSR2:
                    puts("command not found...");
                    continue;

                default:
                    if (!sig_caught) {
                        if (data->cursor + 1 < MAX_HIST) {
                            data->cursor += 1;
                        }
                        else {
                            data->cursor = 0;
                            data->overflow = 1;
                        }
                    }
                
                }

                /*
                case SIGUSR2:
                    fflush(stdout);
                    cpid = fork();

                    continue;
                */
            }

        }// signal loop

        fflush(stdout);
        print_history(data);
        sig_caught = 0;

    }
}

void read_func(shared_mem* data, int const pos)
{
    printf("\nCMD > ");

    int background = 0;

    size_t length = read(STDIN_FILENO, data->buffer, MAX_LINE);

    if (data->buffer[0] == '!' && data->buffer[1] == 'q') raise(SIGTERM);

    /* copy memory contents to history */
    char *history_pos = get_history(data->cmd_history, pos);
    strncpy(history_pos, data->buffer, sizeof(char)*length);
    data->cmd_length[pos] = length;

    /* parse arguments and retu!qrn number of arguments */
    background = parse_args(data->buffer, data->arguments, length);

    /*
    if (background == 1) {
        kill(getpid(), SIGUSR2)
    }
    */

    /* run command */
    if (execvp(data->arguments[0], data->arguments) < 0) {
        raise(SIGUSR2);
    }

}

int parse_args(char buf[], char *argsv[], size_t const length)
{

    int i,      /* loop index for accessing buf array */
        start,  /* index where beginning of next command parameter is */
        ct,     /* index of where to place the next parameter into args[] */
        bckg;   /* background flag */


    /* read what the user enters on the command line */
    ct = 0;
    start = -1;
    bckg = 0;

    /* examine every character in the buf */
    for (i = 0; i < length; i++) {
        switch (buf[i]){
            case ' ':
            case '\t':       /* argument separators */
                if(start != -1){
                    argsv[ct] = (buf+start);    /* set up pointer */
                    ct++;
                }
                buf[i] = '\0'; /* add a null char; make a C string */
                start = -1;
                break;

            case '\n':                 /* should be the final char examined */  
                if (start != -1){
                    argsv[ct] = &buf[start];
                    ct++;
                }
                buf[i] = '\0';
                argsv[ct] = NULL; /* no more arguments to this command */
                break;

            case '&':
                bckg = 1;
                buf[i] = '\0';
                break;

            default:             /* some other character */
                if (start == -1)
                    start = i;
        }

    }
    argsv[ct] = NULL; /* just in case the input line was > 80 */

    return bckg;
}

