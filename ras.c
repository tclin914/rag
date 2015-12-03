#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h> /* open() */
#include <fcntl.h> /* file control options */
#include <arpa/inet.h>
#include <sys/ioctl.h>

#include "fifo.h"

#define STDIN 0
#define STDOUT 1
#define STDERR 2

typedef enum { 
    e_proc = 0, 
    e_argv = 1, 
    e_stdout = 2, 
    e_stderr = 3,
    e_outfile = 4,
    e_public_out = 5,
    e_public_in = 6
} CommandType;

typedef struct Command {
    CommandType commandType;
    char *command;
    struct Command *next;
} Command;

typedef struct {
    unsigned int sockfd;
    char nickname[21];
    char ip[16];
    unsigned short port;
} ClientData;

void doprocessing(int sockfd); 
void handler(int sockfd);
int run(int id, int sockfd, int readfd, Command *command,int counter, int readfdlist[], int writefdlist[], char *commands);
void dealcommand(int id, int sockfd, char *commands);
int sockfd;

int readline(int fd,char *ptr,int maxlen);

void send_welcome(int sockfd);
void send_prompt(int sockfd);

ClientData clientdata[30];
char *envs[30];

int readfdlists[30][2000] = {0};
int writefdlists[30][2000] = {0};
int counters[30] = {0};

char *deads[30];

