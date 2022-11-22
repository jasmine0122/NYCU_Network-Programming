#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <boost/asio.hpp>

using namespace boost::asio;
using namespace boost::asio::ip;
using namespace std;

struct Request{
    const string env_Variables[9] = {
        "REQUEST_METHOD",
        "REQUEST_URI",
        "QUERY_STRING",
        "SERVER_PROTOCOL",
        "HTTP_HOST",
        "SERVER_ADDR",
        "SERVER_PORT",
        "REMOTE_ADDR",
        "REMOTE_PORT"
    };
    string value[9];
};

//---------------------
Request req;
string httpReq;
int child_pid;
int status = 0;
string temp, path;
char** argvv;
//---------------------

void ParseReq(){
	int Index = httpReq.find('\n', 0);
  	string r1 = httpReq.substr(0, Index-1);
  	string r2 = httpReq.substr(Index+1, httpReq.find('\n', Index+1) - Index - 2);

	int startIndex = 0;
  	Index = r1.find(' ', 0);
  	req.value[0] = r1.substr(0, Index); //REQUEST_METHOD

	startIndex = Index + 1;
	Index = r1.find('?', startIndex);
  	if(Index != -1){
		req.value[1] = r1.substr(startIndex, Index - startIndex); 
		temp = req.value[1] + "?";
        startIndex = Index + 1;
        Index = r1.find(' ', startIndex);
        req.value[2] = r1.substr(startIndex, Index - startIndex); //QUERY_STRING
        req.value[1] = req.value[1] + "?" + req.value[2]; //REQUEST_URI
	}
	else{
		Index = r1.find(' ', startIndex);
		req.value[2] = ""; //QUERY_STRING
		req.value[1] =  r1.substr(startIndex, Index - startIndex);  //REQUEST_URI
		temp = req.value[1] + "?";
	}
	//temp = req.value[1] + "?"; //be used in exec

	startIndex = Index + 1;
    req.value[3] = r1.substr(startIndex); //SERVER_PROTOCOL
    r2 = r2.substr(r2.find(' ', 0) + 1);
    req.value[4] = r2; //HTTP_HOST
    //req.value[5] = r2.substr(0, r2.find(':', 0)); //SERVER_ADDR
    req.value[6] = r2.substr(r2.find(':', 0) + 1);	//SERVER_PORT
}

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
      		void do_read(){
        		auto self(shared_from_this());
        		socket_.async_read_some(buffer(data_, max_length), [this, self](boost::system::error_code ec, size_t length){

          			if (!ec){
						//do_write(length);
                  		data_[length] = '\0';
                  		httpReq = string(data_);
                  		ParseReq();

                    	while((child_pid = fork()) < 0){
                        	while(waitpid(-1, NULL, WNOHANG) > 0){}
                    	}

                    	switch (child_pid){
                        	case 0:
								//set_env
								req.value[5] = socket_.local_endpoint().address().to_string();
								//req.value[6] = to_string(htons(socket_.local_endpoint().port()));
								req.value[7] = socket_.remote_endpoint().address().to_string();
								req.value[8] = to_string(htons(socket_.remote_endpoint().port()));
                            	for (int i=0; i<9; i++){
        							setenv(req.env_Variables[i].data(), req.value[i].data(), 1);
								}
								//dup
								dup2(socket_.native_handle(), STDIN_FILENO);
								dup2(socket_.native_handle(), STDOUT_FILENO);
								dup2(socket_.native_handle(), STDERR_FILENO);
        						close(socket_.native_handle());

								//exec
                            	cout << "HTTP/1.1 200 OK\r\n";
								fflush(stdout);
                            	path = "." + temp.substr(0, temp.find('?', 0));
								cout<<path<<endl;
                            	if (execv(path.c_str(), argvv) == -1){
            						//cerr<<"Exec cgi Fail"<<endl;
            						exit(1);
                            	}
                            	exit(0);

                        	default:
							    socket_.close();
            					waitpid(child_pid, &status, 0);
                    	}	
          			}
        		});
  			}

    		void do_write(size_t length){
    			auto self(shared_from_this());
    			async_write(socket_, buffer(data_, length), [this, self](boost::system::error_code ec, size_t /*length*/){
          			if (!ec){
            				do_read();
          			}
        		});
  			}

  	tcp::socket socket_;
  	enum { max_length = 1024 };
  	char data_[max_length];
};

class server{
	public:
  		server(io_context& io_context, short port)
    			: acceptor_(io_context, tcp::endpoint(tcp::v4(), port)){
    			do_accept();
  		}

	private:
  		void do_accept(){
    			acceptor_.async_accept([this](boost::system::error_code ec,  tcp::socket socket){
          		if (!ec){
            			make_shared<session>(move(socket))->start();
          		}
          		do_accept();
        	});
		}
  		tcp::acceptor acceptor_;
};

int main(int argc, char* argv[])
{
	argvv = argv;
  	try{
    	if (argc != 2){
      		cerr << "Usage: async_tcp_echo_server <port>\n";
      		return 1;
    	}

    	io_context io_context;
    	server s(io_context, atoi(argv[1]));
    	io_context.run();
  	}
  	catch (exception& e){
    	cerr << "Exception: " << e.what() << "\n";
  	}
  	return 0;
}
