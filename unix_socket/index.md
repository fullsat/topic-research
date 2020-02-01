## 背景/動機

現在nginxとphp-fpmを利用していてsocket接続時にResource temporary unavailableというエラーが出力される。
unix domain socketを利用しているがこれがどのような通信なのか基礎的なことも分かっていない。
そのためエラーが発生していたとしても根本原因がどこにあるのかが理解できない状況にある。

なおこのエラーは受信バッファのサイズを大きくする、バックログ数を大きくする、といった対処をとる記事がよく見られる。
これは確かに緩やかなリソースの上昇により発生している場合にはパフォーマンスチューニングとして適切な値に思う。
しかし今回のエラーは平常時はリソースに余裕がある状態から、稀にエラーが発生するという状況である。
数百プロセスはほとんどidle状態で、何かしらに遅延が発生して一気にidle状態のプロセスがなくなる。
idle状態のプロセスがなくなったタイミングで空きプロセスが無いためunix domain socketに接続できずにエラーが発生しているという状況だ。
unix socket - php - tcp socket - DB この一連のどこかで障害が発生しているのは間違いなさそうなのだがそれがどこなのか分からない。

## 目的

unix domain socket通信時にエラーが発生する場合を調べる。
問題としてはnginxがphp-fpmのunix domain socketとconnectするときにエラーを出していることだ。
そのため、まずはOSとしてどのように通信を行っているかを調べ、nginxの通信方法を調査する。
通信の理解のためにphp-fpmの通信の流れも確認してみる。

## 疑問

* ソケットに種類があるのか、それはどのように使うのか
* 通信時にソケットの状態はどうなっているのか
* データのキャプチャはできるのか
* 通信時にエラーが発生する場合はどのような場合か

## ソケットの種類と使用方法

