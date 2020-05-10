/**
 * References:
 * [1] http://blog.chinaunix.net/uid-317451-id-92474.html
 */

#include <string.h> // memset, strncmp, strncpy, memcpy
#include <linux/if.h> // ifreq
#include <linux/if_tun.h> // IFF_TUN, IFF_NO_PI
#include <fcntl.h> // open, O_RDWR
#include <unistd.h> // close, read
#include <sys/ioctl.h> // ioctl
#include <stdio.h> // printf, fprintf, perror


/**
 * Create TUN device
 * 
 * @param {char *} dev_name Device name. If left empty, a system-specified value is used. 
 */
int tun_create(char *dev_name) {
    struct ifreq ifr;
    int fd, err;

    // 1. Open TUN character device (as a file)
    if ((fd = open("/dev/net/tun", O_RDWR)) < 0)
        return fd; // as error number
    // 2. Config TUN device using configuration in "ifr"
    memset(&ifr, 0, sizeof(ifr));
    //   IFF_TUN: TUN device for IP datagram (Alt: TAP for Ethernet datagram). 
    //   IFF_NO_PI: No Package Information. Simply a raw datagram, no extra header. 
    ifr.ifr_flags |= IFF_TUN | IFF_NO_PI;
    //   If dev_name is not empty, use this name
    if (strncmp(dev_name, "", IFNAMSIZ) != 0)
        strncpy(ifr.ifr_name, dev_name, IFNAMSIZ);
    //   Configure TUN device
    if((err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0){
        close(fd);
        return err;
    }
    //   Copy the final divice name back
    strncpy(dev_name, ifr.ifr_name, IFNAMSIZ);

    return fd;
}

typedef unsigned char byte;

byte buffer[65535];


int main(int argc, char *argv[]){
    char dev_name[IFNAMSIZ];
    int tun, ret;
    // 1. Create TUN device
    switch(argc) {
    case 1:
        dev_name[0] = '\0';
        break;
    case 2:
        strncpy(dev_name, argv[1], IFNAMSIZ);
        break;
    default:
        fprintf(stderr, "Too many arguments. ");
        return 1;
    }
    if ((tun = tun_create(dev_name)) < 0) {
        perror("tun_create");
        return 2;
    }
    printf("TUN device \"%s\" created. \n", dev_name);

    // 2. Start package forwarding
    while(1) {
        byte ip[4];

        ret = read(tun, buffer, sizeof(buffer));
        if (ret < 0) break;
        printf("Read %d bytes\n", ret);
        memcpy(ip, &buffer[12], 4);
        memcpy(&buffer[12], &buffer[16], 4);
        memcpy(&buffer[16], ip, 4);
        buffer[20] = 0;
        *((unsigned short *)&buffer[22]) += 8;
        ret = write(tun, buffer, ret);
        printf("Write %d bytes\n", ret);
    }

    return 0;
}
