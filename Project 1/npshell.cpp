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
using namespace std;

class Cmd{
	public:
		string command;
		char mark;
		int lineToPipe;
		vector<string> tokens;
		void putVar(string com, char m, int n); //command,mark,num(pipe)
		int comType();
		void split();
		int fd_in, fd_out;
		bool fdIn, fdOut;
		int pipeType; // 0:X, 1:|, 2:!
};

class PipeInfo{
	public:
		int count;
		int fd[2];
};

vector<PipeInfo> pipeList;
vector<Cmd> cmdList;

void Cmd::putVar(string com, char m, int n){
	command = com;
	mark = m;
	lineToPipe = n;
}

void Cmd::split(){
	stringstream ss(command);
	string temp;
	vector<string> tempToken;

	while(ss >> temp)
		tempToken.push_back(temp);

	tokens = tempToken;
}

int Cmd::comType(){
	int t; //command type
	string e = tokens.at(0); //execute command

	if(e == "printenv")
		t = 1;
	else if(e == "setenv")
		t = 2;
	else if(e == "exit" || e == "EOF")
		t = 3;
	else
		t = 4;
	return t;
}

void sig_fork(int signo){
	int status;
	while(waitpid(-1,&status,WNOHANG) > 0){}
	// -1 -> wait any child process
	//WHOHANG -> return immediately without wait
	// == 0 , use WHOHANG and no childPid return
}

void Init_shell(){
	clearenv();
	setenv("PATH","bin:.", 1);
	signal(SIGCHLD,sig_fork); //call wait() catch state
}

void Input(string input){
	//string input;
	Cmd cmds; //record process command
	int f = 0; //find
	int set = 0; 
	int num = 0; //pipe to n 
	int mark = 0; //mark |>!

	while(1){
		f = input.find_first_of("|!",set);
		if(f != set){
			num = 0;
			mark = input[f];
			if(mark == '|')
				cmds.pipeType = 1;
			else if(mark == '!')
				cmds.pipeType = 2;

			string temp = input.substr(set,f-set); //cut
			//cout<<f<<" "<<set<<":"<<temp<<endl;
			//num
			if(isdigit(input[f+1])){
				f++;
				int first = f;
				while(isdigit(input[f+1])){	
					f++;
				}
				num = atoi(input.substr(first,f-first+1).c_str());
			}

			//Initialize pipe object
			cmds.fdIn = 0; //false
			cmds.fdOut = 0;
			cmds.fd_in = 0;
			cmds.fd_out = 0;

			if(temp.size() != 0){
				temp.erase(0, temp.find_first_not_of(" "));
				temp.erase(temp.find_last_not_of(" ")+1);

				cmds.putVar(temp, mark, num); //command,mark,num(pipe)
				cmds.split();
				/*for(int i=0; i<cmds.tokens.size(); i++)
					cout<<(cmds.tokens).at(i)<<" ";
				cout<<cmds.mark<<endl;*/
				cmdList.push_back(cmds);
			}
		}
		if(f == string::npos)
			break;
		set = f+1;
	}		
}

void Pipee(int i){
	PipeInfo nPipe;
	nPipe.count = 0;
	
	if(cmdList.at(i).lineToPipe > 0 && i == cmdList.size()-1)
		nPipe.count = cmdList.at(i).lineToPipe;

	if(cmdList.at(i).mark == '|')
		cmdList.at(i).pipeType = 1;
	else if(cmdList.at(i).mark == '!')
		cmdList.at(i).pipeType = 2;
	else
		cmdList.at(i).pipeType = 0;
	 //number pipe
	if(nPipe.count > 0){
		/*cout<<"number pipe"<<endl;
		cout<<cmdList.at(i).pipeType<<endl;*/
		bool f = 0;
		for(int j=0; j<pipeList.size(); j++){
			if(nPipe.count == pipeList.at(j).count){
				cmdList.at(i).fd_out = pipeList.at(j).fd[1];
				f = 1;
				break;
			}
		}
		if(f == 0){ //create pipe
			pipe(nPipe.fd);
			cmdList.at(i).fd_out = nPipe.fd[1]; //write
			pipeList.push_back(nPipe);		
		}
	}
	else{ 
		/*cout<<"general pipe"<<endl;
		cout<<cmdList.at(i).pipeType<<endl;*/
		if(cmdList.at(i).pipeType != 0){
			nPipe.count = -1;
			pipe(nPipe.fd);
			cmdList.at(i).fd_out = nPipe.fd[1]; //write
			pipeList.push_back(nPipe);
		}
	}
	if(cmdList.at(i).pipeType != 0)
		cmdList.at(i).fdOut = 1;
}

