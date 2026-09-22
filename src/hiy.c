/* This is the only file you will be editing and submit.
 * - Copyright of Starter Code: Profs. Ivan Avramovic, Yutao Zhong, and Hamza Mughal 
 *                              of George Mason University.  All Rights Reserved
 * - Copyright of Student Code: You!  
 * - Restrictions on Student Code: Do not post your code on any public site (eg. Github).
 * -- Use of a PRIVATE Git repository (e.g. GMU's GitLab server) is strongly recommended!
 * -- You may post your code on a PRIVATE repository to give interviewers access to it.
 * -- You are liable for the protection of your code from others.
 * - Date: Mar 2025
 */

/* CS367 Project 3, Spring 2025
 * Name: Juan Carlos Garcia Solis
 */

#include <sys/wait.h>
#include "hiy.h"
#include "parse.h"
#include "util.h"
#include "logging.h"

/* Constants */
#define DEBUG 0 /* You can set this to 0 to turn off the debug parse information */
#define STOP_SHELL  0
#define RUN_SHELL   1

/*.===============================================.
 *| Uncomment the lines below if you would like to use them:
 *+===============================================*/



static const char *task_path[] = { "./", "/usr/bin/", NULL };
static const char *instructions[] = { "quit", "help", "list", "delete", "start", "startbg", "kill", "suspend", "fg", "bg", "pipe", NULL};


#define NUM_PATHS        ( sizeof(task_path)/sizeof(const char*) - 1 )
#define NUM_INSTRUCTIONS ( sizeof(instructions)/sizeof(const char*) - 1 )



/*-------------------------------------------*/
/*  The entry of your task manager program   */
/*-------------------------------------------*/

//Struct for taskNode, to be put in a linked list for tasks
typedef struct taskNode {
    pid_t pid;            // pid given to task
    char **argv;          // argv
    char *cmd;            // Command entered
    int taskNumber;       // Number given to task
    int state;            // State of task
    int log_state;        //Logging state
    int exit_Code;        //Exit code of task
    struct taskNode *next; // Pointer to next Process Node in a linked list
} taskNode;

//Struct for linked-list of nodes, for task list
typedef struct taskQueue {
    taskNode *head; // Points to first task.
} taskQueue;

taskQueue *myTaskList;

//Used to initialize List of Tasks
taskQueue *initializeTaskList(){
    //Check if mem allocation failed
    taskQueue * daList = malloc(sizeof(taskQueue));
    if (daList == NULL){
        return NULL;
    }
    daList->head = NULL;
    return daList;
}

//Used to Setup a new taskNode
taskNode *createTaskNode(int taskNumber, pid_t pid, char *argv[MAXARGS + 1], char *cmd, int state, int exitCode, int logState){
    taskNode *task = malloc(sizeof(taskNode));
    //Check if mem allocation failed
    if (task == NULL){
        return NULL;
    }
    task->argv = clone_argv(argv);
    //Check if mem allocation failed
    if (task->argv == NULL){
        free(task);
        return NULL;
    }
    task->cmd = malloc(sizeof(char) * MAXLINE + 1);
    //Check if mem allocation failed
    if (task->cmd == NULL){
        free(task);
        return NULL;
    }
    //Copy CMD into TaskNode's cmd
    strncpy(task->cmd, cmd, strlen(cmd) + 1);
    //Set remaining values
    task->taskNumber = taskNumber;
    task->pid = pid;
    task->state = state;
    task->exit_Code = exitCode;
    task->log_state = logState;
    task->next = NULL;
    return task;
}

//Adds a task to the List
int addTaskToList(taskNode *task, taskQueue *taskList){
    //If encountered error return -1
    if (task == NULL || taskList == NULL){return -1;}

    //If list is empty make node head
    if (taskList->head == NULL){
        taskList->head = task;
    } else {
        taskNode *current = taskList->head;
        while (current->next != NULL){
            current = current->next;
        }
        current->next = task;
    }
    return 0;
}

//Checking if the userCMD is not a built-in cmd to then add it to the task list
int notBuiltInCMD(char *userCMD){
    for (int i = 0; instructions[i] != NULL; i++) {
        if (strcmp(instructions[i], userCMD) == 0){
            return 0;
        }
    }
    return 1;
}


//Used to free TaskList
void freeTaskList(taskQueue *queue){
    if (queue == NULL) return;
    taskNode *current = queue->head;

    while (current != NULL){
        taskNode *next = current->next;
        free(current);
        current = next;
    }
    free(queue);
}

