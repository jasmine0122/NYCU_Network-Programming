#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fstream>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/algorithm/string.hpp>

#define CONNECT 1
#define BIND 2
#define ACCEPT 90
#define REJECT 91

using namespace boost::asio;
using namespace boost::asio::ip;
using namespace std;
using namespace boost::placeholders;

class sockInfo{
	public:
		unsigned char VN;
		unsigned char CD;
		unsigned char DSTPORT[2];
		unsigned char DSTIP[4];
		array<unsigned char, 8> build_reply(int cd, int status);
		void do_print();
		void do_FireWall();
		int RP;
};

//---------------------
io_context io_Context;
string sourceIp;
string sourcePort;
string dstIp;
string dstPort;
//string USERID;
string DNAME;
short Bport = 27219;
ifstream socksConf;
string filename = "./socks.conf";
string line;
string url;
int pid, status;
//char** argvv;
bool f1 = false, f2 = false, f3 = false, f4 = false;
//---------------------

void sig_fork(int signo){
	int status;
	while(waitpid(-1,&status,WNOHANG) > 0){}
	// -1 -> wait any child process
	//WHOHANG -> return immediately without wait
	// == 0 , use WHOHANG and no childPid return
}

void sockInfo::do_print(){
	cout<<"<S_IP>: "<<sourceIp<<endl;
    cout<<"<S_PORT>: "<<sourcePort<<endl;
    cout<<"<D_IP>: "<<dstIp<<endl;
    cout<<"<D_PORT>: "<<dstPort<<endl;
    if (CD == CONNECT){
        cout<<"<Command>: CONNECT"<<endl;
	}
    else if (CD == BIND){
        cout<< "<Command>: BIND"<<endl;
	}
	//cout<<RP<<endl;
	if (RP == REJECT){
        cout<<"<Reply>: Reject"<<endl;
	}
	else if(RP == ACCEPT){
        cout<<"<Reply>: Accept"<<endl;
	}
	cout<<endl;
}

array<unsigned char, 8> sockInfo::build_reply(int cd, int status){
	array<unsigned char, 8> buf;
	if(status == ACCEPT)
		buf[1] = (unsigned char)ACCEPT;
	else if(status == REJECT)
		buf[1] = (unsigned char)REJECT;

	if(cd == BIND){
		buf[0] = (unsigned char)0;
		buf[2] = DSTPORT[0];
		buf[3] = DSTPORT[1];
		buf[4] = DSTIP[0];
		buf[5] = DSTIP[1];
		buf[6] = DSTIP[2];
		buf[7] = DSTIP[3];
	}
	else if(cd == CONNECT){
		for(int i=0 ; i<8 ; i++){
			if(i != 1)
				buf[i] = (unsigned char)0;
		}
	}
	return buf;
}

void sockInfo::do_FireWall(){
	RP = 91;
	socksConf.open(filename);
	if(socksConf){
		string regex = "";
		if(CD == CONNECT)
			regex ="c ";
		else if(CD == BIND)
			regex = "b ";

		while(getline(socksConf, line)){
			if(line.find(regex) != string::npos){
				vector<string> c1, c2, c3;
				boost::split(c1, line, boost::is_any_of(" "),boost::token_compress_on);
				vector<string> temp = c1;//"b","*.*.*.*"
				boost::split(c2, temp[2], boost::is_any_of("."),boost::token_compress_on);
				vector<string> ipConfig = c2;//"127","0","0","1"
				//cout << "ipconfig: "<<ipConfig[0]<<"."<<ipConfig[1]<<"."<<ipConfig[2]<<"."<<ipConfig[3]<<endl;
				boost::split(c3, dstIp, boost::is_any_of("."),boost::token_compress_on);	
				vector<string> ipDest = c3;
				//cout << "ipdest: "<<ipDest[0]<<"."<<ipDest[1]<<"."<<ipDest[2]<<"."<<ipDest[3]<<endl;

				if( (ipConfig[0] == "*") || (ipConfig[0] == ipDest[0]) )
					f1 = 1;
				if( (ipConfig[1] == "*") || (ipConfig[1] == ipDest[1]) )
					f2 = 1;
				if( (ipConfig[2] == "*") || (ipConfig[2] == ipDest[2]) )
					f3 = 1;
				if( (ipConfig[3] == "*") || (ipConfig[3] == ipDest[3]) )
					f4 = 1;
				if( f1 && f2 && f3 && f4){
					RP = 90;
					break;
				}
			}
		}
		socksConf.close();
	}
	else{
		RP = 91;
		cerr<< "can't open [ " << filename << " ]."<<endl;
	}
}


