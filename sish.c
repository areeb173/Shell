#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_SIZE 100 // maximum size of array

typedef struct Node {
    char* command;
    struct Node* next;
}Node;

// parses command and stores command and arguments in args
void tokenize(char *line, char *args[], const char *delim){
    int i = 0;
    // tokenize
    char *token = strtok(line, delim);
    while(token){
        args[i] = token;
        token = strtok(NULL, delim);
        i++;
    }
}

// creates and adds new node to end of list
void addNode(Node **headPtr, char* data){
    // allocate memory for new node
    Node *newNode = (Node*)malloc(sizeof(Node));
    if(!newNode){
        perror("MEMORY ALLLOCATION FAILED\n");
        return;
    }

    // store command in node
    newNode -> command = strdup(data);
    newNode -> next = NULL;
    
    // if list is empty, set newNode to head
    if(!*headPtr){
        *headPtr = newNode;
        return;
    }

    // traverse to last node node
    Node* last = *headPtr;  // points to last node
    while(last->next){
        last = last->next;
    }
    // append node to end of the list
    last->next = newNode;
}

// prints history list
void printHistory(Node *head){
    Node* temp = head;
    int offset = 0;
    // iterate through list and print offset/command of each node
    while(temp){
        printf("%d %s", offset, temp->command);
        temp = temp->next;
        offset++;
    }
}

// clears history list and frees memory
void clearList(Node **head){
    Node *current = *head;
    Node *next;
    while(current){
        next = current -> next;
        free(current);
        current = next;
    }
    *head = NULL;
}

// function that removes the head of the list
// sets head to 2nd node
void removeHead(Node **head){
    // store reference to the current head to be deleted
    Node* temp = *head;
    // update the head pointer to point to the next node
    *head = (*head)->next;
    // free the memory
    free(temp);
}

// function that executes a command
void executeCommand(char *commands[], char *args[]) {
    // get the number of commands
    int numCommands = 0;
    while (commands[numCommands]) {
        numCommands++;
    }

    // declare file descriptors for the first pipe
    int prevFd[2];

    // Iterate through each command
    for (int i = 0; i < numCommands; i++) {
        int currFd[2]; // declare file descriptors for current pipe

        // create the pipe
        if (pipe(currFd) == -1) {
            perror("ERROR CREATING PIPE\n");
            return;
        }

        // fork a child process
        int pid = fork();
        if (pid < 0) {
            perror("ERROR FORKING\n");
            return;
        }

        // child process
        if (pid == 0) {
            // redirect input from the previous pipe if not on the first command
            if (i > 0) {
                dup2(prevFd[0], STDIN_FILENO);
                close(prevFd[0]); // close the read end
                close(prevFd[1]); // close the write end
            }

            // redirect output to the current pipe if not on the last command
            if (i < numCommands - 1) {
                dup2(currFd[1], STDOUT_FILENO);
                close(currFd[0]); // close the read end
                close(currFd[1]); // close the write end
            }

            // tokenize the current command
            memset(args, 0, sizeof(args[0]) * MAX_SIZE); // clear args[]
            tokenize(commands[i], args, " \n");

            // execute the command
            if (execvp(args[0], args) == -1) {
                perror("ERROR EXECUTING\n");
                exit(1); // exit child process on execution failure
            }
        }

        // close the previous pipe in the parent process (except for the first command)
        if (i != 0) {
            close(prevFd[0]); // close the read end
            close(prevFd[1]); // close the write end
        }

        // current pipe becomes previous pipe for the next iteration
        prevFd[0] = currFd[0];
        prevFd[1] = currFd[1];
    }

    // wait for all child processes to finish
    for (int i = 0; i < numCommands; i++) {
        wait(NULL);
    }
}

int main(){
    const char *argDelim = " \n"; // delimeter for arguments of a command
    const char *commandDelim = "|\n"; // // delimeter for commands in an input

    char *line;
	size_t bufsize = 0;
    char *args[MAX_SIZE] = {NULL}; // initialize all values to NULL
    char *commands[MAX_SIZE] = {NULL}; // initialize all values to NULL
    // initialize list
    Node *head = NULL;
    int historySize = 0;

    // input loop
    while(1){
        printf("sish> "); // input prompt

        // get user input
		if(getline(&line, &bufsize, stdin) == -1){
            perror("ERROR READING LINE\n");
            break;
        }

        // add command to history list
        addNode(&head, line);
        historySize++;
        if(historySize >= MAX_SIZE){
            removeHead(&head);
        }

        // parse the input string into commands and args
        char *lineCopy = strdup(line);
        tokenize(lineCopy, commands, commandDelim);
        tokenize(line, args, argDelim);

        // break from input loop if user entered 'exit'
        if(strcmp(args[0], "exit") == 0 || strcmp(line, "exit\n") == 0){
            break;
        }

        // user input cd command
        if(strcmp(args[0], "cd") == 0){
            if(chdir(args[1]) != 0){    // call chdir system call
                perror("ERROR CHANGING DIRECTORY\n");
            }
        }
        // user input history command
        else if(strcmp(args[0], "history") == 0){
            // check if there is an argument
            if(args[1]){
                // argument is -c
                if(strcmp(args[1], "-c") == 0){
                    // clear history list
                    clearList(&head);
                }
                // argument is offset
                else{
                    // navigate to node at offset
                    int offset = atoi(args[1]);
                    Node *current = head;
                    int i = 0;

                    while (current && i < offset) {
                        current = current->next;
                        i++;
                    }

                    if(current){
                        // clear args[] and commands[]
                        memset(args, 0, sizeof(args));
                        memset(commands, 0, sizeof(commands));
                        // tokenize command at offset
                        char *commandCopy = strdup(current->command);
                        tokenize(commandCopy, commands, commandDelim);
                        tokenize(current->command, args, argDelim);
                        // execute command
                        executeCommand(commands, args);
                    }
                    else {
                        // error message
                        perror("Invalid Offset\n");
                    }
                }
            }
            else{
                // print history
                printHistory(head);
            }
        }
        // user input piped command
        else{
            executeCommand(commands, args);
        }
        // clear args[] and commands[]
        memset(args, 0, sizeof(args));
        memset(commands, 0, sizeof(commands));
    }
    // free allocated memory
    clearList(&head);
    free(line);

    return 0;
}