//Used to find the pid and returns the taskNode holding it
taskNode *findPid(pid_t pid){
    /*in case empty*/
    if (myTaskList->head == NULL){
        return NULL;
    }
    taskNode *current = myTaskList->head;

    while(current != NULL && current->pid != pid && current->state != STATE_RUN_BG){
        current = current->next;
    }
    if (current == NULL){
        return NULL;
    }
    return current;
}


// Signal handler for SIGCHLD takes care of termination of background child's
void sigchld_handler() {
    int status;
    pid_t pid;

    // Reap all terminated child processes without blocking
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        //Used to find the task node
        taskNode *current = findPid(pid);

        if (current != NULL) {
            // If the child exited normally
            if (WIFEXITED(status)) {
                current->exit_Code = WEXITSTATUS(status); // Store exit code
                log_hiy_status(current->taskNumber, current->cmd, current->pid, current->state, STATE_FINISHED);
                current->state = STATE_FINISHED;
                current->log_state = LOG_STATE_FINISHED;
            }
                // If the child was terminated by a signal
            else if (WIFSIGNALED(status)) {
                current->exit_Code = WTERMSIG(status); // Store signal number
                log_hiy_status(current->taskNumber, current->cmd, current->pid, current->state, STATE_KILLED);
                current->state = STATE_KILLED;
                current->log_state = LOG_STATE_KILLED;
            }
        }
    }
}



void startTask(taskQueue *list, int taskNumber, char *inFile, char *outFile) {
    // Check if list is null or if invalid taskNumber is given
    if (!list || taskNumber <= 0) {
        log_hiy_task_num_error(taskNumber);
        return;
    }

    // Searching list for task using task number/id
    taskNode *current = list->head;
    while (current && current->taskNumber != taskNumber) {
        current = current->next;
    }

    // If the task was not found log error
    if (!current) {
        log_hiy_task_num_error(taskNumber);
        return;
    }

    // If task is running or in a suspended state log error
    if (current->state == LOG_STATE_RUN_FG || current->state == LOG_STATE_RUN_BG || current->state == LOG_STATE_SUSPENDED) {
        log_hiy_status_error(taskNumber, current->state);
        return;
    }

    // Fork a new process to execute the task
    pid_t pid = fork();
    if (pid == 0) {
        // Child Process
        setpgid(0, 0);

        // Input redirection
        if (inFile) {
            int inFDescriptor = open(inFile, O_RDONLY);
            if (inFDescriptor < 0) {
                log_hiy_file_error(taskNumber, inFile);
                exit(1);
            }
            log_hiy_redir(taskNumber, LOG_REDIR_IN, inFile);
            dup2(inFDescriptor, STDIN_FILENO);
            close(inFDescriptor);
        }

        // Output redirection
        if (outFile) {
            int outFDescriptor = open(outFile, O_WRONLY | O_CREAT | O_TRUNC, 0600);
            if (outFDescriptor < 0) {
                log_hiy_file_error(taskNumber, outFile);
                exit(1);
            }
            log_hiy_redir(taskNumber, LOG_REDIR_OUT, outFile);
            dup2(outFDescriptor, STDOUT_FILENO);
            close(outFDescriptor);
        }

        // Try first file path
        char filePath[256];
        snprintf(filePath, sizeof(filePath), "%s%s", task_path[0], current->argv[0]);

        if (execv(filePath, current->argv) == -1) {
            // Try second path
            char directFPath[256];
            snprintf(directFPath, sizeof(directFPath), "%s%s", task_path[1], current->argv[0]);
            if (execv(directFPath, current->argv) == -1) {
                log_hiy_start_error(current->cmd);
                exit(1);
            }
        }
    } else if (pid > 0) {
        // Parent process
        current->pid = pid;
        log_hiy_status(taskNumber, current->cmd, current->pid, current->state, STATE_RUN_FG);
        current->state = LOG_STATE_RUN_FG;

        // Wait for child
        int waitStatus;
        waitpid(current->pid, &waitStatus, 0);

        // Check if exited normally
        if (WIFEXITED(waitStatus)) {
            current->exit_Code = WEXITSTATUS(waitStatus);
        } else {
            current->exit_Code = -1;
        }

        log_hiy_status(taskNumber, current->cmd, current->pid, current->state, STATE_FINISHED);
        current->log_state = LOG_STATE_FINISHED;
        current->state = STATE_FINISHED;
    }
}