class bind_session
    : public std::enable_shared_from_this<bind_session>{
		public:
			bind_session(sockInfo sockInput, tcp::socket socket_Input)
			:sock(sockInput),socket_S(move(socket_Input)), acceptor_(io_Context, tcp::endpoint(tcp::v4(), 0)){}
			void start(){
					do_listen();
			}
		private:
			sockInfo sock;
			tcp::socket socket_S;
			tcp::socket socket_D{io_Context};
			tcp::resolver resolve_{io_Context};
			tcp::acceptor acceptor_;
			enum { max_length = 2048 };
			array<unsigned char, 8>  reply;
			array<unsigned char, 8>  reply2;
			array<char, max_length> sS_buf;
			array<char, max_length> sD_buf;
			array<char, max_length> rS_buf;
			array<char, max_length> rD_buf;
			unsigned int Port;
			string DestIp;

			//from destination to source
			void do_sS_handler(boost::system::error_code ec, size_t length){
				if(!ec){
					read_Dest();
				}
			}

			void send_Source(size_t length){
				async_write(socket_S, buffer(rD_buf, length), boost::bind(&bind_session::do_sS_handler, shared_from_this(), _1, _2));
			}

			void do_rD_handler(boost::system::error_code ec, size_t length){
				if(!ec){
					send_Source(length);
				}
				else{
					if(socket_S.is_open())
						socket_S.close(); 
					if(socket_D.is_open())
						socket_D.close();					
				}
			}

			void read_Dest(){
				rD_buf.fill('\0');
				socket_D.async_read_some(buffer(rD_buf), boost::bind(&bind_session::do_rD_handler, shared_from_this(), _1, _2));
			}

			//from source to destination
			void do_sD_handler(boost::system::error_code ec, size_t length){
				if(!ec){
					read_Source();
				}
			}

			void send_Dest(size_t length){
				async_write(socket_D, buffer(rS_buf, length), boost::bind(&bind_session::do_sD_handler, shared_from_this(), _1, _2));
			}

			void do_rS_handler(boost::system::error_code ec, size_t length){
				if(!ec){
					send_Dest(length);
				}
				else{
					if(socket_S.is_open())
						socket_S.close(); 
					if(socket_D.is_open())
						socket_D.close();
				}
			}

			void read_Source(){
				rS_buf.fill('\0');
				socket_S.async_read_some(buffer(rS_buf),boost::bind(&bind_session::do_rS_handler, shared_from_this(), _1, _2));
			}


			void do_send91_handler(boost::system::error_code ec,size_t length){
				if(!ec){
					socket_S.close();
					socket_D.close();
				}
			}

			void do_send2_handler(boost::system::error_code ec,size_t length){
				if(!ec){
					read_Dest();
					read_Source();
				}
			}

			void do_send_second(){
				DestIp = socket_D.remote_endpoint().address().to_string();
				Port = socket_D.local_endpoint().port();
				reply2 = sock.build_reply(CONNECT, ACCEPT);
				reply2[2] = (unsigned char)(Port/256);
				reply2[3] = (unsigned char)(Port%256);
				if(DestIp == dstIp){
					reply2[1] = (unsigned char)90;
					async_write(socket_S, buffer(reply2.data(), reply2.size()), boost::bind(&bind_session::do_send2_handler, shared_from_this(), _1, _2));
				}
				else{
					reply2[1] = (unsigned char)91;
					async_write(socket_S, buffer(reply2.data(), reply2.size()), boost::bind(&bind_session::do_send91_handler, shared_from_this(), _1, _2));					
				}
			}

			void do_send1_handler(boost::system::error_code ec, size_t length){}
			void do_send_first(){
				Port = acceptor_.local_endpoint().port();
				reply = sock.build_reply(CONNECT, ACCEPT);
				reply[2] = (unsigned char)(Port/256);
				reply[3] = (unsigned char)(Port%256);
				async_write(socket_S, buffer(reply.data(), reply.size()), boost::bind(&bind_session::do_send1_handler, shared_from_this(), _1, _2));

			}

			void do_accept(){
				auto self(shared_from_this());
				acceptor_.async_accept(socket_D,[self, this](boost::system::error_code ec){
					if(!ec){
						do_send_second();
					}
				});
			}

			void do_listen(){
				do_accept();
				do_send_first();
        	}
};

