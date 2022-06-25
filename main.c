/**
 * @file main.c
 * @brief checkFile main file, validates file extensions
 * @date 2021/11/04
 * @author Eduardo dos Santos Margarido
 * @author 2200667
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <signal.h>
#include <time.h>

#include "debug.h"
#include "memory.h"
#include "args.h"

// macro definitions for use in arrays
#define BUFFER_MAX          1024
#define LINE_MAX            256
#define EXT_MAX             8

// macro definitions for error codes
#define ERR_PARSER          1           
#define ERR_FORK            2
#define ERR_EXEC            3           
#define ERR_PIPE            4         
#define ERR_OPEN_FILE       5
#define ERR_CLOSE_FILE      6
#define ERR_FGETS           7
#define ERR_OPEN_DIR        8
#define ERR_CLOSE_DIR       9
#define ERR_COUNT_LINES     10
#define ERR_SIGUSR1         11
#define ERR_SIGQUIT         12
#define ERR_TIME            13

// macro definition for BATCH mode

#define BATCH               1


// An extra macro definition was implemented alongside the ones in memory.c, called REALLOC()

// global array to save valid extensions
char* validExt[] = {"pdf",
                    "gif",
                    "jpg",
                    "jpeg",
                    "png",
                    "mp4",
                    "zip",
                    "html"};

// global variables to be used in signal handler
int sigFileNum;
char * sigFile;
struct tm sigTimeInfo;

// functions
void lineSplit(int max, char buffer[], char ** linesPipe);
int extMatch(int pos, char* ext, char* token, char ** file);
int lineCounter(char * batchFile);
void signalHandler(int signal, siginfo_t *siginfo, void *context);


int main(int argc, char *argv[]) {

    // gengetopt struct used to save data from terminal
    struct gengetopt_args_info args;

    if(cmdline_parser(argc, argv, &args))
        ERROR(ERR_PARSER, "cmdline_parser() failed.");

    // -h/--help printf
    if(args.help_given){
        printf("\nAuthor:\t\tEduardo Margarido\nStudent Number:\t2200667\n");
        printf("Usage: \n\t-f/--file <filename> for a filename (up to 10)\n\t-b/--batch <filename> for a file with filenames\n\t-d/--dir <directory> for a directory (does not include subdirectories)\n");
        printf("\nSupported Filetypes:\n\t.pdf\n\t.gif\n\t.jpg/.jpeg\n\t.png\n\t.mp4\n\t.zip\n\t.html\n");
        cmdline_parser_free(&args);
        exit(0);
    }

    // count variables for summary
    int countMis = 0, countOK = 0, countNoExt = 0, countErr = 0, countTotal = 0, countUnSupp = 0;

    // signal struct
    struct sigaction act;
    // set function that handles signals
    act.sa_sigaction = signalHandler;
    // mask without signals, doesn't block signals
    sigemptyset(&act.sa_mask);
    // additional signal info (pid)
    act.sa_flags = SA_SIGINFO;
    act.sa_flags |= SA_RESTART;
    // pipe to redirect stdout and stderr
    int pipefd[2];
    if(pipe(pipefd) == -1){
        ERROR(ERR_PIPE, "pipe() failed");
    }
    // buffer to get raw output from file()
    char pipeBuffer[BUFFER_MAX];

    // var used in for loops to dictate the number loops
    int max = 0;
    if(args.file_given){ 
        max = args.file_given;
    }
    if(args.batch_given){
        // file pointer
        FILE *fp;
        // checks if the batch file is able to be opened and closed
	    if ((fp = fopen(args.batch_arg, "r")) == NULL){
		    ERROR(ERR_OPEN_FILE, "cannot open file '%s'", args.batch_arg);
        }
        else if (fclose(fp) != 0){
		    ERROR(ERR_CLOSE_FILE, "cannot close file '%s'", args.batch_arg);
        }
        max = args.batch_given;
    }

    // filenames acts as an array of pointers, saving all the file names while allocating more memory every loop
    char ** filenames;
    // counter of how many files were stored
    int dirCount = 0;
    // gets all file names in the given directory
    if(args.dir_given){
        // gets the last '/' in the directory entered by the user and checks the length, adding a '/' if needed
        char * foo;
        if((foo = strrchr(args.dir_arg, '/')) == NULL){
            strcat(args.dir_arg, "/");
        }
        else if((strcspn(foo + 1, "/")) != 0){
            strcat(args.dir_arg, "/");
        }
        // dir pointer
        DIR *dp;
        // dirent struct
        struct dirent *dir;
        // first allocation for a char pointer
        filenames = MALLOC(sizeof(char*));
        // checks if it is possible to open the directory
        if((dp = opendir(args.dir_arg)) != NULL){
            // reads file names until it hits NULL
            while((dir = readdir(dp)) != NULL){
                // trims the directories called "." and ".." as they're never used in this context
                if((strcmp(dir->d_name, ".")) == 0 || (strcmp(dir->d_name, "..")) == 0){
                    continue;
                }
                // increases the size of the original block of memory to fit one more char pointer
                filenames = REALLOC(filenames, sizeof(char*) * (dirCount + 1));
                // allocates memory on the dirCount position
                filenames[dirCount] = MALLOC(LINE_MAX);
                // copies and concactenates the string so the format is correct for use in execlp()
                strcpy(filenames[dirCount], args.dir_arg);
                strcat(filenames[dirCount], dir->d_name);
                dirCount++;
            }
            if((closedir(dp)) == -1){
                ERROR(ERR_CLOSE_DIR, "cannot close directory '%s'", args.dir_arg);
            }
        }
        else{
            ERROR(ERR_OPEN_DIR, "cannot open directory '%s'", args.dir_arg);
        }
        max = dirCount;
    }
    // for loop to pass through all the files (or through one batch file)
    for (int i = 0; i < max; i++){
        pid_t pid = fork();
        if (pid == 0) {
            // close reading end in the child
            close(pipefd[0]);

            // redirects stdout and stderr to the pipe
            dup2(pipefd[1], 1);
            dup2(pipefd[1], 2);

            // close writing end in the child
            close(pipefd[1]);
            // mode selection depending on which arguments were given
            if(args.file_given){
                if(execlp("file", "file", "--mime-type", "-b", args.file_arg[i], NULL) == -1){
                    ERROR(ERR_EXEC, "execlp() failed");
                }
            }
            if(args.batch_given){                
                if(execlp("file", "file", "--mime-type", "-bf", args.batch_arg, NULL) == -1){
                    ERROR(ERR_EXEC, "execlp() failed");
                }
                
            }
            if(args.dir_given){
                if(execlp("file", "file", "--mime-type", "-b", filenames[i], NULL) == -1){
                    ERROR(ERR_EXEC, "execlp() failed");
                }
            }
        }
        else if (pid < 0){                
            ERROR(ERR_FORK, "fork() failed");
        }
        // wait so output is in the correct order
        wait(NULL);
    }
    // close writing end in the parent
    close(pipefd[1]);
    // used to store parsed output from file()
    char * linesPipe[BUFFER_MAX];

    // read from pipe to pipeBuffer[]
    while(read(pipefd[0], pipeBuffer, sizeof(pipeBuffer)) != 0){};

    // makes sure lines are split by \n and equals max to the number of loops (number of files to check)
    if(args.batch_given){
        max = lineCounter(args.batch_arg);
        lineSplit(max, pipeBuffer, linesPipe);
    }
    else if(args.file_given){
        max = args.file_given;
        lineSplit(max, pipeBuffer, linesPipe);
    }
    else if(args.dir_given){
        max = dirCount;
        lineSplit(max, pipeBuffer, linesPipe);
    }

    // gets filenames from the given batch file
    if(args.batch_given){
        FILE *fp;
        if ((fp = fopen(args.batch_arg, "r")) == NULL){
            ERROR(ERR_OPEN_FILE, "fopen() failed");
        }
        // allocates memory for the needed amount of pointers
        filenames = MALLOC(sizeof(char *) * max);
        for (int i = 0; i < max; i++){
            // allocates memory for each line
            filenames[i] = MALLOC(sizeof(char) * LINE_MAX);
            if (fgets(filenames[i], LINE_MAX, fp) == NULL){
                ERROR(ERR_FGETS, "fgets() failed");
            }
            // trims the \n
            strtok(filenames[i], "\n");
        }
        if(fclose(fp) != 0){
            ERROR(ERR_CLOSE_FILE, "fclose() failed");
        }
    // gets filenames from gengetopt arg
    }else if(args.file_given){
        // allocates memory for the needed amount of pointers
        filenames = MALLOC(sizeof(char *) * max);
        for (int i = 0; i < max; i++){     
            // allocates memory for each line 
            filenames[i] = MALLOC(sizeof(char) * LINE_MAX);
            strcpy(filenames[i], args.file_arg[i]);
        }
    }

    // prints the process ID for signal sending purposes
    printf("[INFO] PID: '%d'\n", getpid());
    // getting current time
    time_t t;
    if((t = time(NULL)) == -1 ){
        ERROR(ERR_TIME, "time() failed");
    }
    sigTimeInfo = *localtime(&t);
    // loops through all files in filenames[]
    for (int pos = 0; pos < max; pos++){
        if(sigaction(SIGQUIT, &act, NULL) < 0){
            ERROR(ERR_SIGQUIT, "sigaction - SIQUIT");
        }
        if(args.batch_given){
            sigFileNum = pos;
            sigFile = MALLOC(strlen(filenames[pos]));
            strcpy(sigFile, filenames[pos]);
            if(sigaction(SIGUSR1, &act, NULL) < 0){
                ERROR(ERR_SIGUSR1, "sigaction -- SIGUSR1");
            }            
        }
        // sleep to test signals, uncomment if needed
        //sleep(3);
        FREE(sigFile);
        // flag, becomes 1 when the extension is valid
        int flagUnsupported = 0;

        // pointers to be used in strtok()
        char* token;
        char* delim = "/";

        // returns a pointer to the last '.' in the file name
        char* ext = strrchr(filenames[pos], '.');

        if(strstr(linesPipe[pos], "/directory") != NULL){
            continue;
        }
        // checks if the file utilitary analyzed the file
        if(strstr(linesPipe[pos], "cannot") != NULL){
            // default error handler
            FILE *fp;
            if ((fp = fopen(filenames[pos], "r")) == NULL){
                fprintf(stderr, "[ERROR] cannot open file '%s' -- %s\n", filenames[pos], strerror(errno));
            }
            else if(fclose(fp) != 0){
		        ERROR(ERR_CLOSE_FILE, "cannot close file '%s'");
            }
            // increments counters and goes to next loop
            countErr++;
            countTotal++;
            continue;
        }

        // creates a char array and copies the contents of linesPipe[pos] to that array, because strtok manipulates the original string
        char line[LINE_MAX];
        strcpy(line, linesPipe[pos]);
        // initializes token 
        token = strtok(line, delim);
        // skips ahead to the second token since the first one is never the file extension
        token = strtok(NULL, delim);
        // checks if extension is null
        if(ext == NULL){
            printf("[INFO] '%s' has no extension, file type is '%s'\n", filenames[pos], token);
            // increments counters and breaks out of while loop
            countNoExt++;
            countTotal++;
            continue;
        }
        // goes through valid extension list and compares the token to it
        for (int i = 0; i < EXT_MAX; i++){
            if(strncmp(token, validExt[i], (sizeof(char) * 5)) == 0){
                // if it matches, flagUnsupported becomes 1
                flagUnsupported = 1;
                // function to check if the extension matches to the file type
                int whichCount = extMatch(pos, ext, token, filenames);
                // increments counters based on return value and breaks out of for loop
                countTotal++;
                if(whichCount){
                    countOK++;
                }
                else{
                    countMis++;
                }
                break;
            }
        }
        // if the extension isn't valid, prints the unsupported line and increments counters
        if(flagUnsupported == 0){
            printf("[UNSUPPORTED] '%s': type '%s' is not supported by checkFile\n", filenames[pos], linesPipe[pos]);
            countUnSupp++;
            countTotal++;
        }
    }
    // prints the summary of files analyzed every time
    printf("[SUMMARY] files analyzed: %d; files OK: %d; files mismatched: %d; files without extension: %d; unsupported files: %d; errors: %d\n", countTotal, countOK, countMis, countNoExt, countUnSupp, countErr);

    // free memory used by gengetopt
    cmdline_parser_free(&args);
    // free allocated memory used by filenames
    for(int i; i < max; ++i){
        FREE(filenames[i]); 
    }
    FREE(filenames);

    return 0;
}

// function used to split lines whenever it finds a \n
void lineSplit(int max, char buffer[], char* linesPipe[]){
    // initializes linesPipe[0] with the first line
    linesPipe[0] = strtok(buffer, "\n");
    // splits the rest of the lines
    for (int i = 1; i < max; i++){
        linesPipe[i] = strtok(NULL, "\n");
    }
}

// function used to check whether or not the extension matches the file type
int extMatch(int pos, char* ext, char* token, char ** file){
    if(strncmp(ext + 1, token, (sizeof(char) * 6)) == 0 || strcmp(token, "jpeg") == 0 || strcmp(token, "jpg") == 0){
    // ext + 1 advances the pointer one char to cut the "."
        printf("[OK] '%s': extension '%s' matches file type '%s'\n", file[pos], ext + 1, token);
        return 1;
    }else{
        printf("[MISMATCH] '%s': extension is '%s', file type is '%s'\n", file[pos], ext + 1, token);
        return 0;
    }
}

// function used to count the number of lines in a batch file
int lineCounter(char * batchFile){
    FILE *fp;
    // starts at one line
    int countLines = 1;  
    // Check if file exists
    if ((fp = fopen(batchFile, "r")) == NULL){
        ERROR(ERR_OPEN_FILE, "cannot open file '%s', args.batch_arg");
    }
    // getc() until it hits EOF, increments counter if it finds a "\n"
    while (!feof(fp)){
        char c = getc(fp);
		if(c == '\n'){
            countLines++;
        }
    }	
    if(fclose(fp) != 0){
        ERROR(ERR_CLOSE_FILE, "cannot close file '%s', args.batch_arg");
    }
    // checks if batch file has no lines
    if(countLines == 1){
        errno = EINVAL;
        ERROR(ERR_COUNT_LINES, "batch file has no lines");
    }
    return countLines;
}

void signalHandler(int signal, siginfo_t *siginfo, void * context){
    (void)context;
    int errAux;
    // copies global variable errno
    errAux = errno;

    if(signal == SIGQUIT){
        printf("Captured SIGQUIT signal (sent by PID: %ld). Use SIGINT to terminate the application.\n", (long)siginfo->si_pid);
    }
    if(signal == SIGUSR1){
        printf("Captured SIGUSR1 signal.\n");
        printf("Started processing at: %d.%d.%d_%dh%d:%d\n", sigTimeInfo.tm_year + 1900, sigTimeInfo.tm_mon + 1, sigTimeInfo.tm_mday, sigTimeInfo.tm_hour, sigTimeInfo.tm_min, sigTimeInfo.tm_sec);
        printf("Processing file number '%d' -- '%s'.\n", sigFileNum, sigFile);
    }
    errno = errAux;
}