void startbg(taskQueue *list, int taskNumber, char *inFile, char *outFile) {
    // Check if list is null or if invalid taskNumber is given
    if (!list || taskNumber <= 0) {
        log_hiy_task_num_error(taskNumber);
        return;
    }

    // Searching list for task using task number/id
    taskNode *current = list->head;
    while (current && current->taskNumber != taskNumber) {
        current = current->next;
    }

    // If the task was not found log error
    if (!current) {
        log_hiy_task_num_error(taskNumber);
        return;
    }

    // If task is running or in a suspended state log error
    if (current->state == LOG_STATE_RUN_FG || current->state == LOG_STATE_RUN_BG || current->state == LOG_STATE_SUSPENDED) {
        log_hiy_status_error(taskNumber, current->state);
        return;
    }

    // Fork a new process to execute the task
    pid_t pid = fork();
    if (pid == 0) {
        // Child Process
        setpgid(0, 0);

        // Input redirection
        if (inFile) {
            int inFDescriptor = open(inFile, O_RDONLY);
            if (inFDescriptor < 0) {
                log_hiy_file_error(taskNumber, inFile);
                exit(1);
            }
            log_hiy_redir(taskNumber, LOG_REDIR_IN, inFile);
            dup2(inFDescriptor, STDIN_FILENO);
            close(inFDescriptor);
        }

        // Output redirection
        if (outFile) {
            int outFDescriptor = open(outFile, O_WRONLY | O_CREAT | O_TRUNC, 0600);
            if (outFDescriptor < 0) {
                log_hiy_file_error(taskNumber, outFile);
                exit(1);
            }
            log_hiy_redir(taskNumber, LOG_REDIR_OUT, outFile);
            dup2(outFDescriptor, STDOUT_FILENO);
            close(outFDescriptor);
        }

        // Execution of file paths
        char filePath[256];
        snprintf(filePath, sizeof(filePath), "%s%s", task_path[0], current->argv[0]);

        if (execv(filePath, current->argv) == -1) {
            char directFPath[256];
            snprintf(directFPath, sizeof(directFPath), "%s%s", task_path[1], current->argv[0]);
            if (execv(directFPath, current->argv) == -1) {
                log_hiy_start_error(current->cmd);
                exit(1);
            }
        }
    } else if (pid > 0) {
        // Parent Process continuing
        current->pid = pid;
        log_hiy_status(taskNumber, current->cmd, current->pid, current->state, STATE_RUN_BG);
        current->state = STATE_RUN_BG;
        current->log_state = LOG_STATE_RUN_BG;
    }
}

void killTask(taskQueue *list, int taskNumber) {
    if (!list || taskNumber <= 0) {
        log_hiy_task_num_error(taskNumber);
        return;
    }

    // Searching list for task using task number/id
    taskNode *current = list->head;
    while (current && current->taskNumber != taskNumber) {
        current = current->next;
    }

    // If the task was not found log error
    if (!current) {
        log_hiy_task_num_error(taskNumber);
        return;
    }

    // If task is running in FG or BG, send SIGINT
    if (current->state == STATE_RUN_FG || current->state == STATE_RUN_BG) {
        log_hiy_sig_sent(LOG_CMD_KILL, taskNumber, current->pid);
        kill(current->pid, SIGINT);
        return;
    }

    log_hiy_status_error(taskNumber, current->state);
}

void suspendTask(taskQueue *list, int taskNumber) {
    if (!list || taskNumber <= 0) {
        log_hiy_task_num_error(taskNumber);
        return;
    }

    // Searching list for task using task number/id
    taskNode *current = list->head;
    while (current && current->taskNumber != taskNumber) {
        current = current->next;
    }

    // If the task was not found log error
    if (!current) {
        log_hiy_task_num_error(taskNumber);
        return;
    }

    // Exclude idle, finished, or killed processes
    if (current->state == STATE_READY || current->state == STATE_FINISHED || current->state == STATE_KILLED) {
        log_hiy_status_error(taskNumber, current->state);
        return;
    }

    // Send signal to suspend task
    log_hiy_sig_sent(LOG_CMD_KILL, taskNumber, current->pid);
    kill(current->pid, SIGTSTP);

    // Update state and log
    log_hiy_status(taskNumber, current->cmd, current->pid, current->state, STATE_SUSPENDED);
    current->state = STATE_SUSPENDED;
    current->log_state = LOG_STATE_SUSPENDED;
}