int main(int argc, const char *argv[])
{
    int ret = chdir("/u/gcs/103/0356100/ras/dir");

    int newsockfd, portno, clilen;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
    int n, pid;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = 5566;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        exit(1);
    }

    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    /* int one = 1; */
    /* if (ioctl(sockfd, FIONBIO, (char*)&one) < 0) { */
        /* perror("ERROR on setting non-blocking mode"); */
        /* exit(1); */
    /* } */

    fd_set rfds;
    fd_set wfds;
    fd_set afds;
    int nfds, fd;

    nfds = getdtablesize();
    FD_ZERO(&afds);
    FD_SET(sockfd, &afds);

    const char *noname = "(no name)";
    const char *me = "<-me";
    const char *title = "<ID>\t<nickname>\t<IP/port>\t<indicate me>\n";
    const char *indicate = "%d\t%s\t%s%c%hu\t%s\n";
    const char *others = "%d\t%s\t%s%c%hu\n";
    char msg_buf[2048];
    bzero(msg_buf, 2048);

    /* initialize envs */
    const char *init_env = "bin:.";
    int i;
    for (i = 0; i < 30; i++) {
        envs[i] = (char*)malloc(sizeof(char) * 6);
        memcpy(envs[i], init_env, 6);
    }

    for (i = 0; i < 30; i++) {
        clientdata[i].sockfd = 0;
    }

    int isPrompt = 0;
    init_FIFO();

    while (1) {
        memcpy(&rfds, &afds, sizeof(rfds));
        memcpy(&wfds, &afds, sizeof(wfds));
        if (select(nfds, &rfds, &wfds, (fd_set*)0, (struct timeval*)0) < 0) {
            perror("ERROR on select");
            exit(1);
        }

        if (FD_ISSET(sockfd, &rfds)) {
            int ssock;
            ssock = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);
            if (ssock < 0) {
                perror("ERROR on accept");
                exit(1);
            }
            FD_SET(ssock, &afds);
            /* set client data for who command */
            int i;
            for (i = 0; i < 30; i++) {
                if (clientdata[i].sockfd == 0) {
                    clientdata[i].sockfd = ssock;
                    memcpy(clientdata[i].nickname, noname, strlen(noname) + 1);              
                    /* memcpy(clientdata[i].ip, inet_ntoa(cli_addr.sin_addr), 16); */
                    /* clientdata[i].port = cli_addr.sin_port; */
                    memcpy(clientdata[i].ip, "CGILAB", 7);
                    clientdata[i].port = 511;
                    break;
                }
            }
            /* dup2(ssock, STDOUT); */
            /* dup2(ssock, STDERR); */
            /* logint hint */
            const char *welcome = 
            "****************************************\n"
            "** Welcome to the information server. **\n"
            "****************************************\n";
            n = write(ssock, welcome, strlen(welcome) + 1);
            /* fprintf(stdout, welcome); */
            /* fflush(stdout); */

            const char *login =  "*** User '(no name)' entered from (%s/%d). ***\n";
            sprintf(msg_buf, login, clientdata[i].ip, clientdata[i].port);            
            for (i = 0; i < 30; i++) {
                if (clientdata[i].sockfd != 0) {
                    n = write(clientdata[i].sockfd, msg_buf, strlen(msg_buf) + 1);
                    /* dup2(clientdata[i].sockfd, STDOUT); */
                    /* dup2(clientdata[i].sockfd, STDERR); */
                    /* fprintf(stdout, msg_buf, strlen(msg_buf) + 1); */
                    /* fflush(stdout); */
                }
            }
            const char prompt[] = {'%', ' ', 13, '\0' };
            n = write(ssock, prompt, strlen(prompt) + 1);
            /* dup2(ssock, STDOUT); */
            /* dup2(ssock, STDERR); */
            /* printf(prompt); */
            /* fflush(stdout); */
            /* bzero(msg_buf, 2048); */
            
            /* dup2(1, STDOUT); */
            /* dup2(2, STDERR); */
        
        }

        for (fd = 0; fd < nfds; fd++) {
            if (fd != sockfd && FD_ISSET(fd, &rfds)) {
                char buffer[15001];
                n = readline(fd, buffer, sizeof(buffer) - 1);
                buffer[n - 2] = '\0';
    
                int id;
                int i;
                for (i = 0; i < 30; i++) {
                    if (clientdata[i].sockfd == fd) {
                        id = i; 
                        break;
                    }
                }
                /* dup2(fd, STDOUT); */
                /* dup2(fd, STDERR); */
                if (strncmp(buffer, "who", 3) == 0) {
                    n = write(fd, title, strlen(title) + 1);
                    /* fprintf(stdout, title); */
                    /* fflush(stdout); */
                    for (i = 0; i < 30; i++) {
                        if (clientdata[i].sockfd != 0) {
                            if (clientdata[i].sockfd == fd) {
                                sprintf(msg_buf, indicate, i + 1, clientdata[i].nickname,
                                        clientdata[i].ip, '/', clientdata[i].port, me);
                                n = write(fd, msg_buf, strlen(msg_buf) + 1);
                                /* fprintf(stdout, msg_buf); */
                                /* fflush(stdout); */
                            } else {
                                sprintf(msg_buf, others, i + 1, clientdata[i].nickname,
                                        clientdata[i].ip, '/', clientdata[i].port);
                                n = write(clientdata[i].sockfd, msg_buf, strlen(msg_buf) + 1);
                                /* fprintf(stdout, msg_buf); */
                                /* fflush(stdout); */
                            }    
                            bzero(msg_buf, 2048);
                        }
                    }
                } else if (strncmp(buffer, "name", 4) == 0) {
                    int isSame = 1;
                    int isDead = 1;
                    for (i = 0; i < 30; i++) {
                        if (strcmp(clientdata[i].nickname, buffer + 5) == 0) {
                            sprintf(msg_buf, "*** User '%s' already exists. ***\n", buffer + 5);
                            fprintf(stdout, msg_buf);
                            fflush(stdout);
                            bzero(msg_buf, 2048);
                            isSame = 0;
                            break;
                        }
                    }

                    for (i = 0; i < 30; i++) {
                        if (deads[i] != NULL) {
                            if (strcmp(deads[i], buffer + 5) == 0) {
                                sprintf(msg_buf, "*** %s is already dead ***\n", deads[i]);
                                fprintf(stdout, msg_buf);
                                fflush(stdout);
                                bzero(msg_buf, 2048);
                                isDead = 0;
                                break;
                            }
                        }
                    }

                    if (isSame == 1 && isDead == 1) {
                        memcpy(clientdata[id].nickname, buffer + 5, strlen(buffer) - 4);
                    }
                    
                } else if (strncmp(buffer, "yell", 4) == 0) {
                    sprintf(msg_buf, "*** %s yelled ***: %s\n", clientdata[id].nickname, buffer + 5);
                    for (i = 0; i < 30; i++) {
                        if (clientdata[i].sockfd != 0) {
                            /* n = write(clientdata[i].sockfd, msg_buf, strlen(msg_buf) + 1); */
                            dup2(clientdata[i].sockfd, STDOUT);
                            dup2(clientdata[i].sockfd, STDERR);
                            fprintf(stdout, msg_buf);
                            fflush(stdout);
                        }    
                    }
                    bzero(msg_buf, 2048);
                } else if (strncmp(buffer, "tell", 4) == 0) {
                    int receive_id;
                    int msg[1025];
                    bzero(msg, 1025);
                    sscanf(buffer, "%*s %d %[^\t\n]", &receive_id, msg);
                    if (clientdata[receive_id - 1].sockfd != 0) {
                        sprintf(msg_buf, "*** %s told you ***: %s\n", clientdata[id].nickname, msg); 
                        dup2(clientdata[receive_id - 1].sockfd, STDOUT);
                        dup2(clientdata[receive_id - 1].sockfd, STDERR);
                        fprintf(stdout, msg_buf);
                        fflush(stdout);
                    } else {
                        sprintf(msg_buf, "*** Error: user #%d does not exist yet. ***\n", receive_id);   
                        dup2(clientdata[id].sockfd, STDOUT);
                        dup2(clientdata[id].sockfd, STDERR);
                        fprintf(stdout, msg_buf);
                        fflush(stdout);
                    }
                    bzero(msg_buf, 2048);
                } else if (strncmp(buffer, "exit", 4) == 0) {
                    /* logout hint */
                    sprintf(msg_buf, "*** User '%s' left. ***\n", clientdata[id].nickname);
                    for (i = 0; i < 30; i++) {
                        if (clientdata[i].sockfd != 0) {
                            n = write(clientdata[i].sockfd, msg_buf, strlen(msg_buf) + 1);
                            /* dup2(clientdata[i].sockfd, STDOUT); */
                            /* dup2(clientdata[i].sockfd, STDERR); */
                            /* fprintf(stdout, msg_buf); */
                            /* fflush(stdout); */
                        }
                    }
                    /* clear client data */
                    clientdata[id].sockfd = 0;
                    memset(clientdata[id].nickname, 0, 21);
                    memset(clientdata[id].ip, 0, 16);
                    clientdata[id].port = 0;
                    
                    /* dup2(1, STDOUT); */
                    /* dup2(2, STDERR); */

                    close(fd);
                    FD_CLR(fd, &afds);
                    bzero(msg_buf, 2048);
                    continue;
                } else if (strncmp(buffer, "suicide", 7) == 0) {
                    sprintf(msg_buf, "*** %s is already dead ***\n", clientdata[id].nickname);
                    for (i = 0; i < 30; i++) {
                        if (clientdata[i].sockfd != 0) { 
                            dup2(clientdata[i].sockfd, STDOUT);
                            dup2(clientdata[i].sockfd, STDERR);
                            fprintf(stdout, msg_buf);
                            fflush(stdout);
                        }
                    }
                    char *dead_message = (char*)malloc(sizeof(char) * (strlen(clientdata[id].nickname) + 1));
                    memcpy(dead_message, clientdata[id].nickname, strlen(clientdata[id].nickname) + 1);
                    for (i = 0; i < 30; i++) {
                        if (deads[i] == NULL) {
                            deads[i] = dead_message;
                            break;
                        }
                    }

                    /* logout hint */
                    sprintf(msg_buf, "*** User '%s' left. ***\n", clientdata[id].nickname);
                    for (i = 0; i < 30; i++) {
                        if (clientdata[i].sockfd != 0) {
                            /* n = write(clientdata[i].sockfd, msg_buf, strlen(msg_buf) + 1); */
                            dup2(clientdata[i].sockfd, STDOUT);
                            dup2(clientdata[i].sockfd, STDERR);
                            fprintf(stdout, msg_buf);
                            fflush(stdout);
                        }
                    }
                    /* clear client data */
                    clientdata[id].sockfd = 0;
                    memset(clientdata[id].nickname, 0, 21);
                    memset(clientdata[id].ip, 0, 16);
                    clientdata[id].port = 0;
                    fflush(stdout);
                    close(fd);
                    FD_CLR(fd, &afds);
                    bzero(msg_buf, 2048);
                    continue;
                } else {
                    dealcommand(id, fd, buffer);
                    
                }
                /* dup2(fd, STDOUT); */
                /* dup2(fd, STDERR); */
                const char prompt[] = {'%', ' ', 13, '\0'};
                n = write(fd, prompt, strlen(prompt) + 1);
                /* fprintf(stdout, prompt, strlen(prompt) + 1); */
                /* fflush(stdout); */
            }

            if (fd != sockfd && FD_ISSET(fd, &wfds)) {
            }
        }
    
    }
    unlink_ALL_FIFO();
    return 0;
}

