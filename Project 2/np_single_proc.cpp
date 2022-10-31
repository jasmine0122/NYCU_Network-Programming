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

using namespace std;

class envVal{
	public:
		string name;
		string value;
};

class PipeInfo{
	public:
		int count;
		int fd[2];
};

class User{
	public:
		bool hasUser;
		bool done; //check is the user need to be send "% "
		int ssock;
		int ID;
		string address;
		string name;
		vector<PipeInfo> pipes;
		vector<envVal> envVals;
};

class UserInfo{
	public:
		int sourceID;
		int targetID;
		int fd[2];
};

class Cmd{
	public:
		string intact; //intact input
		string command;
		char mark;
		int lineToPipe;
		vector<string> tokens;
		void putVar(string com, char m, int n); //command,mark,num(pipe)
		int comType(int i);
		void split();
		int fd_in, fd_out;
		bool fdIn, fdOut;
		bool userSend, userReceive;
		int send_to_ID, receive_from_ID;
		int pipeType; // 0:X, 1:|, 2:!
		int sendflag;
		int receiveflag;
};

/*----------------*/
fd_set activefds, readfds;
User users[30];
int servingID;
vector<UserInfo> userPipes;
const int FD_NULL = open("/dev/null", O_RDWR);
vector<Cmd> cmdList;
int byebye = 0;
/*----------------*/

void Cmd::putVar(string com, char m, int n){
	command = com;
	mark = m;
	lineToPipe = n;
}

int s = 0;
int r = 0;
void Cmd::split(){
	stringstream ss(command);
	string temp;
	vector<string> tempToken;
	int f = 0;
	int n = 0; //num

	while(ss >> temp){
		if(temp[0] == '>'){
			while(isdigit(temp[f+1]))
				f++;
			if(f == 0)
				tempToken.push_back(temp);
			else				
				s = atoi(temp.substr(1,f).c_str());
			continue;
		}
		else if(temp[0] == '<'){
			while(isdigit(temp[f+1]))
				f++;
			r = atoi(temp.substr(1,f).c_str());
			continue;
		}
		else
			tempToken.push_back(temp);
	}

	tokens = tempToken;
}

int Cmd::comType(int i){
	int t; //command type
	string e = cmdList.at(i).tokens.at(0); //execute command

	if(e == "printenv")
		t = 1;
	else if(e == "setenv")
		t = 2;
	else if(e == "exit" || e == "EOF")
		t = 3;
	else if(e == "who")
		t = 4;
	else if(e == "tell")
		t = 5;
	else if(e == "yell")
		t = 6;
	else if(e == "name")
		t = 7;
	else
		t = 8;
	return t;
}

void sig_fork(int signo){
	int status;
	while(waitpid(-1,&status,WNOHANG) > 0){}
	// -1 -> wait any child process
	//WHOHANG -> return immediately without wait
	// == 0 , use WHOHANG and no childPid return
}

void Init_shell(int ID){
	clearenv();
	for (int i=0; i < users[ID].envVals.size(); i++){
		setenv(users[ID].envVals[i].name.data(), users[servingID].envVals[i].value.data(), 1);
	}
}

