# simple_p2p

A simple implementation (less than 500 lines) of UDP hole punching for P2P communication

## Description


## unconn_udp

A tool used for detecting NAT types of clients. It's much like "`nc -u`" command in Linux. However, it creates a socket which does not restrict peer address so that you can send messages to different hosts and ports within a single socket. 

For detailed usage, see [here](./unconn_udp/usage.md).

## tun_test

This is a simple demonstration of how to create a TUN device and handle I/O in C. It accepts ICMP packets from the TUN, swaps the src and dest address, changes the packet's type and sends it back to TUN. Use `ping -I tunX address` to test its effectiveness. 

## References

[1] http://blog.chinaunix.net/uid-317451-id-92474.html

[2] https://blog.csdn.net/lyztyycode/article/details/80865617

[3] https://blog.csdn.net/li_wen01/article/details/52665505