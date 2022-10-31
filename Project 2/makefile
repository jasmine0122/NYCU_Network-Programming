all: 
	g++ np_simple.cpp -o np_simple
	g++ -std=c++11 np_single_proc.cpp -o np_single_proc
	g++ -std=c++11 -o np_multi_proc np_multi_proc.cpp np_shell.cpp

clean:
	@rm np_simple
	@rm np_single_proc
	@rm np_multi_proc
