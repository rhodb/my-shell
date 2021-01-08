#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <wait.h>
#include <fcntl.h>

#define MAXLINE 514
#define MAXARG  15
#define MAXPROC  514

char error_message[30] = "An error has occurred\n";

void myPrint(char *msg)
{
    write(STDOUT_FILENO, msg, strlen(msg));
}

char* skipSpaces(char *string) {

    while(*string && ((*string == ' ') || (*string == '\t'))) {
        string++;
    }
    return string;    
}


char* trimJunk(char *string)
{
    char *front = string;

    int cnt = 0;
    int len = strlen(string);
    string += (len-1);
  
    while(*string && (*string == ' ' || *string == '\t' || *string == '\n')) {
        string--;
        cnt++;
    }
    
    if (cnt < len) {
        *(string+1) = '\0';
        string = front;
    }
    else {
    string = front;
    *string = '\0';
    }

    return string;
}

char* cleanString(char *string)
{
    string = skipSpaces(string);
    string = trimJunk(string);
    return string;

}


void printArgs(char **argv)
{
    int i = 0;
    while(argv[i]) {
        myPrint(argv[i++]);
        myPrint("\n");
    }
}

void cd(char **argv)
{
    if (!argv[1]) {
        chdir(getenv("HOME"));
    }
    else if (!(strcmp(argv[1], ".."))) {
        char pathbuff[MAXLINE];
        getcwd(pathbuff, MAXLINE);
        int i = strlen(pathbuff);
        while(pathbuff[i] != '/') i--;
        if (!i) pathbuff[1] = '\0';
        else pathbuff[i] = '\0';
        chdir(pathbuff); 
    }
    else if (argv[2]) {
        write(STDOUT_FILENO, error_message, strlen(error_message));
        return;
    }
    else {

        if (!(strncmp(argv[1], "../", 3))) {
            int i = 3;
            while (*argv[1]+i == '/') i++;

            char pathbuff[MAXLINE];
            getcwd(pathbuff, MAXLINE);
            int j = strlen(pathbuff);
            while(pathbuff[j] != '/') j--;
            if (!j) pathbuff[1] = '\0';
            else pathbuff[j] = '\0';
            
            chdir(pathbuff);
            
            char newarg[strlen(argv[1])];
            strcpy(newarg, argv[1]);
            argv[1] = &(newarg[i]);

            if (*argv[1]) {
                cd(argv);
            }
        }
        else if (!(strncmp(argv[1], "./", 2))) {
            int i = 2; 
            while (*argv[1]+i == '/') i++;
            char newarg[strlen(argv[1])];
            strcpy(newarg, argv[1]);
            argv[1] = &(newarg[i]);

            if (*argv[1]) {
                cd(argv);
            }
        }
        else {
            if ((chdir(argv[1])) < 0) {
                write(STDOUT_FILENO, error_message, strlen(error_message));
                return;
            }
        }            
    }
}



void pwd(char *pathbuff, char **argv)
{

    if (argv[1]) {
        write(STDOUT_FILENO, error_message, strlen(error_message));
    }
    else {
        getcwd(pathbuff, MAXLINE);
        myPrint(pathbuff);
        myPrint("\n");
    }
}


int isbuiltin(char **argv)
{
    if (!strcmp(argv[0], "exit")) {
        if (argv[1]) {
            write(STDOUT_FILENO, error_message, strlen(error_message));
            return -1;
        }
        exit(0);
    }
    
    if (!strcmp(argv[0], "cd")) {
        cd(argv);
        return 1;
    }

    if (!(strcmp(argv[0], "pwd"))) {
        char path[MAXLINE];
        pwd(path, argv);
        return 1;
    } 
    return 0;
}