/* parse input command line */
Command *parseCommands(char *commands) {

    int num = 0;
    Command *head, *pre = NULL;
    int len = strlen(commands);

    int check = 0;
    CommandType type;
    char *pch = NULL;
    pch = strtok(commands, " \n\r\t");
    for (unsigned i = 0; pch != NULL; i++) {
        if (pre == NULL) {
            Command *current = (Command*)malloc(sizeof(Command));
            current->command = pch;
            current->commandType = e_proc;
            current->next = NULL;

            pre = current;
            head = current;
        } else if (strcmp(pch, "|") == 0) {
            pch = strtok(NULL, " \n\r\t");
            Command *current = (Command*)malloc(sizeof(Command));
            current->command = pch;
            current->commandType = e_proc;
            current->next = NULL;

            pre->next = current;
            pre = current;
        } else if (strcmp(pch, ">") == 0) {
            /* file redirection */
            pch = strtok(NULL, " \n\r\t");
            Command *current = (Command*)malloc(sizeof(Command));
            current->command = pch;
            current->commandType = e_outfile;
            current->next = NULL;

            pre->next = current;
            pre = current;
        } else if (*pch - '|' == 0) {
            /* numbered-pipe stdout */
            Command *current = (Command*)malloc(sizeof(Command));
            current->command = pch + 1;
            current->commandType = e_stdout;
            current->next = NULL;

            pre->next = current;
            pre = current;
        } else if (*pch - '!' == 0) {
            /* numbered-pipe stderr */
            Command *current = (Command*)malloc(sizeof(Command));
            current->command = pch + 1;
            current->commandType = e_stderr;
            current->next = NULL;
        
            pre->next = current;
            pre = current;
		} else if (*pch - '>' == 0) {
            /* public pipe out */
            Command *current = (Command*)malloc(sizeof(Command));
            current->command = pch + 1;
            current->commandType = e_public_out;
            current->next = NULL;

            pre->next = current;
            pre = current;
        } else if (*pch - '<' == 0) {
            /* public pipe in */
            Command *current = (Command*)malloc(sizeof(Command));
            current->command = pch + 1;
            current->commandType = e_public_in;
            current->next = NULL;

            pre->next = current;
            pre = current;
        } else {
            /* arguments */
            Command *current = (Command*)malloc(sizeof(Command));
            current->command = pch;
            current->commandType = e_argv;
            current->next = NULL;

            pre->next = current;
            pre = current;
        }

        pch = strtok(NULL, " \n\r\t");
    }
    return head;
}


