#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <iostream>
#include <string>
#include <cstring>
#include <regex>
#include "addr_name.h"
using namespace std;


inline bool check_port(int port) {
    return port > 0 && port < 65536;
}
char invalid_port[] = "Port number should be 1-65535";

// Thread IDs
pthread_t recv_id, send_id;

// Known list of IP addresses
addr_name known_list;

char recv_buffer[1024];

void *recv_thread(void *args) {
    int sockfd = *(int *)args;
    char addr_str[AN_ADDR_STR_SIZE], *addr_str2;
    sockaddr_in peer_addr;
    // NOTE: If the initial value of "len" is not set, recvfrom() will not get
    // correct address. 
    socklen_t len = sizeof(sockaddr_in);

    while(true) {
        // 1. Receive data
        // NOTE: '\0' is not transmitted. 
        ssize_t buff_len = recvfrom(sockfd, recv_buffer, sizeof(recv_buffer)-1, 
            0, (sockaddr *)&peer_addr, &len);
        recv_buffer[buff_len] = '\0';
        // 2. Generate IP address
        snprintf(addr_str, AN_ADDR_STR_SIZE, "%s:%d",
            inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port)
        );
        // 3. Convert to name if in the known_list
        if (!(addr_str2 = known_list.get_name(addr_str)))
            addr_str2 = addr_str;
        // 4. Output
        cout << "\e[33m(From " << addr_str2 <<") " << recv_buffer << "\e[0m" << endl;        
    }
}

char send_buffer[1024];

void *send_thread(void *args) {
    int sockfd = *(int *)args;
    sockaddr_in peer_addr;
    socklen_t len = sizeof(sockaddr_in);

    regex re_line("(\\S+)\\s+(\\S.*)");
    regex re_set_name(":(\\w+)");
    regex re_at_name("@(\\w+)");
    regex re_addr("(\\d+\\.\\d+\\.\\d+\\.\\d+):([\\d-,]+)");
    regex re_port_range("(\\d+)-(\\d+)");
    cmatch cresult;
    smatch sresult;
    string command;
    const char *content;
    int content_len;

    cin.exceptions(ios_base::failbit | ios_base::badbit);
    while(true) {
        // 1. Get a line
        try {
            if (cin.getline(send_buffer, 1024).eof())
                break;
        } catch (istream::failure e){
            cin.clear(); // Clear failure state
        }
        if (strcmp(send_buffer, "exit") == 0) break;
        // 2. Divide into command part and content part
        if (regex_match(send_buffer, cresult, re_line)) {
            command = cresult[1].str();
            content = cresult[2].first;
            // "content" should end with '\0' since it's inside send_buffer. 
            content_len = cresult[2].length();
        } else {
            cerr << "Invalid line format" << endl;
            continue;
        }
        // 3. Handle different commands
        // Case 1: :name address
        if (regex_match(command, sresult, re_set_name)) {
            try {
                // NOTE: DO NOT use ".str().c_str()". The string created by
                // str() will be deconstructed immediately. Although the pointer
                // is still available for meaningful content, it's not safe to
                // do that. 
                string temp = sresult[1].str();
                known_list.add_item(
                    temp.c_str(),
                    content
                );
            } catch (addr_name::overflow e) {
                cerr << e.what() << endl;
                continue;
            }
        } else {
            // Case 2: @name content
            if (regex_match(command, sresult, re_at_name)) {
                string name = sresult[1].str();
                char *addr_str = known_list.get_addr(name.c_str());
                if (addr_str) command = addr_str;
                else {
                    cerr << "Name \"" << name << "\" not found" << endl;
                    continue;
                }
            }
            // Case 3: IP:ports content
            if (regex_match(command, sresult, re_addr)) {
                // IP part
                string ip_str = sresult[1].str();
                in_addr_t ip = inet_addr(ip_str.c_str());
                if (ip != INADDR_NONE)
                    peer_addr.sin_addr.s_addr = ip;
                else {
                    cerr << "Bad address" << endl;
                    continue;
                }
                // Ports part
                // NOTE: No more than AN_ADDR_STR_SIZE characters. 
                char ports_str[AN_ADDR_STR_SIZE];
                int n = sresult[2].str().copy(ports_str, 128);
                ports_str[n] = '\0';
                // Start port scan
                char *port_range = strtok(ports_str, ",");
                bool flag_continue = false;
                while (port_range) {
                    try {
                        int port_s, port_e;
                        // Case 3.1: IP:port_s-port_e content
                        if (regex_match(port_range, cresult, re_port_range)) {
                            port_s = stoi(cresult[1].str());
                            port_e = stoi(cresult[2].str());
                        // Case 3.2: IP:port content
                        } else port_s = port_e = stoi(port_range);
                        if (!check_port(port_s) || !check_port(port_e))
                            throw invalid_argument(invalid_port);
                        int dir = port_s < port_e ? 1 : -1;
                        for (int port = port_s; (port - port_e)*dir <= 0; port+=dir) {
                            peer_addr.sin_port = htons(port);
                            peer_addr.sin_family = AF_INET;
                            // Send data
                            sendto(sockfd, content, content_len, 0, 
                                (struct sockaddr *)&peer_addr, 
                                sizeof(struct sockaddr_in));
                        }
                        if ((port_e - port_s) * dir >= 10000)
                            cout << "Port scan " << port_s << "-" << port_e 
                                 << " done" << endl;
                    } catch (invalid_argument e){
                        cerr << e.what() << endl;
                        flag_continue = true;
                        break;
                    }
                    port_range = strtok(nullptr, ",");
                }
                if (flag_continue) continue;
            // Case 4: Otherwise
            } else {
                cerr << "Invalid command format" << endl;
                continue;
            }
        }
    }
    return 0;
}


int main(int argc, char *argv[])
{
    int sockfd;
    int port = 0;
    // 1. Check arguments
    if (argc == 2) {
        try {
            port = stoi(argv[1]);
            if (!check_port(port)) throw invalid_argument(invalid_port);
        } catch (invalid_argument e) {
            cerr << e.what() << endl;
            exit(1);
        }
    } else if (argc > 2){
        cerr << "Too many arguments " << endl;
        exit(2);
    }
    // 2. Create a socket
    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(3);
    }
    // 3. bind to a port
    sockaddr_in local_addr;
    socklen_t len = sizeof(local_addr);
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
    local_addr.sin_port = htons(port);
    local_addr.sin_family = AF_INET;
    if (bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        perror("bind");
        exit(4);
    }
    getsockname(sockfd, (struct sockaddr *)&local_addr, &len);
    cout << "Start listen on "
         << inet_ntoa(local_addr.sin_addr) << ":" << ntohs(local_addr.sin_port)
         << "..." << endl;

    // 4. Open send/receive threads
    pthread_create(&recv_id, NULL, recv_thread, &sockfd);
    pthread_create(&send_id, NULL, send_thread, &sockfd);
    pthread_join(send_id, NULL);

    return 0;
}