void fgTask(taskQueue *list, int taskNumber) {
    if (!list || taskNumber <= 0) {
        log_hiy_task_num_error(taskNumber);
        return;
    }

    // Searching list for task using task number/id
    taskNode *current = list->head;
    while (current && current->taskNumber != taskNumber) {
        current = current->next;
    }

    // If the task was not found log error
    if (!current) {
        log_hiy_task_num_error(taskNumber);
        return;
    }

    // If already running in foreground
    if (current->state == STATE_RUN_FG) {
        log_hiy_status_error(taskNumber, STATE_RUN_FG);
        return;
    }

    // If ready/finished/killed then start fresh in foreground
    if (current->state == STATE_READY || current->state == STATE_FINISHED || current->state == STATE_KILLED) {
        startTask(myTaskList, taskNumber, NULL, NULL);
        return;
    }

    // Resume a suspended task
    if (current->state == STATE_SUSPENDED) {
        log_hiy_sig_sent(LOG_CMD_RESUME, taskNumber, current->pid);
        kill(current->pid, SIGCONT);
        log_hiy_status(taskNumber, current->cmd, current->pid, current->state, STATE_RUN_FG);
        current->state = STATE_RUN_FG;
        current->log_state = LOG_STATE_RUN_FG;

        int waitStatus;
        waitpid(current->pid, &waitStatus, 0);

        if (WIFEXITED(waitStatus)) {
            current->exit_Code = WEXITSTATUS(waitStatus);
        } else {
            current->exit_Code = -1;
        }

        log_hiy_status(taskNumber, current->cmd, current->pid, current->state, STATE_FINISHED);
        current->log_state = LOG_STATE_FINISHED;
        current->state = STATE_FINISHED;
        return;
    }

    // If currently running in background
    if (current->state == STATE_RUN_BG) {
        log_hiy_status(taskNumber, current->cmd, current->pid, current->state, STATE_RUN_FG);

        int waitStatus;
        waitpid(current->pid, &waitStatus, 0);

        if (WIFEXITED(waitStatus)) {
            current->exit_Code = WEXITSTATUS(waitStatus);
        } else {
            current->exit_Code = -1;
        }

        log_hiy_status(taskNumber, current->cmd, current->pid, current->state, STATE_FINISHED);
        current->log_state = LOG_STATE_FINISHED;
        current->state = STATE_FINISHED;
        return;
    }
}

void bgTask(taskQueue *list, int taskNumber) {
    if (!list || taskNumber <= 0) {
        log_hiy_task_num_error(taskNumber);
        return;
    }

    // Searching list for task using task number/id
    taskNode *current = list->head;
    while (current && current->taskNumber != taskNumber) {
        current = current->next;
    }

    // If the task was not found log error
    if (!current) {
        log_hiy_task_num_error(taskNumber);
        return;
    }

    // running in background already
    if (current->state == STATE_RUN_BG) {
        log_hiy_status_error(taskNumber, STATE_RUN_BG);
        return;
    }

    // If in ready/finished/killed state then start in background
    if (current->state == STATE_READY || current->state == STATE_FINISHED || current->state == STATE_KILLED) {
        startbg(myTaskList, taskNumber, NULL, NULL);
        return;
    }

    // Resume suspended task in background
    if (current->state == STATE_SUSPENDED) {
        log_hiy_sig_sent(LOG_CMD_RESUME, taskNumber, current->pid);
        kill(current->pid, SIGCONT);
        log_hiy_status(taskNumber, current->cmd, current->pid, current->state, STATE_RUN_BG);
        current->state = STATE_RUN_BG;
        current->log_state = LOG_STATE_RUN_BG;
        return;
    }

    // running in foreground already
    if (current->state == STATE_RUN_FG) {
        log_hiy_status_error(taskNumber, STATE_RUN_FG);
        return;
    }
}


