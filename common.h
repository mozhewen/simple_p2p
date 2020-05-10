#include <arpa/inet.h> // sockaddr_in, inet_ntoa
#include <string> // class string
using namespace std;


#define CLIENT_NAME_SIZE 32
#define A_RANDOM_NUMBER 10506909318590836117ULL

enum NAT_type {
    NAT_restricted = 0,
    NAT_port_restricted = 1,
    NAT_symmetric = 2
};

struct request {
    unsigned long long verif_code;
    //char local_name[CLIENT_NAME_SIZE];
    NAT_type local_NAT;
    //char peer_name[CLIENT_NAME_SIZE];
};

struct response {
    sockaddr_in peer_addr;
    NAT_type peer_NAT;
};

enum msg_type {
    msg_open = 0,
    msg_keep_alive = 1,
    msg_data = 2,
};

struct packet {
    msg_type header;
    unsigned char buffer[65536];
};

struct packet_short {
    msg_type header;
    char text[128];
};

inline bool operator!=(const sockaddr_in &a, const sockaddr_in &b) {
    return a.sin_addr.s_addr    != b.sin_addr.s_addr
        || a.sin_port           != b.sin_port
        || a.sin_family         != b.sin_family;
}

inline string addr2str(const sockaddr_in &addr) {
    return string(inet_ntoa(addr.sin_addr)) + ":" + to_string(ntohs(addr.sin_port));
}