int run(int id, int sockfd, int readfd, Command *command,int counter, int readfdlist[], int writefdlist[], char *commands) {
    Command *args = command;
    Command *temp = command;

    char msg_buf[2048];
    bzero(msg_buf, 2048);

    int count = 0;
    // get arguments
    while (args->next != NULL && args->next->commandType == e_argv) {
        args = args->next;
        count++;
    }
    char *arguments[count + 1];
    bzero(arguments, count + 1);
    int n;
    char *envar = NULL;
    switch (command->commandType) {
        case e_proc:
            /* exit command */
            if (strcmp(command->command, "exit") == 0) {
                return -1;
            /* printenv */
            } else if (strcmp(command->command, "printenv") == 0) {
                if ((envar = getenv(command->next->command)) != NULL) { 
                    sprintf(msg_buf, "%s=%s\n", command->next->command, envar);
                    dup2(sockfd, STDOUT);
                    dup2(sockfd, STDERR);
                    /* n = write(sockfd, msg_buf, strlen(msg_buf) + 1); */
                    fprintf(stdout, msg_buf);
                    fflush(stdout);
                    return 0;
                }
            /* setenv */
            } else if (strcmp(command->command, "setenv") == 0) {
                // TODO:less arguments event
                setenv(command->next->command, command->next->next->command, 1);
                free(envs[id]);
                envs[id] = (char*)malloc(sizeof(char) * (strlen(command->next->next->command) + 1));
                memcpy(envs[id], command->next->next->command, strlen(command->next->next->command) + 1);
                return 0;
            } else if(strcmp(command->command, "remove") == 0) {
                setenv(command->next->command, "", 1);
                return 0;
            }

            /* get arguments */
            arguments[0] = command->command;
            for (unsigned i = 1; i < count + 1; i++) {
                arguments[i] = temp->next->command;
                temp = temp->next;
            }
            arguments[count + 1] = NULL;
           
            int pfd[2];
            pid_t pid;
            int status_pid;

            /* command tail */
            if (args->next == NULL) {
                /* close writefd */
                if (writefdlist[counter] != 0) {
                    close(writefdlist[counter]);
                }
                
                pid = fork();
                
                if (pid < 0) {
                    perror("ERROR on fork");
                    exit(1);
                }

                if (pid == 0) { 
                    dup2(readfd, STDIN); /* replace stdin with readfd */
                    dup2(sockfd, STDOUT); /* replace stdout with sockfd */
                    dup2(sockfd, STDERR); /* replace stderr with sockfd */

                    /* execv(command->path, arguments); */
                    execvp(command->command, arguments);
                    fprintf(stderr, "Unknown Command: [%s].\n", command->command);
                    exit(1);
                } else {
                    waitpid((pid_t)pid, &status_pid, 0);
                    
                    if (WIFEXITED(status_pid)) {
                        if (WEXITSTATUS(status_pid) == 0) { 
                            return 0;
                        }
                        return 1;
                    }
                    return 0;
                }
            /* redirect data to file */
            } else if (args->next->commandType == e_outfile) { 
                /* close writefd */
                if (writefdlist[counter] != 0) {
                    close(writefdlist[counter]);
                }

                pid = fork();

                if (pid < 0) {
                    perror("ERROR on fork");
                    exit(1);
                }

                if (pid == 0) {
                    int filefd = open(args->next->command, O_CREAT | O_RDWR, S_IREAD | S_IWRITE);
                    dup2(readfd, STDIN); /* replace stdin with readfd */
                    dup2(filefd, STDOUT); /* replace stdout with fileno(file) */
                    dup2(sockfd, STDERR); /* replace stderr with sockfd */
                    close(filefd);
                    /* execv(command->path, arguments); */
                    execvp(command->command, arguments);
                    fprintf(stderr, "Unknown Command: [%s].\n", command->command);
                    exit(1);
                } else {
                    waitpid((pid_t)pid, &status_pid, 0);
                    
                    if (WIFEXITED(status_pid)) {
                        return WEXITSTATUS(status_pid);
                    }
                    return 0;
                }
            /* public pipe out */
            } else if (args->next->commandType == e_public_out) {
                /* close writefd */
                if (writefdlist[counter] != 0) {
                    close(writefdlist[counter]);
                }

                int index = atoi(args->next->command);
                index = index - 1;
                if (create_FIFO(index) < 0) {
                    dup2(sockfd, STDIN);
                    dup2(sockfd, STDERR);
                    sprintf(msg_buf, "*** Error: public pipe #%d already exists. ***\n", index + 1);
                    /* n = write(sockfd, msg_buf, strlen(msg_buf) + 1); */
                    fprintf(stdout, msg_buf);
                    fflush(stdout);
                    bzero(msg_buf, 2048);
                    return 0;
                }
                int writeFIFO, readFIFO;
                open_FIFO(index, &readFIFO, &writeFIFO); 
        
                pid = fork();

                if (pid < 0) {
                    perror("ERROR on fork");
                    exit(1);
                }

                if (pid == 0) {
                    dup2(writeFIFO, STDOUT);
                    dup2(writeFIFO, STDERR);
                    dup2(readfd, STDIN);

                    execvp(command->command, arguments);
                    fprintf(stderr, "Unknown command: [%s].\n", command->command);
                    exit(1);
                } else { 
                    waitpid((pid_t)pid, &status_pid, 0);
                        
                    if (WIFEXITED(status_pid)) {
                        if (WEXITSTATUS(status_pid) == 0) {
                            sprintf(msg_buf, "*** %s #%d just piped '%s' ***\n", clientdata[id].nickname, id, commands);
                            int i;
                            for (i = 0; i < 30; i++) {
                                if (clientdata[i].sockfd != 0) {
                                    /* n = write(clientdata[i].sockfd, msg_buf, strlen(msg_buf) + 1); */
                                    dup2(clientdata[i].sockfd, STDOUT);
                                    dup2(clientdata[i].sockfd, STDERR);
                                    fprintf(stdout, msg_buf);
                                    fflush(stdout);
                                }
                            }
                            bzero(msg_buf, 2048);
                            return 0;
                        } else { 
                            unlink_FIFO(index);
                            return 1;
                        }
                    }
                    return 0;    
                } 
            } else if (args->next->commandType == e_public_in && args->next->next == NULL) {
			    /* close writefd */
                if (writefdlist[counter] != 0) {
                    close(writefdlist[counter]);
                }

                int index = atoi(args->next->command);
                index = index - 1;
                int writeFIFO, readFIFO;
                if (open_FIFO(index, &readFIFO, &writeFIFO) < 0) {
                    sprintf(msg_buf, "*** Error: public pipe #%d does not already exists. ***\n", index + 1);
                    /* n = write(sockfd, msg_buf, strlen(msg_buf) + 1); */
                    dup2(sockfd, STDOUT);
                    dup2(sockfd, STDERR);
                    fprintf(stdout, msg_buf);
                    fflush(stdout);
                    return 0;
                }
                
                pid = fork();

                if (pid < 0) {
                    perror("ERROR on fork");
                    exit(1);
                }

                if (pid == 0) {
                    dup2(sockfd, STDOUT);
                    dup2(sockfd, STDERR);
                    dup2(readFIFO, STDIN);

                    execvp(command->command, arguments);
                    fprintf(stderr, "Unknown command: [%s].\n", command->command);
                    exit(1);
                } else { 
                    waitpid((pid_t)pid, &status_pid, 0);
                        
                    if (WIFEXITED(status_pid)) {
                        if (WEXITSTATUS(status_pid) == 0) {
                            sprintf(msg_buf, "*** %s #%d just received via '%s' ***\n", clientdata[id].nickname, id, commands);
                            int i;
                            for (i = 0; i < 30; i++) {
                                if (clientdata[i].sockfd != 0) {
                                    /* n = write(clientdata[i].sockfd, msg_buf, strlen(msg_buf) + 1); */
                                    dup2(clientdata[i].sockfd, STDOUT);
                                    dup2(clientdata[i].sockfd, STDERR);
                                    fprintf(stdout, msg_buf);
                                    fflush(stdout);
                                }
                            }
                            bzero(msg_buf, 2048);
                            unlink_FIFO(index);
                            return 0;
                        } else { 
                            return 1;
                        }
                    }
                    return 0;    
                }
	
            /* 1 numbered-pipe */
            } else if ((args->next->commandType == e_stdout ||
                    args->next->commandType == e_stderr) && args->next->next == NULL) {
                int std = args->next->commandType == e_stdout ? STDOUT : STDERR; /* stdout or stderr */
                
                int number = atoi(args->next->command);
                int writefd;

                /* close writefd */
                if (writefdlist[counter] != 0) {
                    close(writefdlist[counter]);
                    /* printf("close write = %d\n", writefdlist[counter]); */
                }
                
                /* if pipe is not already exist */
                if ((writefd = writefdlist[(counter + number) % 2000]) == 0) {
                    if (pipe(pfd) < 0) {
                        perror("ERROR creating a pipe");
                        exit(1);
                    }  

                    pid = fork();

                    if (pid < 0) {
                        perror("ERROR on fork");
                        exit(1);
                    }

                    if (pid == 0) {
                        dup2(pfd[1], std); /* replace stdout or stderr with pfd[1] */
                        dup2(readfd, STDIN); /* replace stdin with readfd */
                        close(pfd[0]);

                        /* execv(command->path, arguments); */
                        execvp(command->command, arguments);
                        fprintf(stderr, "Unknown Command: [%s].\n", command->command);
                        exit(1);
                    } else {

                        /* if (readfd != 0) { */
                            /* close(readfd); */
                            /* printf("close = %d\n", readfd); */
                        /* } */
                        waitpid((pid_t)pid, &status_pid, 0);
                     
                        if (readfd != 0) {
                            close(readfd);
                            /* printf("close = %d\n", readfd); */
                        }

                        if (WIFEXITED(status_pid)) {
                            if (WEXITSTATUS(status_pid) == 0) {
                                writefdlist[(counter + number) % 2000] = pfd[1];
                                readfdlist[(counter + number) % 2000] = pfd[0];
                                return 0;
                            } 
                            return 1;
                        }
                        return 0;    
                    }
                /* if pipe is exist */
                } else {
                    pid = fork();
                    if (pid == 0) {
                        dup2(writefd, std); /* replace stdout or stderr with writefd */
                        dup2(readfd, STDIN); /* replace stdin with readfd */
                        
                        /* execv(command->path, arguments); */
                        execvp(command->command, arguments);
                        fprintf(stderr, "Unknown Command: [%s].\n", command->command);
                        exit(1);
                    } else {
                        waitpid((pid_t)pid, &status_pid, 0);
                    
                        /* if (readfd != 0) { */
                            /* close(readfd); */
                        /* } */

                        if (WIFEXITED(status_pid)) {
                            return WEXITSTATUS(status_pid);
                        }
                        return 0;
                    }
                }
            /* 2 numbered-pipe */
            } else if ((args->next->commandType == e_stdout || args->next->commandType == e_stderr) &&
                    (args->next->next->commandType == e_stdout || args->next->next->commandType == e_stderr)) {
                /* close writefd */
                if (writefdlist[counter] != 0) {
                    close(writefdlist[counter]);
                }

                int num_stdout, num_stderr;
                if (args->next->commandType == e_stdout) {
                    num_stdout = atoi(args->next->command);
                    num_stderr = atoi(args->next->next->command);
                } else {
                    num_stdout = atoi(args->next->next->command);
                    num_stderr = atoi(args->next->command);
                }
                int writefd_stdout = writefdlist[(counter + num_stdout) % 2000];
                int writefd_stderr = writefdlist[(counter + num_stderr) % 2000];

                /* numbered-pipe stdout and stderr is not yet created */
                if (writefd_stdout == 0 && writefd_stderr == 0) {
                    /* the number of stdout pipe and stderr pipe is not equal */
                    if (num_stdout != num_stderr) {
                        int pfd_stdout[2], pfd_stderr[2];
                        if (pipe(pfd_stdout) < 0) {
                            perror("ERROR creating a pipe");
                            exit(1);
                        }           
                        if (pipe(pfd_stderr) < 0) {
                            perror("ERROR creating a pipe");
                            exit(1);
                        }
                        
                        pid = fork();

                        if (pid == 0) {
                            /* stdin */
                            dup2(readfd, STDIN);
                            /* stdout */
                            dup2(pfd_stdout[1], STDOUT);
                            close(pfd_stdout[0]);
                            /* stderr */
                            dup2(pfd_stderr[1], STDERR);
                            close(pfd_stderr[0]);

                            /* execv(command->path, arguments); */
                            execvp(command->command, arguments);
                            fprintf(stderr, "Unknown Command: [%s].\n", command->command);
                            exit(1);
                        } else {
                            waitpid((pid_t)pid, &status_pid, 0);
                            
                            if (WIFEXITED(status_pid)) {
                                if(WEXITSTATUS(status_pid) == 0) {
                                    writefdlist[(counter + num_stdout) % 2000] = pfd_stdout[1];
                                    writefdlist[(counter + num_stderr) % 2000] = pfd_stderr[1];
                                    readfdlist[(counter + num_stdout) % 2000] = pfd_stdout[0];
                                    readfdlist[(counter + num_stderr) % 2000] = pfd_stderr[0];
                                    return 0;
                                } 
                                return 1;
                            }
                            return 0;
                        }
                    /* the number of stdout pipe and stderr pipe is equal */
                    } else {
                        if (pipe(pfd) < 0) {
                            perror("ERROR creating a pipe");
                            exit(1);
                        }           
                        
                        pid = fork();

                        if (pid == 0) {
                            /* stdin */
                            dup2(readfd, STDIN);
                            /* stdout */
                            dup2(pfd[1], STDOUT);
                            /* stdin */
                            dup2(pfd[1], STDERR);
                            close(pfd[0]);

                            /* execv(command->path, arguments); */
                            execvp(command->command, arguments);
                            fprintf(stderr, "Unknown Command: [%s].\n", command->command);
                            exit(1);
                        } else {
                            waitpid((pid_t)pid, &status_pid, 0);
                                
                            if (WIFEXITED(status_pid)) {
                                if(WEXITSTATUS(status_pid) == 0) {
                                    writefdlist[(counter + num_stdout) % 2000] = pfd[1];
                                    readfdlist[(counter + num_stdout) % 2000] = pfd[0];
                                    return 0;
                                }
                                return 0;
                            }
                        }
                    }
                /* numbered-pipe stdout is created, but stderr not yet */
                } else if (writefd_stdout != 0 && writefd_stderr == 0) {
                    if (pipe(pfd) < 0) {
                        perror("ERROR creating a pipe");
                        exit(1);
                    }

                    pid = fork();

                    if (pid == 0) {
                        /* stdin */
                        dup2(readfd, STDIN); 
                        /* stdout */
                        dup2(writefd_stdout, STDOUT); 
                        /* stderr */
                        dup2(pfd[1], STDERR); 
                        close(pfd[0]);

                        /* execv(command->path, arguments); */
                        execvp(command->command, arguments);
                        fprintf(stderr, "Unknown Command: [%s].\n", command->command);
                        exit(1);
                    } else {
                        waitpid((pid_t)pid, &status_pid, 0);
                        
                        if (WIFEXITED(status_pid)) {
                            if(WEXITSTATUS(status_pid) == 0) {
                                writefdlist[(counter + num_stderr) % 2000] = pfd[1];
                                readfdlist[(counter + num_stderr) % 2000] = pfd[0];
                                return 0;
                            }
                            return 1;
                        }
                        return 0;
                    }
                /* numbered-pipe stderr is created, but stdout not yet */
                } else if (writefd_stdout == 0 && writefd_stderr != 0) {
                    if (pipe(pfd) < 0) {
                        perror("ERROR creating a pipe");
                        exit(1);
                    }          

                    pid = fork();

                    if (pid == 0) {
                        /* stdin */
                        dup2(readfd, STDIN);
                        /* stdout */
                        dup2(pfd[1], STDOUT);
                        /* stderr */
                        dup2(writefd_stderr, STDERR);
                        close(pfd[0]);

                        /* execv(command->path, arguments); */
                        execvp(command->command, arguments);
                        fprintf(stderr, "Unknown Command: [%s].\n", command->command);
                        exit(1);
                    } else {
                        waitpid((pid_t)pid, &status_pid, 0);

                        if (WIFEXITED(status_pid)) {
                            if(WEXITSTATUS(status_pid) == 0) {
                                writefdlist[(counter + num_stdout) % 2000] = pfd[1];
                                readfdlist[(counter + num_stdout) % 2000] = pfd[0];
                                return 0;
                            }
                            return 1;
                        }
                        return 0;
                    }
                /* numbered-pipe stdout and stderr are created */
                } else {
                    
                    pid = fork();

                    if (pid == 0) {
                        /* stdin */
                        dup2(readfd, STDIN);
                        /* stdout */
                        dup2(writefd_stdout, STDOUT);
                        /* stderr */
                        dup2(writefd_stderr, STDERR);

                        /* execv(command->path, arguments); */
                        execvp(command->command, arguments);
                        fprintf(stderr, "Unknown Command: [%s].\n", command->command);
                        exit(1);
                    } else {
                        waitpid((pid_t)pid, &status_pid, 0);
                        
                        if (WIFEXITED(status_pid)) {
                            return WEXITSTATUS(status_pid);
                        }
                        return 0;
                    }
                }               
            } else {
                /* close writefd */
                if (writefdlist[counter] != 0) {
                    close(writefdlist[counter]);
                    writefdlist[counter] = 0;
                }

                /* NOTE:Create needed pipe first, then close writefd of current counter */
                if (pipe(pfd) < 0) {
                    perror("ERROR creating a pipe");
                    exit(1);
                }


				int index = -1;
                int writeFIFO, readFIFO;
                if (args->next->commandType == e_public_in) {
                    index = atoi(args->next->command);
                    index = index - 1;
                    int writeFIFO, readFIFO;
                    if (open_FIFO(index, &readFIFO, &writeFIFO) < 0) {
                        sprintf(msg_buf, "*** Error: public pipe #%d does not already exists. ***\n", index + 1);
                        dup2(sockfd, STDOUT);
                        dup2(sockfd, STDERR);
                        fprintf(stdout, msg_buf);
                        fflush(stdout);
                        return 0;
                    }
                    readfd = readFIFO;
                }

                pid = fork();
                    
                if (pid == 0) {
                    dup2(readfd, STDIN); /* replace stdin with readfd  */
                    dup2(pfd[1], STDOUT); /* replace stdout with pfd[1] */
                    dup2(sockfd, STDERR); /* replace stderr with sockfd */
                    close(pfd[0]);

                    /* execv(command->path, arguments); */
                    execvp(command->command, arguments);
                    fprintf(stderr, "Unknown Command: [%s].\n", command->command);
                    exit(1);
                } else {
                    close(pfd[1]);
                 
                    waitpid((pid_t)pid, &status_pid, 0);
                    if (readfd != 0) {
                        close(readfd);   
                    }
                   
                    if (WIFEXITED(status_pid)) {
                        if(WEXITSTATUS(status_pid) == 0) {
                            readfdlist[counter] = pfd[0];                    
                            if (index != -1) {
                                sprintf(msg_buf, "*** %s #%d just received via '%s' ***\n", clientdata[id].nickname, id, commands);
                                int i;
                                for (i = 0; i < 30; i++) {
                                    if (clientdata[i].sockfd != 0) {
                                        /* n = write(clientdata[i].sockfd, msg_buf, strlen(msg_buf) + 1); */
                                        dup2(clientdata[i].sockfd, STDOUT);
                                        dup2(clientdata[i].sockfd, STDERR);
                                        fprintf(stdout, msg_buf);
                                        fflush(stdout);
                                    }
                                }
                                bzero(msg_buf, 2048);
                                unlink_FIFO(index);
                            }
                            return 0;
                        }
                        return 1;
                    }
                    return 0;
                }
            }
            break;
        case e_argv:
            break;

        case e_stdout:
            break;

        case e_stderr:
            break;   

        case e_outfile:
            break;
    }
}