class connect_session
    : public std::enable_shared_from_this<connect_session>{
		public:
			connect_session(sockInfo sockInput, tcp::socket socket_Input)
			:sock(sockInput),socket_S(move(socket_Input)){}
			void start(){
					do_resolve();
			}
		private:
			sockInfo sock;
			tcp::socket socket_S;
			tcp::socket socket_D{io_Context};
			tcp::resolver resolve_{io_Context};
			enum { max_length = 2048 };
			array<unsigned char, 8>  reply;
			array<char, max_length> sS_buf;
			array<char, max_length> sD_buf;
			array<char, max_length> rS_buf;
			array<char, max_length> rD_buf;

			//from destination to source
			void do_sS_handler(boost::system::error_code ec, size_t length){
				if(!ec){
					read_Dest();
				}
			}

			void send_Source(size_t length){
				async_write(socket_S, buffer(rD_buf, length), boost::bind(&connect_session::do_sS_handler, shared_from_this(), _1, _2));
			}

			void do_rD_handler(boost::system::error_code ec, size_t length){
				if(!ec){
					send_Source(length);
				}
			}

			void read_Dest(){
				rD_buf.fill('\0');
				socket_D.async_read_some(buffer(rD_buf), boost::bind(&connect_session::do_rD_handler, shared_from_this(), _1, _2));
			}

			//from source to destination
			void do_sD_handler(boost::system::error_code ec, size_t length){
				if(!ec){
					read_Source();
				}
			}

			void send_Dest(size_t length){
				async_write(socket_D, buffer(rS_buf, length), boost::bind(&connect_session::do_sD_handler, shared_from_this(), _1, _2));
			}

			void do_rS_handler(boost::system::error_code ec, size_t length){
				if(!ec){
					send_Dest(length);
				}
			}

			void read_Source(){
				rS_buf.fill('\0');
				socket_S.async_read_some(buffer(rS_buf), boost::bind(&connect_session::do_rS_handler, shared_from_this(), _1, _2));
			}

			void do_send_reply(boost::system::error_code ec, size_t length){
				if(!ec){
					read_Source();
					read_Dest();
				}
			}

			void do_con_handler(boost::system::error_code ec){
				if(!ec){
					reply = sock.build_reply(CONNECT, ACCEPT);
					async_write(socket_S, buffer(reply.data(), reply.size()), boost::bind(&connect_session::do_send_reply, shared_from_this(), _1, _2));
				}
			}

			void do_connect(tcp::resolver::iterator it){
				socket_D.async_connect(*it, boost::bind(&connect_session::do_con_handler, shared_from_this(), _1));
			}

			void do_res_handler(boost::system::error_code ec, tcp::resolver::iterator it){
				if(!ec){
					do_connect(it);
				}
			}

			void do_resolve(){
				tcp::resolver::query res_que(dstIp, dstPort);
				resolve_.async_resolve(res_que, boost::bind(&connect_session::do_res_handler, shared_from_this(), _1, _2));
        	}
};


