# home 端借助 office 端代理上网

## 配置方法

1. 需要一个有公网 IP 的 server （我的是百度云）

   在 server 上运行

   ```
   $ ./sp_server 12345
   ```

   程序会自动监听 12345 端口

2. 在 home 端和 office 端分别运行 sp_client，语法为

   ```
   $ sudo ./sp_client [-t tun_name] [-n 0|1|2] serverIP:serverPort(填12345端口)
   ```

   sp_client 会将自己的信息上传 server，当 server get 到两条客户端信息时，会向两个 client 返回对端的信息，然后 sp_client 就会开始互连了（自动完成了打洞操作）

   -t 自定义 TUN 设备名称 如果省略，系统会自动从 tun0 开始编号

   -n NAT类型 0=全锥型，1=端口限制锥型，2=对称型

3. 在 office 端

   * 开启 IP 转发功能（使这台电脑能作为路由器中转 IP 数据报，否则的话系统收到收件地址不是自己的包时会直接把它扔掉而不是转发出去）

      ```
      # echo 1 > /proc/sys/net/ipv4/ip_forward
      ```

   * 启动刚才新建的 TUN 网卡（假设它叫 tun0），绑定 IP 地址

      ```
      # ip link set tun0 up
      # ip addr add 10.0.0.1 peer 10.0.0.2 dev tun0
      ```

   * 开启 iptables/nft 的地址伪装功能
      最初从 home 发出的 IP 包的寄件地址是 10.0.0.2，这个地址被 office 转发的时候会被 iptables/nft 伪装成 office 的内网地址，而回复的包的收件地址是 office 的内网地址，iptables/nft 会帮你再转回 10.0.0.2，然后再发回 home。如果不这样做，以 10.0.0.1、10.0.0.2 的地址在 office 的内网中游走，没人知道你是谁（因为这两个地址是你随便起的）

      ```
      # iptables -t nat -A POSTROUTING -o (这里填你办公室上网网卡的名称：eth0,eno1之类的) -j MASQUERADE
      ```

      **或者**

      ```
      # nft add rule nat postrouting masquerade
      ```

      **注意：** 这取决于你的系统支持哪个工具，nft 这个工具更新一些

4. 在 home 端
   
   * 新建网络命名空间“p2p”，把 TUN 设备扔到这里面去，方便你在普通上网与代理上网切换（也可以不建，但不推荐）

      ```
      # ip netns add p2p
      # ip link set tun0 netns p2p
      ```

   * （可选）为新建的 namespace“p2p”打开本地回环（loopback, lo），不这么做不影响上网，但是会 ping 不通 localhost
  
      ```
      # ip -n p2p link set lo up
      ```

   * 启用 TUN 设备，和 office 端的操作相同，但地址反过来写

      ```
      # ip -n p2p link set tun0 up
      # ip -n p2p addr add 10.0.0.2 peer 10.0.0.1 dev tun0
      ```

   * 在这个新的网络空间”p2p“中添加默认路由，使其流经 tun0

      ```
      # ip -n p2p route add default via 10.0.0.2
      ```

      **注意：** 这里填 via 10.0.0.1 也行的，从结果来说效果是相同的，我不清楚他们本质上有何区别

5. 一切就绪，最后一步是修改 DNS 服务器的地址，不然的话运行在”p2p”网络空间的程序还是会使用你本机（home 端）的 DNS 服务器地址，这可能会导致域名解析不了。

   需要改 `/etc/resolv.conf` 文件，但你又不想把文件覆盖掉，因为你还是希望能够在普通上网和代理上网之间自由切换，所以就需要另一种命名空间：mount namespace
  
   复制一份 `resolv.conf` 文件，放在 `...path/` 目录下（改成你具体的目录），将文件里面的 nameserver 改成你想要的服务器地址

   最后是进入”p2p“网络空间，并且新建一个 mount namespace，把你修改过的 `resolv.conf` 文件挂载到 `/etc/resolv.conf` 上：

   ```
   $ sudo -E nsenter --net=/var/run/netns/p2p unshare --mount sh -c 'mount --bind ...path/resolv.conf /etc/resolv.conf; sudo -u mozhewen(改成你的用户名) -E bash'
   ```

   在这个新打开的 bash 里，就可以打开各种应用程序了，图形界面也是支持的。

   要想在这个 bash 里打开 chrome 浏览器，需要告诉 chrome 新建一个 session （否则 chrome 会在当前 session 下新开页面，这用的还是系统默认的 IP 地址，不是新建的网络空间的 IP 地址），这需要加一个选项：

   ```
   $ google-chrome --user-data-dir=/tmp
   ```

6. IPv6 配置方法

   * 在office 端，开启 IPv6 转发

      ```
      # echo 1 > /proc/sys/net/ipv6/conf/all/forwarding
      ```

   * 给 TUN 设备添加 IPv6 地址，如：（随便起的）
  
      office 端：

      ```
      # ip addr add 2001::1 peer 2001::2 dev tun0
      ```

      home 端则把上面地址反过来

   * 在 office 端，要使用 iptables -> ip6tables 来做地址伪装：

      ```
      # ip6tables -t nat -A POSTROUTING -o (这里填你办公室上网网卡的名称：eth0,eno1之类的) -j MASQUERADE
      ```

      如果是 nft 的话，应该刚才那条命令已经包含了 IPv6 ？沒试过不知道。

   * 在 home 端，设置 IPv6 的默认路由

      ```
      # ip -6 -n p2p route add default via 2001::2
      ```

完成。

## ssh VPN

以上是两客户端均无公网 IP 时的 p2p 连接方法。其实只要其中有一端有公网 IP，就不用这么麻烦，直接使用 `ssh -D/-R` 动态端口转发即可，例如：

   * office 端有公网 IP，则在 home 端输入

      ```
      $ ssh -D 1080 user@officeIP
      ```

   然后 home 端浏览器 Preferences/Settings 中设置 socks5 代理，Host 填 localhost，Port 填 1080，即在浏览器中使用 office 代理上网

   * home 端有公网 IP，则在 office 端输入

      ```
      $ ssh -R 1080 user@homeIP
      ```

   然后 home 端浏览器和上面一样设置 Host & Port 即可

   **注意：** 像 Chrome 浏览器是引用系统层面的代理设置的，所以使用 Chrome 的话需在系统设置中改

动态端口转发方法只适用于支持 socks5 协议的应用层的数据传输，不是所有的软件都支持的。如果想要实现更底层的连接（虚拟网卡，直接转发 IP 数据报），可使用 `ssh -w` 命令，这个命令的原理和我的程序一样，都是新建 TUN 设备，因此关于 TUN 设备的配置步骤都是相通的（上述第 3 - 6 步）。

用法：

   ```
   # ssh -w0:0 user@peerIP
   ```

上述命令会自动在 ssh 连接的两端各新建一个 tun0，具体细节就不讲了，甩一个链接 https://help.ubuntu.com/community/SSH_VPN
