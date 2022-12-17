#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <string.h>
#include <fstream>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/algorithm/string.hpp>

#define ACCEPT 90
using namespace boost::asio::ip;
using namespace boost::asio;
using namespace std;
using namespace boost::placeholders;


struct HostInfo{
    string host;
    string port;
    string file;
    bool used = 0;
};
HostInfo hostInfo[5];

string socksHost;
string socksPort;
io_context io_Context;

class client : public enable_shared_from_this<client>{
        public:
            client(string id)
                : id_(id){
                    file_.open(("./test_case/" + hostInfo[stoi(id)].file), ios::in);
                }
            
  		    void start(){
    		    do_resolve();
  		    }            
 
        private:
            tcp::socket socket_{io_Context};
            tcp::resolver resolver_{io_Context};
            //tcp::resolver::query que_;
            string id_;
            fstream file_;
            enum { max_length = 15000 };
            char data_[max_length];
            //unsigned char sock_[max_length];
			array<unsigned char, 8> req_;
            array<unsigned char, 8> reply_;
            string data1 = "";            
            string data2 = ""; 
            string text = "";
            string IP = "";

            string do_replace(string t){
                string ret = "";
                for (int i=0; i<(int)t.length(); i++){
                    if (t[i] == '\n')
                        ret += "<br>";
                    else if (t[i] == '\r')
                        ret += "";
                    else if (t[i] == '\'')
                        ret += "&apos;";
                    else if (t[i] == '\"')
                        ret += "&quot;";  
                    else if (t[i] == '&')
                        ret += "&amp;";  
                    else if (t[i] == '<')
                        ret += "&lt;";
                    else if (t[i] == '>')
                        ret += "&gt;";
                    else
                        ret += t[i];
                }
                return ret;
            }

            void do_write(){
                auto self(shared_from_this());
                data2 = "";
                getline(file_, data2);
			    if(data2.find("exit") != string::npos){
				    file_.close();
				    hostInfo[stoi(id_)].used = 0;
			    }
                data2 = data2 + "\n";
                text = do_replace(data2);
                cout << "<script>document.getElementById('s" << id_ << "').innerHTML += '<b>" << text << "</b>';</script>"<<endl;
                text = "";
                //const char *mes = text.c_str();
                async_write(socket_, buffer(data2.c_str(), data2.length()), [this, self](boost::system::error_code ec, size_t /*length*/){
                    if (!ec){
                        if (hostInfo[stoi(id_)].used == 1)
                            do_read();
                    }
                });
            }

            void do_read(){
                auto self(shared_from_this());
                bzero(data_, max_length);
                socket_.async_read_some(buffer(data_, max_length), [this, self](boost::system::error_code ec, size_t length){
                    if (!ec){
                        data1 = "";
                        data1.assign(data_);
					    bzero(data_, max_length);
                        text = do_replace(data1);
                        //strcpy(data_, text.c_str());
                        //turn shell output to web page
                        cout<<"<script>document.getElementById('s" << id_ << "').innerHTML += '"<< text <<"';</script>"<<endl;
                        if (text.find("% ") != string::npos){
                            do_write();
                        }
                        else{
                            do_read();
                        }                    
                    }
                });
            }

            void do_reply_handler(boost::system::error_code ec, size_t length){
                if(!ec){
				    if(reply_[1] == (unsigned char)ACCEPT)
					    do_read();
                }
            }

            void do_reply(){
                reply_.fill(0);
                socket_.async_read_some(buffer(reply_, reply_.size()), boost::bind(&client::do_reply_handler, shared_from_this(), _1, _2));
            }           

            void do_send_handler(boost::system::error_code ec, size_t length){
                if(!ec){
                    do_reply();
                }
            }

            void do_send(){
                //do_read();
                req_[0] = (unsigned char)4;
                req_[1] = (unsigned char)1;
                req_[2] = (unsigned char)(stoi(hostInfo[stoi(id_)].port)/256);
                req_[3] = (unsigned char)(stoi(hostInfo[stoi(id_)].port)%256);

			    tcp::resolver resolver_(io_Context);
			    tcp::resolver::query que(hostInfo[stoi(id_)].host , "");
			    for(tcp::resolver::iterator it = resolver_.resolve(que); it != tcp::resolver::iterator(); ++it){
				    tcp::endpoint ep = *it;
			        if(ep.address().is_v4())
				        IP = ep.address().to_string();
			    }
			    if(IP != ""){
                    vector<string> temp;
                    boost::split(temp, IP, boost::is_any_of("."), boost::token_compress_on);
				    req_[4] = (unsigned char)(stoi(temp[0]));
				    req_[5] = (unsigned char)(stoi(temp[1]));
				    req_[6] = (unsigned char)(stoi(temp[2]));
				    req_[7] = (unsigned char)(stoi(temp[3]));
			    }
			    socket_.async_write_some(buffer(req_.data(), req_.size()), boost::bind(&client::do_send_handler, shared_from_this(), _1, _2));                    
            }

