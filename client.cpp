/**
 * References:
 * [1] http://blog.chinaunix.net/uid-317451-id-92474.html
 * [2] https://blog.csdn.net/lyztyycode/article/details/80865617
 * [3] https://blog.csdn.net/li_wen01/article/details/52665505
 */

#include <linux/if.h> // ifreq
#include <linux/if_tun.h> // IFF_TUN, IFF_NO_PI
#include <fcntl.h> // open, O_RDWR
#include <unistd.h> // close, read, write
#include <sys/ioctl.h> // ioctl
#include <arpa/inet.h> // sockaddr_in
#include <sys/socket.h> // socket
#include <pthread.h> // pthread_create, pthread_mutex_lock, pthread_mutex_unlock, ...
#include <cstring> // memset, strncmp, strncpy, memcpy, strtok
#include <iostream>
#include "common.h"
using namespace std;


const timeval recv_timeout = {20, 0}; // Socket receive timeout
const unsigned int keep_alive_interval = 10;
const unsigned int wait_sec = 5; // Wait peer open the door

/**
 * Create a TUN device
 * 
 * @param {char *} dev_name Device name. If left empty, a system-specified value
 *   is used. 
 */
int tun_create(char *dev_name) {
    ifreq ifr;
    int fd, err;

    // 1. Open TUN character device (as a file)
    if ((fd = open("/dev/net/tun", O_RDWR)) < 0)
        return fd; // as error number
    // 2. Config TUN device using configuration in "ifr"
    memset(&ifr, 0, sizeof(ifr));
    // IFF_TUN: TUN device for IP datagram (Alt: TAP for Ethernet datagram). 
    // IFF_NO_PI: No packet Information. Simply a raw datagram, no extra header. 
    ifr.ifr_flags |= IFF_TUN | IFF_NO_PI;
    // If dev_name is not empty, use this name
    if (strncmp(dev_name, "", IFNAMSIZ) != 0)
        strncpy(ifr.ifr_name, dev_name, IFNAMSIZ);
    // Configure TUN device
    if((err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0){
        close(fd);
        return err;
    }
    // Copy the final divice name back
    strncpy(dev_name, ifr.ifr_name, IFNAMSIZ);

    return fd;
}

/**
 * Request peer address (blocked)
 */
response server_request(int sockfd, const sockaddr_in &server_addr, const request &req) {
    response res;
    sockaddr_in pac_addr;
    socklen_t len = sizeof(pac_addr);
    sendto(sockfd, &req, sizeof(req), 0, (sockaddr *)&server_addr, sizeof(server_addr));
    memset(&pac_addr, 0, sizeof(pac_addr));
    do {
        recvfrom(sockfd, &res, sizeof(res), 0, (sockaddr *)&pac_addr, &len);
    } while (pac_addr != server_addr);
    return res;
}

sockaddr_in peer_addr;

/**
 * Send a packet to peer to make local NAT record peer's information (Let the
 * local NAT allow peer's package back). I call this process "open the door". 
 * There are 4 cases for both ends' NAT types: 
 *   1. symmetric - symmetric           Peer connection is unlikely
 *   2. symmetric - port restricted     Port scan is needed
 *   3. symmetric - restricted          
 *   4. no symmetric                    Simply send an open-the-door packet in each end
 * NOTE: If one end is symmetric, the other end will receive a "You made it"
 * packet. In other cases, no such message is sent or received. 
 */
int open_the_door(int sockfd, NAT_type local_NAT, NAT_type peer_NAT) {
    if (local_NAT == NAT_symmetric && peer_NAT == NAT_symmetric) {
        cerr << "NAT types of both ends are symmetric. Give up" << endl;
        return -1;
    }
    // 1.1. Send "Open the door" message
    packet_short pack = {msg_open, "Open the door"};
    if (local_NAT == NAT_port_restricted && peer_NAT == NAT_symmetric) {
        cout << "Local NAT type is port restricted, peer NAT type is symmetric\n"
             << "Wait " << wait_sec << " second(s) for peer opening the door... " << endl;
        sleep(wait_sec);
        cout << "Start port scan... " << endl;
        // Port scan
        for (int i = 1024; i < 65536; i++) {
            peer_addr.sin_port = htons(i);
            sendto(sockfd, &pack, sizeof(pack), 0, (sockaddr *)&peer_addr, sizeof(peer_addr));
        }
    } else if (local_NAT == NAT_symmetric && peer_NAT == NAT_restricted) {
        cout << "Local NAT type is symmetric, peer NAT type is restricted\n"
             << "Wait " << wait_sec << " second(s) for peer opening the door... " << endl;
        sleep(wait_sec);
    } else {
        cout << "Open the door to peer " << addr2str(peer_addr) << endl;
        sendto(sockfd, &pack, sizeof(pack), 0, (sockaddr *)&peer_addr, sizeof(peer_addr));
    }
    // 1.2. Receive port scan message
    if (local_NAT == NAT_symmetric && peer_NAT == NAT_port_restricted) {
        cout << "Local NAT type is symmetric, peer NAT type is port restricted\n"
             << "Wait for peer's port scan arrival... " << endl;
        socklen_t len = sizeof(peer_addr);
        int ret = recvfrom(sockfd, &pack, sizeof(pack), 0, (sockaddr *)&peer_addr, &len);
        if (ret < 0) {
            cerr << "Timed out" << endl;
            return -2;
        } else cout << "Peer arrived" << endl;
    }
    // 2.1. Send "You made it" message
    if (local_NAT == NAT_symmetric) {
        // Used for peer detecting local's port number
        strncpy(pack.text, "If you see this message, you made it. ", sizeof(pack.text));
        sendto(sockfd, &pack, sizeof(pack), 0, (sockaddr *)&peer_addr, sizeof(peer_addr));
        cout << "\"You made it\" packet sent" << endl;
    }
    // 2.2. Wait for peer's response to detect the actual peer port number
    if (peer_NAT == NAT_symmetric) {
        // NOTE: This will change the "peer_addr"'s port to an available one. 
        // It's not safe without checking the sender of the packet. However,
        // it's unlikely for others to get access in a symmetric NAT from outside. 
        cout << "Wait for peer's response... " << endl;
        socklen_t len = sizeof(peer_addr);
        int ret = recvfrom(sockfd, &pack, sizeof(pack), 0, (sockaddr *)&peer_addr, &len);
        if (ret < 0) {
            cerr << "Timed out" << endl;
            return -2;
        } else {
            cout << "Response from peer: " << pack.text << "\n"
                 << "Peer address changed to " << addr2str(peer_addr) << endl;
        }
    }
    return 0;
}

// Thread IDs
pthread_t recv_id, send_id, keep_alive_id;
// Mutex lock
pthread_mutex_t send_mtx = PTHREAD_MUTEX_INITIALIZER;

packet recv_packet, send_packet;

void *recv_thread(void *args) {
    int tunfd = ((int *)args)[0], sockfd = ((int *)args)[1];
    socklen_t len = sizeof(peer_addr);
    while (true) {
        ssize_t pack_len = recvfrom(sockfd, &recv_packet, sizeof(recv_packet), 
            0, (sockaddr *)&peer_addr, &len);
        if (pack_len < 0) {
            cerr << "Timed out" << endl;
        } else if (recv_packet.header == msg_keep_alive) {
            // cout << "keep alive ^_^" << endl; // For debug
        } else if (recv_packet.header == msg_data) {
            write(tunfd, recv_packet.buffer, pack_len - sizeof(recv_packet.header));
        }
    }
}

void *send_thread(void *args) {
    int tunfd = ((int *)args)[0], sockfd = ((int *)args)[1];
    while (true) {
        ssize_t buff_len = read(tunfd, &send_packet.buffer, sizeof(send_packet.buffer));
        send_packet.header = msg_data;
        pthread_mutex_lock(&send_mtx);
        sendto(sockfd, &send_packet, buff_len + sizeof(send_packet.header), 0,
            (sockaddr *)&peer_addr, sizeof(peer_addr));
        pthread_mutex_unlock(&send_mtx);
    }
}

void *keep_alive_thread(void *args) {
    int sockfd = *(int *)args;
    packet_short kl = {msg_keep_alive, "Keep alive"};
    while (true) {
        pthread_mutex_lock(&send_mtx);
        sendto(sockfd, &kl, sizeof(kl), 0, (sockaddr *)&peer_addr, sizeof(peer_addr));
        pthread_mutex_unlock(&send_mtx);
        sleep(keep_alive_interval);
    }
}


/**
 * Simple P2P implementation
 * 
 * Server is used for dececting peers' public IP's & ports and not for data
 * transmission. 
 * Data transmission is done directly from peer to peer (hence the name P2P). 
 * 
 * @usage: 
 *   sp_client [-t tun_name] [-n NAT_type] serverIP:serverPort
 * 
 *   NAT_type
 *     0 = Restricted NAT
 *     1 = Port restricted NAT
 *     2 = Symmetric NAT
 * 
 *   Example: 
 *     sp_client -n2 1.2.3.4:2333
 * 
 *   NOTE: 
 *     1. Domain name resolution is not supported here for the simplicity of
 *   the program. 
 *     2. Full-cone NAT is classified into restricted NAT in this program since
 *   they can be handled in the same way (For the full-cone case you actually
 *   don't need to open the door)
 */
int main(int argc, char *argv[]) {
    char dev_name[IFNAMSIZ] = "";
    NAT_type local_NAT = NAT_restricted, peer_NAT;

    sockaddr_in server_addr;
    int tunfd, sockfd;

    // 0. Parse arguments
    try {
        int opt;
        while ((opt = getopt(argc, argv, "+t:n:")) >= 0) {
            switch(opt) {
            case 't':
                // TUN name
                strncpy(dev_name, optarg, IFNAMSIZ);
                break;
            case 'n':
                // NAT type
                local_NAT = (NAT_type)stoi(optarg);
                break;
            }
        }
        if (argc - optind != 1) {
            cerr << "Invalid argument number" << endl;
            exit(-1);
        }
        // serverIP:serverPort
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_addr.s_addr = inet_addr(strtok(argv[optind], ":"));
        server_addr.sin_port = htons(stoi(strtok(NULL, ":")));
        server_addr.sin_family = AF_INET; // IPv4
    } catch (invalid_argument e) {
        cerr << e.what() << endl;
        exit(-2);
    }

    // 1. Create a TUN device
    if ((tunfd = tun_create(dev_name)) < 0) {
        perror("tun_create");
        exit(-3);
    }
    cout << "TUN device \"" << dev_name << "\" created\n";

    // 2. Create a socket for server and peer connections
    // UDP is used for simplicity. (It's much simpler than TCP)
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(-4);
    }
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));

    // 3. Server & peer connection
    request req = {A_RANDOM_NUMBER, local_NAT};
    response res = server_request(sockfd, server_addr, req);
    peer_addr = res.peer_addr;
    peer_NAT = res.peer_NAT;
    cout << "Response from server: " 
         << "Peer address " << addr2str(peer_addr) << ", "
         << "peer NAT type " << peer_NAT << endl;
    if (open_the_door(sockfd, local_NAT, peer_NAT) < 0) {
        perror("open_the_door");
        exit(-5);
    }
    int fds[2] = {tunfd, sockfd};
    pthread_create(&recv_id, NULL, recv_thread, fds);
    pthread_create(&send_id, NULL, send_thread, fds);
    pthread_create(&keep_alive_id, NULL, keep_alive_thread, &sockfd);
    pthread_join(recv_id, NULL);

    return 0;
}