void longString(char* bad_input, FILE *stream)
{
    char buff[100];
    char *binput;
    int extra = 100;

    if (stream == stdin) {
        myPrint(bad_input);
        myPrint("\n");
        write(STDOUT_FILENO, error_message, strlen(error_message));
        while(extra > 98) {
            binput = fgets(buff, 100, stream);
            extra = strlen(binput);
        }
    }
    else {
        while(extra > 98) {
            binput = fgets(buff, 100, stream);
            write(STDOUT_FILENO, binput, strlen(binput));
            extra = strlen(binput);
        }
        write(STDOUT_FILENO, error_message, strlen(error_message));
    }
}


void parseprocess(char *cmd_buff, char **argv)
{
    char *token;
    int argc = 0;

    cmd_buff = cleanString(cmd_buff);
    char *nexttoken = cmd_buff;

    if (*cmd_buff == '\0') {
        argv[0] = NULL;
        return;
    }

    while((token = strtok_r(nexttoken, " \t", &nexttoken))) {
        token = cleanString(token);
        argv[argc++] = token;
    }
    argv[argc] = NULL;

}


void writeBeginning(int fd, char **argv)
{
    pid_t pid; 
    int status;

    off_t oldendpos = lseek(fd, 0, SEEK_END);
    char buff[oldendpos];
    lseek(fd, 0, SEEK_SET);
    read(fd, buff, oldendpos);
    lseek(fd, 0, SEEK_SET);

    if (!(pid = fork())) {
        dup2(fd, 1);
        if (!isbuiltin(argv)) {
            if ((execvp(argv[0], argv)) < 0) {
                write(STDOUT_FILENO, error_message, strlen(error_message));
                exit(0);
            }
        }
    }
    while(pid > 0) {
        pid = waitpid(-1, &status, 0);
    }

    write(fd, buff, oldendpos);

}


void execRedirection(char *process, char *file, int type)
{
    if (!file) {
        write(STDOUT_FILENO, error_message, strlen(error_message));
        return;
    }

    char *argv[MAXARG];

    parseprocess(process, argv);
    if (!strcmp(argv[0], "cd") || !strcmp(argv[0], "pwd") || !strcmp(argv[0], "exit")) {
        write(STDOUT_FILENO, error_message, strlen(error_message));
        return;
    }

    int fd1 = open(file, O_RDWR);    
    if (!type && (fd1 > 0)) {
        close(fd1);
        write(STDOUT_FILENO, error_message, strlen(error_message));
        return;
    }
    else {
        close(fd1);
        int fd2;
        if ((fd2 = open(file, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP)) < 0) {
            close(fd2);
            write(STDOUT_FILENO, error_message, strlen(error_message));
            return;
        }
        else {
            writeBeginning(fd2, argv);
            close(fd2);
            return;
        }
    } 
}


int isRedirection(char *process)
{
    if (*process == '\0') return -1;

    process = cleanString(process);
    if (process[strlen(process)-1] == '>') {
        write(STDOUT_FILENO, error_message, strlen(error_message));
        return -1;
    }
    
    
    char *RDargs[2];

    char *RDprocbuff = malloc(strlen(process)+1);
    strcpy(RDprocbuff, process);
    
    char *token;
    char *start;
    char *nexttoken;

    int rd = 0;
    int adrd = 0;
    start = RDprocbuff;
    nexttoken = RDprocbuff;

    while((token = strtok_r(nexttoken, ">", &nexttoken))) {
        rd++;
        if (rd > 0 && *token == '+') {
            token++;
            token = cleanString(token);
            if (*token == '\0') {
                write(STDOUT_FILENO, error_message, strlen(error_message));
                return -1;
            }
            RDargs[rd-1] = token;
            adrd++;
        }
        else {
            token = cleanString(token);
            
            if (*token == '\0' || rd > 2) {
                write(STDOUT_FILENO, error_message, strlen(error_message));
                return -1;
            }
            else RDargs[rd-1] = token;
        }
    }
    RDprocbuff = start;
  
    if (rd > 2) {
        write(STDOUT_FILENO, error_message, strlen(error_message));
        free(RDprocbuff);
        return -1;
    }
    else if (rd == 1) {
        free(RDprocbuff);
        return 0;
    }
    else {
        if (adrd) execRedirection(RDargs[0], RDargs[1], 1);
        else (execRedirection(RDargs[0], RDargs[1], 0));
        free(RDprocbuff); 
        return 1;
    }
}


