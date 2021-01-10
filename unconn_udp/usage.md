 # uncoon_udp Usage

## NAME

unconn_udp - unconnected UDP

## SYNOPSIS

```
unconn_udp [PORT]
```

Listen on the port number PORT. If PORT is left unspecific, a random port will be chosen. 


## DESCRIPTION

This command is designed with minimal functions only for detecting the NAT type of your local area network. 

## SYNTAX

unconn_udp will read the standard input line by line. Each line must have one of the following formats: 

1. Send a message to a designated IP address
   
    ```
    IP:ports message
    ```

    "ports" can be a number (e.g. 12345) or a range of numbers (e.g. 12345-54321) which means a port scan. If the remote side is listening on one of the "ports" using unconn_udp, the message will be shown on his standard output in the form: 

    ```
    (From source_IP:source_port) message
    ```

2. Name an IP address

    In most cases, you may want to send multiple messages to a particular IP. It's a handy way to define a name for that address. This can be done by

    ```
    :name IP_address:ports
    ```

    When a name is defined, unconn_udp will automatically recognize the corresponding address and replace it with this name. 

3.  Send a message using a name

    ```
    @name message
    ```

    @name will be replaced with the corresponding address. 

4. Exit
   
    Simply type 

    ```
    exit
    ```