void changeType(char *com[], int sizee, int i){
	vector<string> tok = cmdList.at(i).tokens;

	for(int j=0; j<sizee; j++){
		com[j] = (char*)tok.at(j).c_str();
	}
	com[sizee] = NULL;
	
}

int findIndex(vector<string> &v, string str){
	int i;
	for(i=0;i<v.size();i++)
	if(v[i] == str)
		break;
	if(i == v.size())
		i = -1;
	return i;
}

void Execommand(int i){
		int type = cmdList.at(i).comType();
		char* com[1000];
		//cout<<"type: "<<type<<endl;
		if(type == 1){ //printenv
			char* envName = getenv(cmdList.at(i).tokens.at(1).c_str());
			if(envName != NULL)
				cout<< envName <<endl;
		}
		else if(type == 2){ //setenv
			setenv(cmdList.at(i).tokens.at(1).c_str(),
			       cmdList.at(i).tokens.at(2).c_str(), 1);
		}
		else if(type == 3){ //exit || EOF
			exit(0);
		}
		else if(type == 4){
			int status;
			FORK:
			pid_t child_pid = fork();			

			if(child_pid < 0){
				while(waitpid(-1,&status,WNOHANG) > 0){}
				goto FORK;

			}

			//start fork process
			if(child_pid == 0){
				//cout<<"im in child process"<<endl;
				if(cmdList.at(i).fdIn == 1)
					dup2(cmdList.at(i).fd_in, STDIN_FILENO);
				if(cmdList.at(i).fdOut == 1){
					dup2(cmdList.at(i).fd_out, STDOUT_FILENO); //type=1, |
					if(cmdList.at(i).pipeType == 2) //type=2, !
						dup2(cmdList.at(i).fd_out, STDERR_FILENO);
				}

				for(int j=0; j<pipeList.size(); j++){
					close(pipeList.at(j).fd[0]);
					close(pipeList.at(j).fd[1]);
				}
				int index = findIndex(cmdList.at(i).tokens,">");
				//cout<<"index:"<<index<<endl;
				if(index == -1) //not find
					changeType(com, cmdList.at(i).tokens.size(), i);
				else{
					string file = cmdList.at(i).tokens.back().data();
					//cout<<"file: "<<file<<endl;
					int fd = open(file.c_str(), O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC ,0600);
					dup2(fd, STDOUT_FILENO);
					close(fd);
					changeType(com, index, i);
				}

				//cout<<"com: "<<com[0]<<endl;
				if(execvp(com[0], com) == -1)
					cerr<<"Unknown command: ["<<com[0]<<"]."<<endl;
				exit(1);
			}
			else{
				if(cmdList.at(i).fdIn == 1){
					for(int j=0; j<pipeList.size(); j++){
						if(pipeList.at(j).fd[0] == cmdList.at(i).fd_in){
							//parentClose
							close(pipeList.at(j).fd[0]);
							close(pipeList.at(j).fd[1]);
							pipeList.erase(pipeList.begin()+j);
							break;
						}
					}
				}
				if(cmdList.at(i).pipeType == 0){
					//cout<<"1: "<<child_pid<<endl;
					waitpid(child_pid,&status, 0);
				}
				else{
					//cout<<"2: "<<child_pid<<endl;
					waitpid(-1,&status,WNOHANG);
					//waitpid(-1, NULL ,WNOHANG);
				}
			}
						
		}
}

int main() {	
	Init_shell();
	while(1){
		string input;
		cout<<"% ";
		getline(cin,input);

		if(input.empty() == true || input.find_first_not_of(" ",0) == string::npos)
			continue;

		Input(input);
		
		for(int i=0; i<cmdList.size(); i++){
			Pipee(i); //pipe
			if(i == 0){
				for(int j=0; j<pipeList.size(); j++){
					if(pipeList.at(j).count == 0){
						cmdList.at(i).fd_in = pipeList.at(j).fd[0];
						cmdList.at(i).fdIn = 1;
						break;
					}
				}
			}
			else{
				for(int j=0; j<pipeList.size(); j++){
					if(pipeList.at(j).count == -1){ //number pipe finish
						cmdList.at(i).fd_in = pipeList.at(j).fd[0];
						break;
					}
				}
				cmdList.at(i).fdIn = 1;
			}
			Execommand(i);  //fork
		}
		for(int i=0; i<pipeList.size(); i++)
			pipeList.at(i).count--;
		cmdList.clear();
	}	
}