int readline(int fd,char *ptr,int maxlen) {
	int n,rc;
	char c;
	*ptr = 0;
	for(n=1;n<maxlen;n++)
	{
		if((rc=read(fd,&c,1)) == 1)
		{
			*ptr++ = c;	
			if(c=='\n')  break;
		}
		else if(rc==0)
		{
			if(n==1)     return(0);
			else         break;
		}
		else
			return(-1);
	}
	return(n);
}

void send_welcome(int sockfd) {
    const char *welcome = 
        "****************************************\n"
        "** Welcome to the information server. **\n"
        "****************************************\n";
    int n = write(sockfd, welcome, strlen(welcome) + 1);
    if (n < 0) {
        perror("ERROR on writing welcome information to client");
        exit(1);
    }
    /* fprintf(stdout, welcome, strlen(welcome) + 1); */
}

void send_prompt(int sockfd) {
    const char prompt[] = {'%', ' ', 13, '\0'};
    /* int n = write(sockfd, prompt, strlen(prompt) + 1); */
    /* if (n < 0) { */
        /* perror("ERROR on writing prompt to client"); */
        /* exit(1); */
    /* } */
    fprintf(stdout, prompt, strlen(prompt) + 1);
    fflush(stdout);
}

void handler(int sockfd) {

}

