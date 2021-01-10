/**
 * References:
 * [1] https://blog.csdn.net/lyztyycode/article/details/80865617
 */

#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include "common.h"
using namespace std;


/**
 * Simple P2P server
 * 
 * @usage:
 *   sp_server [port]
 */
int main(int argc, char *argv[])
{
    int port_number = 12345;
    switch (argc) {
    case 1:
        break;
    case 2:
        try { port_number = stoi(argv[1]); }
        catch (invalid_argument e) {
            cerr << e.what() << endl;
            exit(-2);
        }
        break;
    default:
        cerr << "Invalid argument number" << endl;
        exit(-1);
    }

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd < 0) {
        perror("socket");
        exit(-3);
    }
 
    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
    server_addr.sin_port = htons(port_number);
    server_addr.sin_family = AF_INET; 
    if(bind(sockfd, (sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(-4);
    }
    cout << "Listen on " << addr2str(server_addr) << "\n" << endl;

    // Client information
    request req;
    sockaddr_in client_addr;
    response info[2];
    while(true) {
        memset(info, 0, sizeof(info));

        // Client 1
        do {
            socklen_t len = sizeof(sockaddr_in);
            recvfrom(sockfd, &req, sizeof(req), 0, (sockaddr *)&client_addr, &len);
        } while (req.verif_code != A_RANDOM_NUMBER);
        info[0].peer_addr = client_addr;
        info[0].peer_NAT = req.local_NAT;
        cout << "Client " << addr2str(client_addr) << " registered" << endl;

        // Client 2
        do {
            socklen_t len = sizeof(sockaddr_in);
            recvfrom(sockfd, &req, sizeof(req), 0, (sockaddr *)&client_addr, &len);
        } while (req.verif_code != A_RANDOM_NUMBER);
        info[1].peer_addr = client_addr;
        info[1].peer_NAT = req.local_NAT;
        cout << "Client " << addr2str(client_addr) << " registered" << endl;

        // Response to two clients
        sendto(sockfd, &info[1], sizeof(response), 0, (sockaddr *)&info[0].peer_addr, sizeof(sockaddr_in));
        sendto(sockfd, &info[0], sizeof(response), 0, (sockaddr *)&info[1].peer_addr, sizeof(sockaddr_in));
        cout << "Respond to 2 clients done" << endl;
        cout << endl;
    }

    return 0;
}