[UnixDomainSocketのマニュアル](https://linuxjm.osdn.jp/html/LDP_man-pages/man7/unix.7.html)

Unix Domain Socket自体は単なるプロセス間通信のための手段。
OSが提供している通信の手段は主に以下の通り。

```
プロセス間通信
- pipe
  - 無名パイプ: ローカルのプロセス間通信
  - 名前付きパイプ: ローカルのプロセス間通信
- socket
  - Unix Local: ローカルのプロセス間通信
  - IPv4: インターネット
  - IPv6: インターネット
  - netlink: カーネルモジュールとの通信
  - packet: デバイスドライバとの通信
  - ALG: カーネルの暗号APIへのインタフェース
  - IPX: ファイルサーバとの通信,もうあまり使っていなさそう
  - X25: WAN通信,もうあまり使っていなさそう
  - AX25: アマチュア無線通信
  - ATM PVC: LANやグローバルな通信,もうあまり使ってなさそう
  - AppleTalk: Apple製品で使われてた通信,もうあまり使ってなさそう
```

ソケット通信にも通信のやり取りとして、信頼性、順序性、データ転送方式などの違いがあり、以下のように分類されている。

```
通信の種類(ソケットごとに使えるものと使えないものがある)
- STREAM
  - TCPのような順番と信頼性がある通信
- DGRAM
  - UDPのようなコネクションレス、信頼性なし、固定最大長メッセージ
- SEQPACKET
  - STREAMと同じだがデータは固定最大長メッセージの通信
- RAW
  - ネットワークプロトコルに直接アクセスする/IPプロトコルに直接アクセスする
- RDM
  - 信頼性はあるが順序は保証しないデータグラム層(よく分かっていない)
```

ちなみにphp-fpmは当然ながらSOCK_STREAM([参照](https://github.com/php/php-src/blob/1ad08256f349fa513157437abc4feb245cce03fc/sapi/fpm/fpm/fpm_sockets.c#L190)

TCPもUnixドメインソケットも使えるようにfamily部分は抽象化されている。

これらの知識をもとにphp-fpmが実際に使っている通信をかなり抽象化して簡単に接続実験をする。[参照](/src/ex1/index.md)

ここでわかることはsocket() / bind() / listen() / accept() / read() で通信を行っていること。

実際にphp-fpmは以下のようにリクエストを受け付けている模様。

* socket() / bind() / listen()

https://github.com/php/php-src/blob/1ad08256f349fa513157437abc4feb245cce03fc/sapi/fpm/fpm/fpm_sockets.c#L190
https://github.com/php/php-src/blob/1ad08256f349fa513157437abc4feb245cce03fc/sapi/fpm/fpm/fpm_sockets.c#L211
https://github.com/php/php-src/blob/1ad08256f349fa513157437abc4feb245cce03fc/sapi/fpm/fpm/fpm_sockets.c#L231

fpm_main.c(main) -> fpm.c(fpm_init) -> fpm_sockets.c(fpm_sockets_init_main -> fpm_socket_af_unix_listening_socket -> fpm_sockets_get_listening_socket -> fpm_sockets_new_listening_socket)

* fork() / dup()

https://github.com/php/php-src/blob/f826bbde93c55d08de9d962946d00dd0cf5d98a1/sapi/fpm/fpm/fpm_children.c#L412

fpm_main.ci(main) -> fpm.c(fpm_run) -> fpm_children.c(fpm_children_create_initial -> fpm_children_make(fork) -> fpm_child_init(dup))

親プロセスでlistenしたソケットをforkしたプロセスでdupして複製しておく

* accept() / poll()

https://github.com/php/php-src/blob/5d6e923d46a89fe9cd8fb6c3a6da675aa67197b4/main/fastcgi.c#L1407

fpm_main.c(main) -> fastcgi.c(fcgi_accept_request)

acceptブロックし、受けたらリクエストを処理。

* read()

https://github.com/php/php-src/blob/5d6e923d46a89fe9cd8fb6c3a6da675aa67197b4/main/fastcgi.c#L981

fpm_main.c(main) -> fastcgi.c(fcgi_accept_request -> fcgi_read_request -> safe_read)

## 通信時にソケットの状態はどうなっているのか

以下はsleepしているタイミングで都度ssコマンドを実施し、その結果を示したもの。
つまり完全な状態遷移の状況を表すものではない。
しかし明らかな挙動を示唆してくれており、これを正常系とみなすと、異常な状態が分かる。

### 1. server側でsocket()を呼び出した時

ssでは表示されない。

### 2. server側でbind()を呼び出した時

ssで以下のように出力される。

```
u_str UNCONN 0 0 /tmp/server.sock 20680812 * 0 users:(("server",pid=22053,fd=3)) <->
```

### 3. server側でlisten()を呼び出した時

状態がListenになってSend-Qが5となっている。
5はlistenに渡したbacklogと思われる。

```
u_str LISTEN 0 5 /tmp/server.sock 20680812 * 0 users:(("server",pid=22053,fd=3)) <->
```

### 4. server側でaccept()を呼び出した時

3.から変化なし。

### 5. client側でconnect()を呼び出した時

connect出来たのでコネクション確立したソケットができている。

```
u_str LISTEN 0 5 /tmp/server.sock 20680812 * 0 users:(("server",pid=22053,fd=3)) <->
u_str ESTAB  0 0 /tmp/server.sock 20682789 * 20681945 users:(("server",pid=22053,fd=4)) <->
```

### 6. client側でwrite()を呼び出した時

writeが呼び出されたのでソケットのバッファに書き込まれている。

hello worldで、11文字、11Byteなので、Recv-Qも11という挙動。

```
u_str LISTEN 0  5 /tmp/server.sock 20680812 * 0 users:(("server",pid=22053,fd=3)) <->
u_str ESTAB  11 0 /tmp/server.sock 20682789 * 20681945 users:(("server",pid=22053,fd=4)) <->
```

### 7. server側でread()を呼び出した時

readすると受信バッファから11バイト読みだされるためRecv-Qの11が0となっている。

```
u_str LISTEN 0 5 /tmp/server.sock 20680812 * 0 users:(("server",pid=22053,fd=3)) <->
u_str ESTAB  0 0 /tmp/server.sock 20682789 * 20681945 users:(("server",pid=22053,fd=4)) <->
```

### 8. server側でclose()を呼び出した時

コネクションはクローズしたのでssでは表示されなくなった。

## データのキャプチャはできるのか

トラブル発生時にsocketに流れるパケットをキャプチャできたら便利である。

tcpであればtcpdumpやwiresharkなど様々なツールが存在する。

しかし、TCP/IP以外の通信のキャプチャはどのようにやればいいのか。

* strace

システムコールを介するのでsystemcallの呼び出し状況が見られるstraceを使う方法が紹介されていた。

`strace ./server` の実行結果が以下の通り。

```
execve("./server", ["./server"], 0x7ffe320467e0 /* 33 vars */) = 0
brk(NULL)                               = 0x2349000
access("/etc/ld.so.preload", R_OK)      = -1 ENOENT (No such file or directory)
openat(AT_FDCWD, "/etc/ld.so.cache", O_RDONLY|O_CLOEXEC) = 3
fstat(3, {st_mode=S_IFREG|0644, st_size=33822, ...}) = 0
mmap(NULL, 33822, PROT_READ, MAP_PRIVATE, 3, 0) = 0x7fd6cabfc000
close(3)                                = 0
openat(AT_FDCWD, "/lib64/libc.so.6", O_RDONLY|O_CLOEXEC) = 3
read(3, "\177ELF\2\1\1\3\0\0\0\0\0\0\0\0\3\0>\0\1\0\0\0@\21\2\0\0\0\0\0"..., 832) = 832
fstat(3, {st_mode=S_IFREG|0755, st_size=2021312, ...}) = 0
mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x7fd6cabfa000
mmap(NULL, 3844768, PROT_READ|PROT_EXEC, MAP_PRIVATE|MAP_DENYWRITE, 3, 0) = 0x7fd6ca637000
mprotect(0x7fd6ca7d8000, 2097152, PROT_NONE) = 0
mmap(0x7fd6ca9d8000, 24576, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE, 3, 0x1a1000) = 0x7fd6ca9d8000
mmap(0x7fd6ca9de000, 15008, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0) = 0x7fd6ca9de000
close(3)                                = 0
arch_prctl(ARCH_SET_FS, 0x7fd6cabfb4c0) = 0
mprotect(0x7fd6ca9d8000, 16384, PROT_READ) = 0
mprotect(0x600000, 4096, PROT_READ)     = 0
mprotect(0x7fd6cac05000, 4096, PROT_READ) = 0
munmap(0x7fd6cabfc000, 33822)           = 0
socket(AF_UNIX, SOCK_STREAM, 0)         = 3
unlink("/tmp/server.sock")              = 0
bind(3, {sa_family=AF_UNIX, sun_path="/tmp/server.sock"}, 110) = 0
listen(3, 5)                            = 0
accept(3, {sa_family=AF_UNIX}, [110->2]) = 4
read(4, "hello world", 256)             = 11
fstat(1, {st_mode=S_IFCHR|0620, st_rdev=makedev(136, 2), ...}) = 0
brk(NULL)                               = 0x2349000
brk(0x236a000)                          = 0x236a000
brk(NULL)                               = 0x236a000
write(1, "hello world\1\n", 13hello world
)         = 13
close(3)                                = 0
close(4)                                = 0
exit_group(0)                           = ?
+++ exited with 0 +++i
```

`strace ./client`の実行結果が以下の通り。

```
$ strace ./client
execve("./client", ["./client"], 0x7fff8c12d2e0 /* 33 vars */) = 0
brk(NULL)                               = 0x1b28000
access("/etc/ld.so.preload", R_OK)      = -1 ENOENT (No such file or directory)
openat(AT_FDCWD, "/etc/ld.so.cache", O_RDONLY|O_CLOEXEC) = 3
fstat(3, {st_mode=S_IFREG|0644, st_size=33822, ...}) = 0
mmap(NULL, 33822, PROT_READ, MAP_PRIVATE, 3, 0) = 0x7fad88b38000
close(3)                                = 0
openat(AT_FDCWD, "/lib64/libc.so.6", O_RDONLY|O_CLOEXEC) = 3
read(3, "\177ELF\2\1\1\3\0\0\0\0\0\0\0\0\3\0>\0\1\0\0\0@\21\2\0\0\0\0\0"..., 832) = 832
fstat(3, {st_mode=S_IFREG|0755, st_size=2021312, ...}) = 0
mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x7fad88b36000
mmap(NULL, 3844768, PROT_READ|PROT_EXEC, MAP_PRIVATE|MAP_DENYWRITE, 3, 0) = 0x7fad88573000
mprotect(0x7fad88714000, 2097152, PROT_NONE) = 0
mmap(0x7fad88914000, 24576, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE, 3, 0x1a1000) = 0x7fad88914000
mmap(0x7fad8891a000, 15008, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0) = 0x7fad8891a000
close(3)                                = 0
arch_prctl(ARCH_SET_FS, 0x7fad88b374c0) = 0
mprotect(0x7fad88914000, 16384, PROT_READ) = 0
mprotect(0x600000, 4096, PROT_READ)     = 0
mprotect(0x7fad88b41000, 4096, PROT_READ) = 0
munmap(0x7fad88b38000, 33822)           = 0
socket(AF_UNIX, SOCK_STREAM, 0)         = 3
connect(3, {sa_family=AF_UNIX, sun_path="/tmp/server.sock"}, 110) = 0
write(3, "hello world", 11)             = 11
close(3)                                = 0
exit_group(0)                           = ?
+++ exited with 0 +++
```

たしかにserver側でreadの第２引数に"hello world"が渡っていることから、
通信内容を確認できることが分かる。

この方法の欠点はsocket自身に対する操作ではないためreadやwriteのみを抜き出すにしても、
ファイルディスクリプタまで調べてから抜きださないといけないことだろう。
文字コードもデコードされないまま表示されているため都度変換も必要となる。

異なる方法ではsocatを利用する方法が散見される。
ただこちらは既存のsocketに対して閲覧する類ではない。

```
socat -v UNIX-LISTEN:/tmp/client.sock, UNIX-CONNECT:/tmp/server.sock
```

client.sock => socat => server.sock

といった、socatを流れるものを出力する方法である。

```
> 2020/xx/xx 8:20:30.617612  length=11 from=0 to=10
> hello world% 
```

このような出力となる。

おそらくstraceで確認したほうが既存構成を変えずに確認できる分、確認しやすいだろう。

現状では、socketファイルを指定するだけで、tcpdumpのように流れるデータを出力、加工して表示してくれるツールや方法は見つけられなかった。

## 通信時にエラーが発生する場合はどのような場合か

ここまでで、エラーが発生する場所として人間が網羅するには多すぎることはわかるだろうか。

アプリケーションから見た通信に必要な関数はだいたい決まっている。

client側の場合は以下の通り。

まずはconnect()でESTABLISHEDなコネクションを作成する。

プログラムから見ればファイルディスクリプタで、fopen等したあと返ってくるものと取り扱いは同じだ。

そのためreadとwriteに渡す値として使える。

つまりあとはwriteでリクエストを送り、readでレスポンスを受け取るという流れになる。

writeとreadと書いたが、nginxは実際にはsend/recvを利用している。

マニュアルによるとオプションの指定の仕方によってwrite/readと等価となる。

したがって、nginx側でエラーが発生するとしたら、connect/send/recvのどこかの可能性が高い。

一方でserver側に必要なものは、listen/accept/read/write。

php-fpmを見る限りはこれに加えてpollも含まれるだろうか。

そして最終的にLISTEN以外のコネクションは閉じる必要があるためcloseが必要となる。

ざっと文字にしただけでも関連する関数が多いことが分かるだろう。

M/Wを作成する際は確認が必須だが、今の目的はエラーの調査だ。

そのため、今回エラーが発生していた個所だけ見てみることとする。

### 1. エラー発生個所、clientのconnect

今回socketを調べるきっかけとなったのが以下のエラーである。

```
[error] 19932#0: *81 connect() to unix:/var/run/php-fpm/load-of-web.sock failed (11: Resource temporarily unavailable) while connecting to upstream, client: 127.0.0.1, server: localhost, request: "GET / HTTP/1.1", upstream: "fastcgi://unix:/var/run/php-fpm/load-of-web.sock:", host: "localhost"
```

これが発生するまでの経路は以下の通り。
nginxがfastcgiモジュールでunix socketにconnectするところで出力されている。

`connect() to ... load-of-web.sock` と出力されていることからも認識と相違が無い。

* ngx_http_fastcgi_handler => ngx_h ttp_client_request_body (https://github.com/nginx/nginx/blob/master/src/http/modules/ngx_http_fastcgi_module.c#L748)
* ngx_http_client_request_body => post_handler(https://github.com/nginx/nginx/blob/master/src/http/ngx_http_request_body.c#L201)
* post_handlerはngx_http_upstream_initの関数ポインタ(https://github.com/nginx/nginx/blob/master/src/http/ngx_http_upstream.c#L513)
* ngx_http_upstream_init => ngx_http_upstream_init_request(https://github.com/nginx/nginx/blob/master/src/http/ngx_http_upstream.c#L545)
* ngx_http_upstream_init_request => ngx_http_upstream_connect(https://github.com/nginx/nginx/blob/master/src/http/ngx_http_upstream.c#L729)
* ngx_http_upstream_connect => ngx_event_connect_peer(https://github.com/nginx/nginx/blob/master/src/event/ngx_event_connect.c#L205)

Resource temporary unavailableはEAGAINのエラーによるもの。(https://linuxjm.osdn.jp/html/LDP_man-pages/man3/errno.3.html)
これはログから、11: Resource temporarily unavailableが出ていることからも明らかである。

以上からconnectのマニュアルを読んでみる。

オンラインのconnectのマニュアルによるとUnixドメインソケットとルーティングキャッシュにエントリーが十分にないらしい。(https://linuxjm.osdn.jp/html/LDP_man-pages/man2/connect.2.html)
ただし、ルーティングキャッシュがarpキャッシュのことだとするとunix socketとは関係が無いと考えられる。
manコマンド(`man connect`)を確認してみるとEAGAINのエラー番号がそもそもない。

根拠としては希薄だが、nginxのソースコードのコメントにその答えが書いてある。

https://github.com/nginx/nginx/blob/master/src/event/ngx_event_connect.c#L221

どうやらconnectに失敗するとECONNREFUSEDの代わりに、EAGAINが返ってくるらしい。

そしてそれはlisten queueが一杯になっている時だという。

マニュアルに載っていないので実験で確かに同じエラーになるかを確認する。

```nginx.conf
user  nginx;
worker_processes  2;

error_log  /var/log/nginx/error.log warn;
pid        /var/run/nginx.pid;


events {
  worker_connections  2048;
}


http {
  include       /etc/nginx/mime.types;
  default_type  application/octet-stream;

  log_format  main  '$remote_addr - $remote_user [$time_local] "$request" '
    '$status $body_bytes_sent "$http_referer" '
    '"$http_user_agent" "$http_x_forwarded_for" "$request_time"';

  access_log  /var/log/nginx/access.log  main;

  sendfile        on;

  keepalive_timeout  65;

  include /etc/nginx/conf.d/*.conf;
}
```

```nginx.d/default.conf
server {
    index index.php index.html;
    server_name localhost;
    error_log  /var/log/nginx/error.log;
    access_log /var/log/nginx/access.log main;
    root /var/www/html;

    location / {
        try_files $uri $uri/ /index.php$is_args$args;
    }

    location ~ \.php$ {
        fastcgi_pass unix:/var/run/php-fpm/load-of-web.sock;
        fastcgi_index index.php;
        fastcgi_param SCRIPT_FILENAME $document_root/$fastcgi_script_name;
        include fastcgi_params;
    }
}
```

```php-fpm.conf
include=/etc/php-fpm.d/*.conf

[global]
pid = /var/run/php-fpm/php-fpm.pid
error_log = /var/log/php-fpm/error.log
log_level = warning
process_control_timeout = 10
daemonize = yes
rlimit_files = 4096
```

```php-fpm.d/www.conf
[www]

user = user-data 
listen = /var/run/php-fpm/load-of-web.sock
listen.owner = user-data
listen.group = user-data
listen.backlog = 2

pm = dynamic
pm.max_children = 2
pm.start_servers = 2
pm.min_spare_servers = 2
pm.max_spare_servers = 2
```

```/var/www/html/index.php
<?php

sleep(10);
print("hello\n");
```

上記で起動し、並列で6つ curl http://localhost/index.php を実行する。
最後に実行した2つのリクエストは前述したエラーログと共に、502エラーとなって返ってくる。

以上から、nginxのコメントは正しく、listen queueが溢れるとエラーとなることが分かる。

また、今回のエラーの発生原因は、listen queueが何らかの理由で溢れてしまったということが分かった。

## 終わりに

socket通信は奥が深く、一朝一夕にまとめられるものではないことが分かった。

当初エラーを網羅して記載しようとしていたのだが、介するシステムコールが多すぎて網羅してもあまり意味が無く無意味だと悟った。

そしてこれを書き始めてからエラーが発生する条件と原因が判明しやる気も失った。

しかし、ここまで書いたことが分かれば、エラー確認や、M/Wの作成のとっかかりとすることができるため、今後困ることは無いように思える。
