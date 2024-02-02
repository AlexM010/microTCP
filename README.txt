To compile:
	mkdir build
	cd build
	cmake ..
	make

To run:
	Server:
		./build/test/bandwidth_test -m -f <file> -p <port> -s
	Client:
		./build/test/bandwidth_test -m -f <file> -p <server_port> -a <server_ip>


In the folder a report is included:	"Report.pdf"