            void do_connect(tcp::resolver::iterator it){
                auto self(shared_from_this());
                socket_.async_connect(*it, [this, self](const boost::system::error_code ec){
                    if (!ec){
                        do_send();
                    }
                });
            }

            void do_resolve(){
                auto self(shared_from_this());
                tcp::resolver::query que_(hostInfo[stoi(id_)].host, hostInfo[stoi(id_)].port);
                resolver_.async_resolve(que_, [this, self](boost::system::error_code ec, tcp::resolver::iterator it){
                    if(!ec){
                        do_connect(it);
                        //do_read();
                    }
                });
            }             
};

void print_HTML(){
	cout<<"content-type: text/html\r\n\r\n";
	cout<<"<!DOCTYPE html>"<<endl;
	cout<<"<html lang=i\"en\">"<<endl;
 	cout<<"<head>"<<endl;
    cout<<"<meta charset=\"UTF-8\" />"<<endl;
	cout<<"<title>NP Project 3 Sample Console</title>"<<endl;
	cout<<"<link"<<endl;
	cout<<"rel=\"stylesheet\""<<endl;
	cout<<"href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\""<< endl;
	cout<<"integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\""<<endl;
	cout<<"crossorigin=\"anonymous\""<<endl;
	cout<<"/>"<<endl;
	cout<<"<link"<<endl;
	cout<<"href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\""<<endl;
	cout<<"rel=\"stylesheet\""<<endl;
	cout<<"/>"<<endl;
	cout<<"<link"<<endl;
	cout<<"rel=\"icon\""<<endl;
	cout<<"type=\"image/png\""<<endl;
	cout<<"href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\""<<endl;
	cout<<"/>"<<endl;
    cout<<"<style>"<<endl;
	cout<<"* {"<<endl;
	cout<<"font-family: 'Source Code Pro', monospace;"<<endl;
	cout<<"font-size: 1rem !important;"<<endl;
	cout<<"}"<<endl;
	cout<<"body {"<<endl;
	cout<<"background-color: #212529;"<<endl;
	cout<<"}"<<endl;
	cout<<"pre {"<<endl;
	cout<<"color: #cccccc;"<<endl;
	cout<<"}"<<endl;
	cout<<"b {"<<endl;
	cout<<"color: #01b468;"<<endl;
	cout<<"}"<<endl;
	cout<<"</style>"<<endl;
  	cout<<"</head>"<<endl;
    cout<<"<body>"<<endl;
	cout<<"<table class=\"table table-dark table-bordered\">"<<endl;
	cout<<"<thead>"<<endl;
	cout<<"<tr>"<<endl;
	for(int i=0; i<5; i++){
		if(hostInfo[i].used == 1){
			cout<<"<th scope=\"col\">";
			cout<<hostInfo[i].host<<":"<<hostInfo[i].port ;
			cout<<"</th>"<<endl;
		}
	}
	cout<<"</tr>"<<endl;
	cout<<"</thead>"<<endl;
	cout<<"<tbody>"<<endl;
	cout<<"<tr>"<<endl;
	for(int i=0; i<5; i++){
		if(hostInfo[i].used == 1){
			cout<<"<td><pre id=\"s";
			cout<<to_string(i);
			cout<<"\" class=\"mb-0\"></pre></td>"<<endl;
		}
	}
	cout<<"</tr>"<<endl;
	cout<<"</tbody>"<<endl;
	cout<<"</table>"<<endl;
	cout<<"</body>"<<endl;
	cout<<"</html>"<<endl;
}

void parse_qString(){
    string qString = getenv("QUERY_STRING");
    vector<string> c1, c2;
	boost::split(c1, qString, boost::is_any_of("&"), boost::token_compress_on);

	for(int i=0 ; i<5 ; i++){
		for(int j=0 ; j<3 ; j++){
            boost::split(c2, c1[(i*3)+j], boost::is_any_of("="), boost::token_compress_on);
            if(c2[1] == ""){
		        hostInfo[i].used = 0;
		        break;
	        }
	        else{
		        hostInfo[i].used = 1;
            }

			if(j == 0)
				hostInfo[i].host = c2[1];
			else if(j == 1)
				hostInfo[i].port = c2[1];
			else if(j == 2)
				hostInfo[i].file = c2[1];
		}		
	}
    //get the info
    vector<string> c3, c4;
    boost::split(c3, c1[15], boost::is_any_of("="),boost::token_compress_on);
	vector<string> socksHostValue = c3;
	socksHost = socksHostValue[1];
    boost::split(c4, c1[16], boost::is_any_of("="),boost::token_compress_on);
	vector<string> socksPortValue = c4;
	socksPort = socksPortValue[1];
}

int main(int argc, char* argv[]){
    try{
        parse_qString();
        print_HTML();     
        for (int i=0; i<5; i++){
            if (hostInfo[i].used == 1)
                make_shared<client>(to_string(i))->start();         
        }
        io_Context.run();
    } 
    catch (exception& e){
        cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}