void dealcommand(int id, int sockfd, char *commands) {

    setenv("PATH", envs[id], 1);

    char *buffer = (char*)malloc(sizeof(char) * (strlen(commands) + 1));
    memcpy(buffer, commands, strlen(commands) + 1);

    Command *head = parseCommands(commands);
    Command *go = head;

    int retfd, status;
    int move = 0;
    int readfd = readfdlists[id][counters[id] % 2000];
    /* printf("readfd %d\n", readfd); */
    /* fflush(stdout); */
    while (go != NULL) {
            
        status = run(id, sockfd, readfd, go, counters[id], readfdlists[id], writefdlists[id], buffer);
        /* printf("status = %d\n", status); */
        /* fflush(stdout); */
        if (status == 0) {
            move = 1;
        } else if (status == 1) {
            break;
        }
        readfd = readfdlists[id][counters[id] % 2000];
        while (go->next != NULL && (go->next->commandType == e_argv || 
                go->next->commandType == e_stdout ||
                go->next->commandType == e_stderr ||
                go->next->commandType == e_outfile)) {
            go = go->next;
        }
        go = go->next;
    }
        
    /* free linked list */
    Command *tmp;
    while (head != NULL) {
        tmp = head;
        head = head->next;
        free(tmp);
    }

    if (status == -1) {
        return;
    }
    
    /* normal situation */
    if (move == 1) {
        /* reset writefd and readfd; */
        writefdlists[id][counters[id] % 2000] = 0;
        readfdlists[id][counters[id] % 2000] = 0;
        /* printf("counter++\n"); */
        /* fflush(stdout); */
        /* printf("counter = %d\n", counter); */
        counters[id]++;
        move = 0;
    }

}

