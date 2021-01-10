main:

client:
	g++ -O2 client.cpp -lpthread -o sp_client

server:
	g++ -O2 server.cpp -o sp_server