void Input(string input){
	//string input;
	Cmd cmds; //record process command
	int f = 0; //find
	int set = 0; 
	int num = 0; //pipe to n 
	char mark = 0; //mark |>!

	while(1){
		s = 0;
		r = 0;
		f = input.find_first_of("|!",set);
		if(f != set){
			num = 0;
			if(input[f] == '!'){
				if(isdigit(input[f+1]))
					mark = input[f];
				
				else{
					mark = 0;
					f = -1;
				}
			}
			else
				mark = input[f];

			if(mark == '|')
				cmds.pipeType = 1;
			else if(mark == '!' )
				cmds.pipeType = 2;
			else
				cmds.pipeType = 0;

			string temp = input.substr(set,f-set);

			if(isdigit(input[f+1]) && cmds.pipeType != 0){
				f++;
				int first = f;
				while(isdigit(input[f+1]))	
					f++;
				num = atoi(input.substr(first,f-first+1).c_str());		
			}

			cmds.fdIn = 0; //false
			cmds.fdOut = 0;
			cmds.fd_in = 0;
			cmds.fd_out = 0;
			cmds.userSend = 0;
			cmds.userReceive = 0;
			cmds.send_to_ID = 0;
			cmds.receive_from_ID = 0;
			cmds.intact = input;
			cmds.sendflag = 0;
			cmds.receiveflag = 0;

			if(temp.size() != 0){
				temp.erase(0, temp.find_first_not_of(" "));
				temp.erase(temp.find_last_not_of(" ")+1);
				cmds.putVar(temp, mark, num); //command,mark,num(pipe)
				cmds.split();
				cmds.sendflag = s;
				cmds.receiveflag = r;

				cmdList.push_back(cmds);
			}
		}

		if(f == string::npos){
			break;
		}
		set = f+1;
	}
	//cout<<"input OK"<<endl;		
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

	if(nPipe.count > 0){ 
		bool f = 0;
		for(int j=0; j<users[servingID].pipes.size(); j++){
			if(nPipe.count == users[servingID].pipes.at(j).count){
				cmdList.at(i).fd_out = users[servingID].pipes.at(j).fd[1];
				f = 1;
				break;
			}
		}
		if(f == 0){ //create pipe
			pipe(nPipe.fd);
			cmdList.at(i).fd_out = nPipe.fd[1]; //write
			users[servingID].pipes.push_back(nPipe);		
		}
	}
	else
	{ 

		if(cmdList.at(i).pipeType != 0){
			nPipe.count = -1;
			pipe(nPipe.fd);
			cmdList.at(i).fd_out = nPipe.fd[1]; //write
			users[servingID].pipes.push_back(nPipe);
		}
	}

	if(cmdList.at(i).pipeType != 0)
		cmdList.at(i).fdOut = 1;

	if (cmdList.at(i).sendflag > 0){ //user pipe send
		cmdList.at(i).userSend = 1;
		cmdList.at(i).send_to_ID = cmdList.at(i).sendflag - 1;
	}

	if (cmdList.at(i).receiveflag > 0){ //user pipe receive
		cmdList.at(i).userReceive = 1;
		cmdList.at(i).receive_from_ID = cmdList.at(i).receiveflag - 1;
	}

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

void broadcast(int *sourceID, int *targetID, string m){
	const char *msg = m.c_str();
	char unit;
	if (targetID == NULL){
		for (int i=0; i<30; i++){
			if (users[i].hasUser)
				write(users[i].ssock, msg, sizeof(unit)*m.length());
		}
	} 
	else 
		write(users[*targetID].ssock, msg, sizeof(unit)*m.length());
}

string getName(string n){
	if (n == "")
		return "(no name)";
	else 
		return n;
}

void Init_userData(int i){
	users[i].hasUser = 0;
	users[i].ssock = 0;
	users[i].address = "";
	users[i].done = 1;
	users[i].ID = 0;
	users[i].name = "";
	users[i].envVals.clear();

	envVal NewEnv;
	NewEnv.name = "PATH";
	NewEnv.value = "bin:.";
	users[i].envVals.push_back(NewEnv);

	for (int j=0; j<users[i].pipes.size(); j++){
		close(users[i].pipes.at(j).fd[0]);
		close(users[i].pipes.at(j).fd[1]);
	}
	users[i].pipes.clear();
	for (int j=0; j<userPipes.size(); j++){
		if (userPipes[j].sourceID == i || userPipes.at(j).targetID == i){
			close(userPipes.at(j).fd[0]);
			close(userPipes.at(j).fd[1]);
			userPipes.erase(userPipes.begin()+j);
		}
	}
}

int status;
int Execommand(int i){
		int type = cmdList.at(i).comType(i);
		char* com[1000];

		if(type == 1){ //printenv
			char *envName = getenv(cmdList.at(i).tokens.at(1).data());
			if(envName != NULL){
				string s(envName);
				broadcast(NULL, &servingID, s + "\n");
			}
		}
		else if(type == 2){ //setenv
			envVal new_env;
			new_env.name = cmdList.at(i).tokens.at(1);
			new_env.value = cmdList.at(i).tokens.at(2);
			users[servingID].envVals.push_back(new_env);

			setenv(new_env.name.data(), new_env.value.data(), 1);
		}
		else if(type == 3){ //exit || EOF
			//show_logoutMsg
			string m = "*** User '" + getName(users[servingID].name) + "' left. ***\n";
			broadcast(NULL, NULL, m);
			byebye = 1;
		}
		else if(type == 4){ //who
			string m = "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n";
			for (int i=0; i<30; i++){
				if (users[i].hasUser == 1){
					m += to_string(users[i].ID + 1) + "\t" 
					+ getName(users[i].name) + "\t" + users[i].address;
					if (users[i].ID == servingID){
						m += "\t<-me";
					}
					m += "\n";
				}
			}
			broadcast(NULL, &servingID, m);
		}
		else if(type == 5){ //tell
			string m = "";
			for(int j=2; j<cmdList.at(i).tokens.size();j++){
				if(j != cmdList.at(i).tokens.size() -1)
					m += cmdList.at(i).tokens.at(j) + " ";
				else
					m += cmdList.at(i).tokens.at(j);
			}

			int targetID = stoi(cmdList.at(i).tokens.at(1))-1;

			if (users[targetID].hasUser){
				m = "*** " + getName(users[servingID].name) + " told you ***: " + m + "\n";
				broadcast(NULL, &targetID, m);
			} else {
				m = "*** Error: user #" + to_string(targetID + 1) + " does not exist yet. ***\n";
				broadcast(NULL, &servingID, m);
			}

		}
		else if(type == 6){ //yell
			string t = "";
			for(int j=1; j<cmdList.at(i).tokens.size();j++){
				if(j != cmdList.at(i).tokens.size() -1)
					t += cmdList.at(i).tokens.at(j) + " ";
				else
					t += cmdList.at(i).tokens.at(j);
			}

			string m = "*** " + getName(users[servingID].name) + " yelled ***: " + t + "\n";
			broadcast(NULL, NULL, m);
		}
		else if(type == 7){ //name
			string newName = cmdList.at(i).tokens.at(1).c_str();
			for (int j=0; j<30; j++){
				if (j == servingID)
					continue;
				if (users[j].hasUser && users[j].name == newName){
					string m = "*** User '" + newName + "' already exists. ***\n";
					broadcast(NULL, &servingID, m);
					return 1; //0?1?
				}
			}

			users[servingID].name = newName;
			string m = "*** User from " + users[servingID].address 
				   + " is named '" + users[servingID].name + "'. ***\n";
			broadcast(NULL, NULL, m);
		}
		else if(type == 8){
			FORK:
			pid_t child_pid = fork();			

			if(child_pid < 0){
				while(waitpid(-1,&status,WNOHANG) > 0){}
				goto FORK;

			}

			//start fork process
			if(child_pid == 0){
				if(cmdList.at(i).fdIn == 1)
					dup2(cmdList.at(i).fd_in, STDIN_FILENO);
				if(cmdList.at(i).fdOut == 1){ 
					if(cmdList.at(i).pipeType != 2){ //pipeType 0, 1
						dup2(cmdList.at(i).fd_out, STDOUT_FILENO); 
						dup2(users[servingID].ssock, STDERR_FILENO);
					}
					else if(cmdList.at(i).pipeType == 2){ 
						dup2(cmdList.at(i).fd_out, STDOUT_FILENO);
						dup2(cmdList.at(i).fd_out, STDERR_FILENO);
					}
				}
				else{
					dup2(users[servingID].ssock, STDOUT_FILENO);
					dup2(users[servingID].ssock, STDERR_FILENO);
				}			

				for(int j=0; j<users[servingID].pipes.size(); j++){
					close(users[servingID].pipes.at(j).fd[0]);
					close(users[servingID].pipes.at(j).fd[1]);
				}
				for (int j=0; j<userPipes.size(); j++){
					close(userPipes[j].fd[0]);
					close(userPipes[j].fd[1]);
				}

				int index = findIndex(cmdList.at(i).tokens,">");
				if(index == -1 || cmdList.at(i).userSend == 1){ //not find
					if(cmdList.at(i).pipeType == 0)
						changeType(com, cmdList.at(i).tokens.size(), i);
					else
						changeType(com, cmdList.at(i).tokens.size(), i);
				}
				else{
					string file = cmdList.at(i).tokens.at(cmdList.at(i).tokens.size()-1).data();
					int fd = open(file.c_str(), O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC ,0600);
					dup2(fd, STDOUT_FILENO);
					dup2(fd, STDERR_FILENO);
					close(fd);
					changeType(com, index, i);
				}

				if(execvp(com[0], com) == -1)
					cerr<<"Unknown command: ["<<com[0]<<"]."<<endl;
				exit(1);
			}
			else{
				if(cmdList.at(i).fdIn == 1){
					for(int j=0; j<users[servingID].pipes.size(); j++){
						if(users[servingID].pipes.at(j).fd[0] == cmdList.at(i).fd_in){
							//parentClose
							close(users[servingID].pipes.at(j).fd[0]);
							close(users[servingID].pipes.at(j).fd[1]);
							users[servingID].pipes.erase(users[servingID].pipes.begin()+j);
							break;
						}
					}
					for (int j=0; j<userPipes.size(); j++){
						if (userPipes[j].fd[0] == cmdList.at(i).fd_in){
							close(userPipes[j].fd[0]);
							close(userPipes[j].fd[1]);
							userPipes.erase(userPipes.begin() + j);
							break;
						}
					}
				}
				if(cmdList.at(i).fdOut == 1)
					waitpid(-1, &status, WNOHANG);
				else
					waitpid(child_pid, &status, 0);
			}
						
		}
		return 1;
}

bool checkExist(int &i, int rec, int ser){
	bool e = 0;
	for (int j=0; j<userPipes.size(); j++){
		if (userPipes[j].sourceID == rec && userPipes[j].targetID == ser){
			i = j;
			e = 1;
			break;
		}
	}
	return e;
}

//check and create user pipe, then set the fds in cmdBlock
void handleUserPipe(int i){
	int index;

	if (cmdList.at(i).userReceive == 1){
		if (cmdList.at(i).receive_from_ID < 0 || cmdList.at(i).receive_from_ID > 29 || !users[cmdList.at(i).receive_from_ID].hasUser){
			string m = "*** Error: user #" + to_string(cmdList.at(i).receive_from_ID + 1) + " does not exist yet. ***\n";
			broadcast(NULL, &servingID, m);
			cmdList.at(i).fdIn = 1;
			cmdList.at(i).fd_in = FD_NULL;
		} 
		else {
			if (checkExist(index, cmdList.at(i).receive_from_ID, servingID)){
				string m = "*** " + getName(users[servingID].name) + " (#" + to_string(servingID + 1) + ") just received from "
					 + getName(users[cmdList.at(i).receive_from_ID].name) + " (#" + to_string(cmdList.at(i).receive_from_ID + 1) + ") by '" + cmdList.at(i).intact + "' ***\n";
				broadcast(NULL, NULL, m);
				cmdList.at(i).fdIn = 1;
				cmdList.at(i).fd_in = userPipes[index].fd[0];
			}
			else {
				string m = "*** Error: the pipe #" + to_string(cmdList.at(i).receive_from_ID + 1) + "->#" + to_string(servingID + 1) + " does not exist yet. ***\n";
				broadcast(NULL, &servingID, m);
				cmdList.at(i).fdIn = 1;
				cmdList.at(i).fd_in = FD_NULL;
			}
		}
	}

	if (cmdList.at(i).userSend == 1){
		if (cmdList.at(i).send_to_ID < 0 || cmdList.at(i).send_to_ID > 29 || !users[cmdList.at(i).send_to_ID].hasUser){
			string m = "*** Error: user #" + to_string(cmdList.at(i).send_to_ID + 1) + " does not exist yet. ***\n";
			broadcast(NULL, &servingID, m);
			cmdList.at(i).fdOut = 1;
			cmdList.at(i).fd_out = FD_NULL;
		} 
		else {
			if (checkExist(index, servingID, cmdList.at(i).send_to_ID)){
				string m = "*** Error: the pipe #" + to_string(servingID + 1) + "->#" + to_string(cmdList.at(i).send_to_ID + 1) + " already exists. ***\n";
				broadcast(NULL, &servingID, m);
				cmdList.at(i).fdOut = 1;
				cmdList.at(i).fd_out = FD_NULL;
			} else {
				string m = "*** " + getName(users[servingID].name) + " (#" + to_string(servingID + 1) + ") just piped '" + cmdList.at(i).intact + "' to "
					 + getName(users[cmdList.at(i).send_to_ID].name) + " (#" + to_string(cmdList.at(i).send_to_ID + 1) + ") ***\n";
				broadcast(NULL, NULL, m);

				UserInfo user_nPipe;
				user_nPipe.sourceID = servingID;
				user_nPipe.targetID = cmdList.at(i).send_to_ID;
				pipe(user_nPipe.fd);
				userPipes.push_back(user_nPipe);
				cmdList.at(i).fdOut = 1;
				cmdList.at(i).fd_out = user_nPipe.fd[1];
			}
		}
	}
}

void np_shell(int ID) {	
	servingID = ID;
	if (users[servingID].hasUser == 0)
		return;
	else
		Init_shell(servingID);

	char text[15000];
	string input;

	if (users[servingID].done == 1){
		broadcast(NULL, &servingID, "% ");
		users[servingID].done = 0;
	}

	if (FD_ISSET(users[servingID].ssock, &readfds) > 0){	
		bzero(text, sizeof(text));
		int count = read(users[servingID].ssock, text, sizeof(text));

		if (count > 0){
			input = text;
			if (input[input.length()-1] == '\n'){
				input = input.substr(0, input.length()-1);
				if (input[input.length()-1] == '\r'){
					input = input.substr(0, input.length()-1);
				}
			}
		}
	}
	else
		return;

	if(input.empty() == true || input.find_first_not_of(" ",0) == string::npos){
		users[servingID].done = 1;
		return;			
	}

	Input(input);
	for(int i=0; i<cmdList.size(); i++){
		Pipee(i); //pipe
		if(i == 0){
			for(int j=0; j<users[servingID].pipes.size(); j++){
				if(users[servingID].pipes.at(j).count == 0){
					cmdList.at(i).fd_in = users[servingID].pipes.at(j).fd[0];
					cmdList.at(i).fdIn = 1;
					break;
				}
			}
		}
		else{
			for(int j=0; j<users[servingID].pipes.size(); j++){
				if(users[servingID].pipes.at(j).count == -1){ //number pipe finish
					cmdList.at(i).fd_in = users[servingID].pipes.at(j).fd[0];
					break;
				}
			}
			cmdList.at(i).fdIn = 1;
		}
		//check and create user pipe, then set the fds in cmdBlock
		handleUserPipe(i);

		if(Execommand(i) == 0)  //fork
			return;
	}

	for(int i=0; i<users[servingID].pipes.size(); i++)
		users[servingID].pipes.at(i).count--;

	cmdList.clear();
	users[servingID].done = 1;

	if(byebye == 1){
		byebye = 0;
		FD_CLR(users[servingID].ssock, &activefds);
		close(users[servingID].ssock);
		Init_userData(servingID);
		while(waitpid(-1, &status, WNOHANG) > 0){}
	}	
}

void newUser(int msock){
	struct sockaddr_in cli_addr;
	socklen_t cli_len = sizeof(cli_addr);
	struct timeval timeval = {0, 10};

	for (int i=0; i<30; i++){
		if (users[i].hasUser){
			continue;
		} else {
			int ssock;
			if ((ssock = accept(msock, (struct sockaddr *)&cli_addr, &cli_len))){
				users[i].hasUser = true;
				users[i].ssock = ssock;
				users[i].address = inet_ntoa(cli_addr.sin_addr);
				users[i].address = users[i].address+ ":" + to_string(htons(cli_addr.sin_port));
				users[i].ID = i;
				FD_SET(users[i].ssock, &activefds);

				//show welcome
				string text = "";
				text =  text + "****************************************\n"
				     + "** Welcome to the information server. **\n"
				     + "****************************************\n";
				broadcast(NULL, &users[i].ID, text);

				//login message
				string log = "*** User '" + getName(users[users[i].ID].name) + "' entered from "
					   + users[users[i].ID].address + ". ***\n";
				broadcast(NULL, NULL, log);
				
				break;
			}
		}
	}
}

int create_socket(unsigned short port){
	int msock;
	if ((msock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		cerr << "Socket create fail.\n";
		exit(0);
	}
	struct sockaddr_in sin;

	bzero((char *)&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
    	sin.sin_port = htons(port);

	if (bind(msock, (struct sockaddr *)&sin, sizeof(sin)) < 0){
		cerr << "Socket bind fail.\n";
		exit(0);
	}
	FD_SET(msock, &activefds);
	return msock;
}

int main(int argc, char *argv[]){
	if (argc != 2)
		return 0;

	//initial
	for (int i=0; i<30; i++){
		Init_userData(i);
	}
	FD_ZERO(&activefds);
	FD_ZERO(&readfds);
	servingID = 0;
	userPipes.clear();
	clearenv();

	unsigned short port = (unsigned short)atoi(argv[1]);

	int msock = create_socket(port);
	listen(msock, 30);

	struct timeval timeval = {0, 10};
	while(1){
		memcpy(&readfds, &activefds, sizeof(readfds));

		int max = msock;
		for (int i=0; i<30; i++)
			if (users[i].ssock > max)
				max = users[i].ssock;
		select(max+1, &readfds, NULL, NULL, &timeval);

		//accept
		if (FD_ISSET(msock, &readfds))
			newUser(msock);

		for (int i=0; i<30; i++)
			np_shell(i);
	}
	return 0;
}

