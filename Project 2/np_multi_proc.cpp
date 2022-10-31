#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <string.h>
#include <ctype.h>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sstream>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/shm.h>
#include "multi.h"
using namespace std;

User *users;
BroadcastMsg *msg;
int ID;
int shm_1, shm_2;

bool checkPipe(int from, int to){
	char s[20];
    	sprintf(s,"user_pipe/%d_%d", from, to);
    	if(access(s,F_OK) != -1)
        	return 1;
    	return 0;
}

void reap(int signo){
	if(signo == SIGCHLD){
		int status;
		while(waitpid(-1, &status, WNOHANG) > 0){}

        	for(int i=0; i<30; i++){
        		if(users[i].hasUser == 1){
                		if(kill(users[i].pid, 0) < 0){
                    			char s[100];
					users[i].hasUser = 0;
                    			sprintf(s,"*** User '%s' left. ***\n", users[ID].name);
                    			strcpy(msg->str, s);

                    			for(int j=0; j<30; j++)
                        			if(users[j].hasUser == 1)
                            				kill(users[j].pid, SIGUSR1);
 
                    			char t[20];
                    			for(int j=0; j<30; j++){
                        			if(checkPipe(i+1, j+1) == 1){
                            				sprintf(t, "user_pipe/%d_%d", i+1, j+1);
                            				remove(t);
                       				}

                        			if(checkPipe(j+1,i+1) == 1){
                            				sprintf(t, "user_pipe/%d_%d", j+1, i+1);
                            				remove(t);
                        			}
                    			}
                    			break;
                		}
            		}
        	}
    	}
    	else{
        	shmdt(users);
        	shmdt(msg);
        	shmctl(shm_1, IPC_RMID, (shmid_ds*)0);
        	shmctl(shm_2, IPC_RMID, (shmid_ds*)0);
        	exit(1);
    	}
}

int main(int argc, char *argv[]){
	int sockfd, newsockfd, status;
	if (argc != 2)
		return 0;

	//userPipes.clear();
	clearenv();

	unsigned short port = (unsigned short)atoi(argv[1]);

	//sockfd = create_socket(port);
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		cerr << "Socket create fail.\n";
		exit(0);
	}
	struct sockaddr_in serv_addr, cli_addr;

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    	serv_addr.sin_port = htons(port);

	if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
		cerr << "Socket bind fail.\n";
		exit(0);
	}
	listen(sockfd, 30);

	shm_1 = shmget(SHMKEY_Users, sizeof(User)*30, 0666|IPC_CREAT);
    	shm_2 = shmget(SHMKEY_BroadcastMsg, sizeof(BroadcastMsg), 0666|IPC_CREAT);
	users = (User*)shmat(shm_1, (char*)0, 0);
    	msg = (BroadcastMsg*)shmat(shm_2, (char*)0, 0);	

    	for(int i=0;i<30;i++){
        	users[i].hasUser = 0;
		users[i].pid = 0;
		users[i].done = 1;
    	}

    	signal(SIGCHLD, reap);
    	signal(SIGINT, reap);

	while(1){
		socklen_t cli_len = sizeof(cli_addr);
		newsockfd = accept(sockfd, (sockaddr *)&cli_addr, &cli_len);
		cout<<"hi"<<endl;
		for (ID=0; ID<30; ID++){
			if (users[ID].hasUser == 0)
				break;
		}
		FORK:
		pid_t child_pid = fork();			

		if(child_pid < 0){
			while(waitpid(-1,&status,WNOHANG) > 0){}
			goto FORK;

		}

		//start fork process
		if(child_pid == 0){
            		close(sockfd);
            		close(STDIN_FILENO);
            		close(STDOUT_FILENO);
            		close(STDERR_FILENO);
            		dup2(newsockfd, STDIN_FILENO);
            		dup2(newsockfd, STDOUT_FILENO);
            		dup2(newsockfd, STDERR_FILENO);
            		close(newsockfd);

            		usleep(10);
			np_shell();
			exit(0);
		}
		else{
			char str[INET_ADDRSTRLEN];
			strcpy(users[ID].name, "(no name)");
			users[ID].hasUser = 1;
            		inet_ntop(AF_INET, &(cli_addr.sin_addr), str, INET_ADDRSTRLEN);
            		
			//get port
           		int port = ntohs(cli_addr.sin_port);
            		char s[100];
            		sprintf(s,"%s:%d", str, port);
           		strcpy(users[ID].address, s);
            		users[ID].pid = child_pid;

            		close(newsockfd);
		}
	}
}