void execLine(char *process)
{
    char *argv[MAXARG];
    pid_t pid;
    int status;

    if (!isRedirection((process))) {
        parseprocess(process, argv);
        if (!argv[0]) return;
        if (!isbuiltin(argv)) {
            if (!(pid = fork())) {
                if (execvp(argv[0], argv) < 0) {
                    write(STDOUT_FILENO, error_message, strlen(error_message));
                    exit(0);
                }
            }
            while(pid > 0) pid = waitpid(-1, &status, 0);
        }
    }
}


struct proc_list {
    char *process;
    struct proc_list *next;
};

typedef struct proc_list *job;

job add_proc(job currentjob, char *prc)
{
    job tmp;
    tmp = currentjob;
    while(tmp->next) {
        tmp = tmp->next;
    }

    job newprc = malloc(sizeof(struct proc_list));
    newprc->process = malloc(strlen(prc)+1);
    strcpy(newprc->process, prc);
    newprc->next = NULL;
    
    tmp->next = newprc;

    return currentjob;

}

void free_job(job j)
{
    job tmp;
    while (j) {
        tmp = j;
        j = j->next;
        if (tmp->process) free(tmp->process);
        free(tmp);
    }

}

void printJob(job j) 
{
    job tmp;
    while(j) {
        tmp = j->next;
        myPrint(j->process);
        j = tmp;
    }
}

job parseline(char *line_buff)
{
    line_buff = cleanString(line_buff);
 
    if (!(strcmp(line_buff, "\n"))) {
        return NULL;
    }
 
    job j = malloc(sizeof(struct proc_list));
    j->next = NULL;
    char *token;
    char *nexttoken = line_buff;

    int alloc = 1; 
    while((token = strtok_r(nexttoken, ";", &nexttoken))) {
       if (*token == '\0'); 
       else {
           if (alloc) {
               token = cleanString(token);
               j->process = malloc(strlen(token)+1);
               strcpy(j->process, token);
               alloc = 0;
           }
           else {
               token = cleanString(token);
               add_proc(j, token);
           }
        }
    }
    return j; 
}



void  evaluateline(char *cmdline, FILE *stream)
{
    if ((strlen(cmdline) == (MAXLINE-1)) && (cmdline[MAXLINE-2] != '\n')) {
        longString(cmdline, stream);
        return;
    }

    char *buff = malloc(strlen(cmdline)+1);
    strcpy(buff, cmdline);
    
    
    job newjob = parseline(buff);
    job tmp;
    job tstep = newjob;
    while(tstep) {
        tmp = tstep->next;
        execLine(tstep->process);
        tstep = tmp;   
    }

    free(buff);
    free(newjob);
    return;

}



int main(int argc, char *argv[]) 
{
    char cmd_buff[MAXLINE];
    char *pinput;
    char *check;

    if (argc > 2) {
        write(STDOUT_FILENO, error_message, strlen(error_message));
        exit(0);
    }
    else if (argc == 2) {
        FILE* stream1 = fopen(argv[1], "r");
        if (!stream1) {
            write(STDOUT_FILENO, error_message, strlen(error_message));
            exit(0);
        }
        else{
            while ((pinput = fgets(cmd_buff, MAXLINE, stream1))) {
                check = skipSpaces(pinput);
                if (strcmp(check, "\n")) { 
                    write(STDOUT_FILENO, pinput, strlen(pinput));
                    evaluateline(pinput, stream1);
                }
            }
            fclose(stream1);
            exit(0);
        }
    }
    else {
        while (1) {
            myPrint("myshell> ");
            pinput = fgets(cmd_buff, MAXLINE, stdin);
      
            if (strcmp(pinput, "\n")) {
                evaluateline(pinput, stdin);
            }
        }
    } 
}
