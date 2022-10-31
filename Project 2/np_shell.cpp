#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <ctype.h>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sstream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/shm.h>
#include "multi.h"
using namespace std;

const size_t COMMAND_BUFFER=15000;

extern User *users;
extern BroadcastMsg *msg;
extern int shmid1,shmid2;
extern int ID;

bool checkExist(int sourceID, int targetID){
    char s[20];
    sprintf(s,"user_pipe/%d_%d", sourceID, targetID);
    if(access(s, F_OK) != -1)
        return 1;
    return 0;
}

void broadcast(int *sourceID, int *targetID, char *m){
	strcpy(msg->str, m);
	if (targetID == NULL){
		for (int i=0; i<30; i++){
			if (users[i].hasUser == 1)
				kill(users[i].pid, SIGUSR1);
		}
	} 
	else 
		kill(users[*targetID].pid, SIGUSR1);
}

void who(){ //who
	//cout<<"who"<<endl;
	printf("<ID>\t<nickname>\t<IP:port>\t<indicate me>\n");
	for (int i=0; i<30; i++){
		if (users[i].hasUser == 1){
			printf("%d\t%s\t%s", i+1, users[i].name, users[i].address);
			if (i == ID)
				printf("\t<-me");
			printf("\n");
		}
	}
	//broadcast(NULL, ID, m);
}
		
void tell(int targetID, char *s){ //tell
	//cout<<"tell"<<endl;
	char m[5000];
	targetID -= 1;
	
	if (users[targetID].hasUser == 1){
		sprintf(m, "*** %s told you ***: %s\n", users[ID].name, s);
        	strcpy(msg->str, m);
		broadcast(NULL, &targetID, m);
	} 
	else
		printf("*** Error: user #%d does not exist yet. ***\n", targetID+1);
		//broadcast(NULL, &ID, m);
}

void yell(char *s){ //yell
	//cout<<"yell"<<endl;
	char m[5000];

	sprintf(m, "*** %s yelled ***: %s\n", users[ID].name, s);
	broadcast(NULL, NULL, m);
}

void name(char *newName){ //name
	//cout<<"name"<<endl;
	char m[1000];
	for (int j=0; j<30; j++){
		if (j == ID)
			continue;
		if (users[j].hasUser == 1 && (strcmp(users[j].name,newName) == 0)){
			sprintf(m, "*** User '%s' already exists. ***\n", newName);
			broadcast(NULL, &ID, m);
			return;
		}
	}

	strcpy(users[ID].name, newName);
	sprintf(m, "*** User from %s is named '%s'. ***\n", users[ID].address, users[ID].name);
	broadcast(NULL, NULL, m);
}

//Execute Command from Parsed Vector
void Exec(vector<string> line){
	vector<const char*> pointerVec;
    	for(int i=0; i<line.size(); i++){
        	pointerVec.push_back(line[i].c_str());
    	}
    	pointerVec.push_back(NULL);
    	const char** commands = &pointerVec[0];
    
    	if(execvp(line[0].c_str(),(char* const*)commands) < 0){
        	fprintf(stderr,"Unknown command: [%s].\n",line[0].c_str());
        	exit(1);
    	}
}

void signal_Handler(int signo){
    	printf("%s", msg->str);
}


void sig_fork(int signo){
	int status;
	while(waitpid(-1,&status,WNOHANG) > 0){}
	// -1 -> wait any child process
	//WHOHANG -> return immediately without wait
	// == 0 , use WHOHANG and no childPid return
}