class session
	: public enable_shared_from_this<session>{
    	public:
      		session(tcp::socket socket)
        		: socket_(move(socket)){
      			}

      		void start(){
        		do_read();
      		}

    	private:
			tcp::socket socket_;
  			enum { max_length = 2048 };
			char recv_Buf[max_length];

			void send_reply_handler(boost::system::error_code ec, size_t length){};

			void do_read_handler(boost::system::error_code ec, size_t length){
				if(!ec){
						sourceIp = socket_.remote_endpoint().address().to_string();
						sourcePort = to_string(socket_.remote_endpoint().port());
						//parseReq(recv_Buf);

						sockInfo s;
						//version
    					s.VN = recv_Buf[0];
						//command
						s.CD = recv_Buf[1];
						//port
						s.DSTPORT[0] = recv_Buf[2];
						s.DSTPORT[1] = recv_Buf[3];
						//ip
						s.DSTIP[0] = recv_Buf[4];
						s.DSTIP[1] = recv_Buf[5];
						s.DSTIP[2] = recv_Buf[6];
						s.DSTIP[3] = recv_Buf[7];
						dstIp = to_string((int)s.DSTIP[0]) + "." + to_string((int)s.DSTIP[1]) + "." + to_string((int)s.DSTIP[2]) + "." + to_string((int)s.DSTIP[3]);
 						dstPort = to_string((int)s.DSTPORT[0]*256 + (int)s.DSTPORT[1]);
						string USERID(recv_Buf + 8);

						//if IP == 0.0.0.X
						if((int)s.DSTIP[0] == 0 && (int)s.DSTIP[1] == 0 && (int)s.DSTIP[2] == 0){
							tcp::resolver resolver(io_Context);
							tcp::resolver::query que(DNAME, "");
							tcp::resolver::iterator it;
							DNAME.assign(recv_Buf + 8 + USERID.length() + 1);
							//cout << "domain name:" + DNAME << endl;
							//s.url = DNAME;
							for(it = resolver.resolve(que); it != tcp::resolver::iterator(); ++it){
								tcp::endpoint ep = *it;
								if(ep.address().is_v4())
									dstIp = ep.address().to_string();
							}
						}

						//check if VN is correct
						if(s.VN != 4){
							return;
						}	

						s.do_FireWall();
						//check if connect or bind or accept or reject
						if(s.RP == ACCEPT){
							if(s.CD == CONNECT){
								s.do_print();
								make_shared<connect_session>(s, move(socket_))->start();
							}
							else if(s.CD == BIND){
								s.do_print();
								make_shared<bind_session>(s, move(socket_))->start();
							}
						}
						else if(s.RP == REJECT){
							if(s.CD == CONNECT )
								s.do_print();
							else if(s.CD == BIND)
								s.do_print();
							array<unsigned char, 8> reply = s.build_reply(s.CD, s.RP);
							socket_.async_write_some(buffer(reply.data(), reply.size()), boost::bind(&session::send_reply_handler, shared_from_this(),_1,_2));	
						}
				}
			}  

      		void do_read(){
				bzero(recv_Buf, 0);
        		socket_.async_read_some(buffer(recv_Buf, max_length), boost::bind(&session::do_read_handler, shared_from_this(), _1, _2));
  			}
};

class server{
	public:
  		server(io_context& io_context, short port)
    			: acp_(io_context, tcp::endpoint(tcp::v4(), port)){
    			do_accept();
  		}

	private:
  		void do_accept(){
    			acp_.async_accept([this](boost::system::error_code ec, tcp::socket socket){
          		if (!ec){
						io_Context.notify_fork(boost::asio::io_service::fork_prepare);
						pid = fork();
						if(pid == 0){
							io_Context.notify_fork(boost::asio::io_service::fork_child);						
            				make_shared<session>(move(socket))->start();
						}
						else{
							io_Context.notify_fork(boost::asio::io_service::fork_parent);
							socket.close(); //parents close socket
							do_accept();
						}						
          		}
          		//do_accept();
        	});
		}

  		tcp::acceptor acp_;
};

int main(int argc, char* argv[])
{
	//argvv = argv;
  	try{
    	if (argc != 2){
      		return 1;
    	}
		signal(SIGCHLD, sig_fork);
    	server s(io_Context, atoi(argv[1]));
    	io_Context.run();
  	}
  	catch (exception& e){
    	cerr << "Exception: " << e.what() << "\n";
  	}
  	return 0;
}
