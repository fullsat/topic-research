```
gcc -o server server.c
gcc -o client client.c
./server &
./client
rm server
rm client
```

よく忘れる通信の流れ

## server

socket - bind - listen - accept - read

* socket作成
* socketとファイルシステムを結びつける(TCP的にはIPとポートの登録)
* acceptされるソケットであると指定するのとacceptされるまでに受け付けられる接続のqueueのサイズを決定
* 接続があるまで待機(実用的にはepolなどでノンブロックにする)
* acceptできたらreadする(acceptで払いだされた最初に作ったソケットとは別のソケットを使う)

## client

socket - connect - write

* socket作成
* 通信相手のアドレスを用意してconnect
* connect出来たらwriteする
