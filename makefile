all: clean compile

compile:
	g++ -pthread dataServer.cpp -o dataServer
	g++ -pthread remoteClient.cpp -o remoteClient

clean:
	rm -f dataServer remoteClient

move:
	mv *.txt ..
