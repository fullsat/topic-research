```
gcc -o server server.c
gcc -o client client.c
./server &
./client
rm server
rm client
```

## 上記の合間にSleepを入れる

* server
  * socket()
  * usleep()
  * bind()
  * usleep()
  * listen()
  * usleep()
  * accept()
  * usleep()
  * read()
  * usleep()
* acceptまで到達したらclientで以下を実施
  * socket()
  * usleep()
  * connect()
  * usleep()
  * write()

```
ls /tmp/server.sock
ss -aneex | grep server.sock
```