int main() {
    char *cmd = NULL;
    int do_run_shell = RUN_SHELL;

    /* Initial Prompt and Welcome */
    log_hiy_intro();
    log_hiy_help();

    //Space to initialize my implementations
    myTaskList = initializeTaskList();
    int taskNumber = 0;
    int totalTasks = 0;
    struct sigaction sa = {
            .sa_handler = sigchld_handler,
            .sa_flags = SA_RESTART | SA_NOCLDSTOP
    };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGCHLD, &sa, NULL);
    //End of initialization of my-stuff

    /* Shell looping here to accept user command and execute */
    while (do_run_shell == RUN_SHELL) {
        char *argv[MAXARGS+1] = {0};        /* Argument list */
        Instruction inst = {0};           /* Instruction structure: check parse.h */

        /* Print prompt */
        log_hiy_prompt();
        
        /* Get Input - Allocates memory for the cmd copy */
        cmd = get_input(); 
        /* If the input is whitespace/invalid, get new input from the user. */
        if(cmd == NULL) {
          continue;
        }
        
        /* Parse the Command and Populate the Instruction and Arguments */
        initialize_command(&inst, argv);    /* initialize arg lists and instruction */
        parse(cmd, &inst, argv);            /* call provided parse() */

        if (DEBUG) {  /* display parse result, redefine DEBUG to turn it off */
          debug_print_parse(cmd, &inst, argv, "Main, after parse");
        }

        /* After parsing: your code to continue from here */
        /*.===============================================.
         *| - The command has been parsed and you have cmd, inst, and argv filled with data
         *| - Very highly recommended to start calling your own functions after this point.
         *+===============================================*/
            

        /*==BUILT_IN: quit===*/
        if (strcmp(inst.instruct, "quit") == 0){
          do_run_shell = STOP_SHELL;  /*set the main loop to exit when you finish processing it */

          /*.===============================================.
           *| You will need to print a message when quit is entered, based on the instructions
           *| in the project spec. You can do that here; in your functions; or at the end of main.
           *+===============================================*/
          log_hiy_quit();
        }

        /*==BUILT_IN: help===*/
        if (strcmp(inst.instruct, "help") == 0){
            log_hiy_help();
        }

        /*==BUILT_IN: list===*/
        if (strcmp(inst.instruct, "list") == 0){
            log_hiy_num_tasks(totalTasks);
            if (myTaskList->head == NULL){
//                return 0;
                continue;
            }

            taskNode *head = myTaskList->head;
            while (head != NULL){
                log_hiy_task_info(head->taskNumber, head->cmd, head->log_state, head->pid, head->exit_Code);
                head = head->next;
            }
        }

        /*==BUILT_IN: delete===*/
        if (strcmp(inst.instruct, "delete") == 0){
            //When list is empty and trying to delete give error
            if (myTaskList->head == NULL){
                log_hiy_task_num_error(inst.num);
                continue;
            }
            //pointers used to go through myList
            taskNode *current = myTaskList->head;
            taskNode *prev = NULL;

            //Go through list to find wanted task
            while(current != NULL && current->taskNumber != inst.num){
                prev = current;
                current = current->next;
            }

            //If task was not found give error
            if (current == NULL){
                log_hiy_task_num_error(inst.num);
                continue;
            }

            //If task is in a conflicting state cannot delete it, give error
            if (current->log_state == LOG_STATE_RUN_BG ||
                current->log_state == LOG_STATE_RUN_FG ||
                current->log_state == LOG_STATE_SUSPENDED){
                log_hiy_status_error(inst.num, current->log_state);
                continue;
            }

            //Log deletion
            log_hiy_delete(inst.num);
            //Decrement total tasks
            totalTasks--;

            //Remove node from task node from myList.
            if (prev != NULL){
                prev->next = current->next;
            } else {
                myTaskList->head = current->next;
            }

            //Free space allocated for cmd and node
            free(current->cmd);
            free(current);
        }

        /*==BUILT_IN: start===*/
        if (strcmp(inst.instruct, "start") == 0){
            startTask(myTaskList,inst.num, inst.infile, inst.outfile);
        }
        /*==BUILT_IN: startbg===*/
        if (strcmp(inst.instruct, "startbg") == 0){
            startbg(myTaskList,inst.num, inst.infile, inst.outfile);
        }

        /*==BUILT_IN: kill===*/
        if (strcmp(inst.instruct, "kill") == 0){
            killTask(myTaskList, inst.num);
        }

        /*==BUILT_IN: suspend===*/
        if (strcmp(inst.instruct, "suspend") == 0){
            suspendTask(myTaskList, inst.num);
        }

        /*==BUILT_IN: fg===*/
        if (strcmp(inst.instruct, "fg") == 0){
            fgTask(myTaskList, inst.num);
        }
        /*==BUILT_IN: bg===*/
        if (strcmp(inst.instruct, "bg") == 0){
            bgTask(myTaskList, inst.num);
        }

        /*==BUILT_IN: pipe===*/
        if (strcmp(inst.instruct, "bg") == 0){
            continue;
        }

        /*==NOT_BUILT_IN==*/
        if (notBuiltInCMD(inst.instruct)){
            taskNode *task = createTaskNode(++taskNumber, 0, argv, cmd, STATE_READY, 0, LOG_STATE_READY);
            addTaskToList(task,myTaskList);
            log_hiy_task_init(task->taskNumber, task->cmd);
            totalTasks++;
        }


        /*.===============================================.
         *| After your code: We cleanup before Looping to the next command.
         *| free_command WILL free the cmd, inst and argv data.
         *| Make sure you've COPIED ANY INFORMATION YOU NEED first.
         *| Hint: You can use the util.c functions for copying this information.
         *+===============================================*/


        free_command(cmd, &inst, argv);

        cmd = NULL;
    }  // end while
    freeTaskList(myTaskList);

    return 0;
}  // end main()

