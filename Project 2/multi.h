#ifndef _MULTI_H_
#define _MULTI_H_

#define SHMKEY_Users 10701
#define SHMKEY_BroadcastMsg 10702

class User{
	public:
		bool hasUser;
		bool done; //check is the user need to be send "% "
		int pid;
		char address[25];
		char name[50];
};


class BroadcastMsg{
	public:
		char str[1000];
};

struct PipeInfo{
    int fd[2];     // 0 = read, 1 = write
    int count;
};

struct UserInfo{
    int from;
    int to;
    int fd[2];
};

void np_shell();
#endif