void np_shell(){
	signal(SIGCHLD, sig_fork);
    	signal(SIGUSR1, signal_Handler);
	clearenv();
	//show welcome
	char text[] = "****************************************\n** Welcome to the information server. **\n****************************************";
	cout<<text<<endl;
	//login message
	string log = "*** User '"+string(users[ID].name)+"' entered from "+string(users[ID].address)+". ***\n";
	broadcast(NULL, NULL, strdup(log.c_str()));
	setenv("PATH", "bin:.", 1);
    	vector <PipeInfo> userPipes;

    	while(1){
        	char *input = (char*)malloc(COMMAND_BUFFER);
        	char in[COMMAND_BUFFER];
        	char intact[COMMAND_BUFFER];
        	vector<vector<string>> cmds;
        	bool FileFlag = 0;
        	int PipeType = -1;     //0 -> |, 1 -> !
        	int PipeCount = 0;
        	int pipeToID = 0;
        	int pipeFromID = 0;
        	int userPipeIn = -1;
        	int userPipeOut = -1;
        	string file = "";
        	vector <int> pidTable;

        	//Parsing-- 
        	cout<<"% ";
        	if(!cin.getline(input,COMMAND_BUFFER)){
            		exit(1);
        	}
        	//Handle \r\n for telnet
        	char *current_pos = strchr(input,'\r');
        	while (current_pos){
            		*current_pos = '\0';
            		current_pos = strchr(current_pos,'\r');
        	}
        	strcpy(in,input);
        	strcpy(intact, input);

        	if(strlen(input)==0){
            		free(input);
            		continue;
        	}

		if(strcmp(input, "exit") == 0){ //exit
			//show_logoutMsg
			char m[100];
			sprintf(m, "*** User '%s' left. ***\n",users[ID].name);
			broadcast(NULL, NULL, m);
			users[ID].hasUser = 0;
			
			//remove user pipe
			char s[20];
    			for(int i=0; i<30; i++){
        			if(checkExist(ID+1, i+1)){
           				sprintf(s,"user_pipe/%d_%d",ID+1, i+1);
           				remove(s);
        			}
        			if(checkExist(i+1, ID+1)){
           				sprintf(s,"user_pipe/%d_%d",i+1, ID+1);
            				remove(s);
        			}
    			}
            		shmdt(msg);
            		shmdt(users);
            		exit(0);
		}


        	char *str;
        	str = strtok(input," ");
        	int i=0;
        	cmds.push_back(vector<string>());
        
        	while (str != NULL){
            		if(str[0]=='|' || str[0]=='!'){
                		if(strlen(str) > 1){
                    			if(str[0] == '|')
						PipeType = 0;
					else
						PipeType = 1;
                    			str[0]=' ';
                    			PipeCount=atoi(str);
                		}
                		else{
                    			cmds.push_back(vector<string>());
                    			i++;
                		}
                		str = strtok (NULL, " ");
                		continue;
            		}
            		else if (str[0] == '>'){
                		if(strlen(str) == 1){
                    			FileFlag = 1;
                    			str = strtok (NULL, " ");
                    			file = string(str);
                    			break;
                		}
                		else{
                    			str[0]=' ';
                    			pipeToID = atoi(str);
                    			str = strtok (NULL, " ");
                    			continue;
                		}
            		}
            		else if(str[0] == '<'){
                		str[0] = ' ';
                		pipeFromID = atoi(str);
                		str = strtok (NULL, " ");
                		continue;
            		}
            		cmds[i].push_back(string(str));
            		str = strtok (NULL, " ");
        	}  

        	//  Check Numpipe
        	for(int i=0;i<userPipes.size();i++)
            		userPipes[i].count -= 1;

        	PipeInfo pipeOut, pipeIn;
        	bool pIn = 0;
        
        	if(PipeType > -1){
            		int f = -1;
            		for(int i=0; i<userPipes.size(); i++){
                		if(userPipes[i].count == PipeCount){
                    			f = i;
                    			break;
                		}
            		}
            		if(f > -1){
                		pipeOut = userPipes[f];
            		}
            		else{
                		pipeOut.count = PipeCount;
                		pipe(pipeOut.fd);
                		userPipes.push_back(pipeOut);
            		}
        	}
        
        	for(int i=0; i<userPipes.size(); i++){
            		if(userPipes[i].count == 0){
                		pIn = 1;
                		pipeIn = userPipes[i];
                		userPipes.erase(userPipes.begin()+i);
                		close(pipeIn.fd[1]);
                		break;
            		}
        	}

        	//User Pipe
        	if(pipeFromID > 0){
            		if(users[pipeFromID-1].hasUser == 0){
                		printf("*** Error: user #%d does not exist yet. ***\n",pipeFromID);
                		free(input);
                		continue;
            		}
        	}
        
        	if(pipeToID > 0){
            		if(users[pipeToID-1].hasUser == 0){
                		printf("*** Error: user #%d does not exist yet. ***\n", pipeToID);
                		free(input);
                		continue;
            		}
        	}
        
        	if(pipeFromID > 0){
            		if(checkExist(pipeFromID,ID+1) == 0){
                		printf("*** Error: the pipe #%d->#%d does not exist yet. ***\n", pipeFromID, ID+1);
                		free(input);
                		continue;
            		}
            		else{
                		char s1[20],s2[20];
                		sprintf(s1, "user_pipe/%d_%d", pipeFromID, ID+1);
                		sprintf(s2, "user_pipe/!%d_%d", pipeFromID, ID+1);
                		rename(s1, s2);
                		userPipeIn = open(s2, O_RDONLY);
                		char s[100];
                		sprintf(s,"*** %s (#%d) just received from %s (#%d) by '%s' ***\n", users[ID].name, ID+1, users[pipeFromID-1].name, pipeFromID, intact);
                		broadcast(NULL, NULL, s);
                		usleep(10);
            		}
        	}

        	if(pipeToID > 0){
            		if(checkExist(ID+1, pipeToID)){
                		printf("*** Error: the pipe #%d->#%d already exists. ***\n",ID+1,pipeToID);
                		free(input);
                		continue;
            		}
            		else{
                		char s1[20];
                		sprintf(s1,"user_pipe/%d_%d",ID+1,pipeToID);
                		userPipeOut = open(s1, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR);
                		char s[100];
                		sprintf(s,"*** %s (#%d) just piped '%s' to %s (#%d) ***\n",users[ID].name, ID+1, intact, users[pipeToID-1].name, pipeToID);
                		broadcast(NULL, NULL, s);
            		}
        	}
        

        	// Executing --

        	if(cmds[0][0]=="who"){
            		who();
            		free(input);
            		continue;
        	}

        	if(cmds[0][0]=="tell"){
            		int to=atoi(cmds[0][1].c_str());
            		char *s=strchr(in,' ');
            		s=strchr(s+1,' ');
            		s++;
            		while(*s==' ')s++;
            		tell(to,s);
            		free(input);
            		continue;
        	}

        	if(cmds[0][0]=="yell"){
  	          	char *s=strchr(in,' ');
        	    	s++;
        	    	while(*s==' ')s++;
        	    	yell(s);
        	    	free(input);
        	    	continue;
        	}
	
        	if(cmds[0][0]=="name"){
	            	char *s=strchr(in,' ');
	            	s++;
	            	while(*s==' ')s++;
	            	name(s);
	            	free(input);
	            	continue;
	        }
	
	        //handle printenv
	        if(cmds[0][0]=="printenv"){
	           	char *str=getenv(cmds[0][1].c_str());
	            	if(str!=NULL){
	                	printf("%s\n",str);
	            	}
	            	free(input);
	            	continue;
	        }
	        
	        //handle setenv
	        if(cmds[0][0]=="setenv"){
	            	if(cmds[0].size()>=3)
	                	setenv(cmds[0][1].c_str(),cmds[0][2].c_str(),1);
	            	free(input);
	            	continue;
	        }
	        if(cmds[0][0]=="unsetenv"){
	            	if(cmds[0].size()>=2)
	                	unsetenv(cmds[0][1].c_str());
	            	free(input);
	            	continue;
	        }
	
	        //exec
	        int pipe0[2];
	        int prePipeRd = 0;
	        if(pIn == 1){
	            	prePipeRd = pipeIn.fd[0];
	        }
	        if(userPipeIn != -1){
	            	prePipeRd = userPipeIn;
	        }
	
	        int filefd;
	        if(FileFlag == 1){
	            	filefd = open(file.c_str(), O_CREAT|O_WRONLY|O_TRUNC,S_IRUSR|S_IWUSR);
	        }
	
	        for(int i=0; i<cmds.size(); i++){
	            	if((cmds.size()-i)>1){
	                	if(pipe(pipe0)<0) printf("Pipe Error\n");
	                	//cout << pipe0[0] << " "<<pipe0[1]<<endl;
	            	}
	            	pid_t childpid;
	            	while((childpid=fork())<0){
	                	usleep(1000);
	            	}
	            	pidTable.push_back(childpid);
	            	if(childpid==0){
	                	if(((cmds.size()-i)==1) && FileFlag){
	                    
	                    		//cout<<fd<<endl;
	                    		close(1);
	                    		dup2(filefd,1);
	                    		close(filefd);
	                	}
	
	                	if(prePipeRd>0){        //No pipe in
	                    		close(0);           //close stdin
	                    		dup2(prePipeRd,0);  //pre read to stdin
	                    		close(prePipeRd);   //close pre
	                	}
	                	if((cmds.size()-i)>1){     //Not last command
	                    		close(1);           //close stdout
	                    		dup2(pipe0[1],1);   //write to stdout
	                    		close(pipe0[0]);    //close read
	                    		close(pipe0[1]);    //close write
	                	}
	                	if((cmds.size()-i)==1){    //Last Command
	                    		if(PipeType>-1){
	                        		close(1);
	                        		dup2(pipeOut.fd[1],1);
	                        		if(PipeType==1){
	                           			close(2);
	                            			dup2(pipeOut.fd[1],2);
	                        		}
	                        		close(pipeOut.fd[0]);
	                        		close(pipeOut.fd[1]);
	                    		}
	                    		if(userPipeOut!=-1){
	                        		close(1);
	                        		dup2(userPipeOut,1);
	                        		close(2);
	                        		dup2(userPipeOut,2);
	                        		close(userPipeOut);
	                    		}  
	                	}
	                	Exec(cmds[i]);
			}
			else{
	            		if(prePipeRd>0){        
	                    		close(prePipeRd);   //close pre read
	                	}
	                	if((cmds.size()-i)>1){
	                    		close(pipe0[1]);    //close write
	                    		prePipeRd=pipe0[0]; //read to next process
	                	}
	                	if(FileFlag && (cmds.size()-i) == 1){
	                    		close(filefd);
	                	}
	                	if((cmds.size()-i)==1&&userPipeOut!=-1){
	                    		close(userPipeOut);
	                	}
	        	}
	
	 	}
	        if(PipeType == -1){
	            	for(int i=0;i<pidTable.size();i++){
	                	int pid=pidTable[i];
	                	int status;
	                	waitpid(pid,&status,0);
	            	}
	            	if(userPipeIn!=-1){
	                	char s[20];
	                	sprintf(s,"user_pipe/!%d_%d",pipeFromID,ID+1);
	                	remove(s);
	            	}
	        }
	        free(input);
	}
    	return;
}
