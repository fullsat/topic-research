## 背景/動機

現在nginxとphp-fpmを利用していてsocket接続時にtemporary unavailableというエラーが出力される。
unix domain socketを利用しているがこれがどのような通信ソケットなのか基礎的なもの以外分かっていない。
そのためエラーが発生していたとしても根本原因がどこにあるのかが理解できない状況にある。

## 目的

unix domain socketがエラーを発生する場合を調べる
問題としてはnginxがphp-fpmのunix domain socketと通信するときにエラーを出していることだが、
php-fpmのコードを追ったとしてもエラーをどのようにハンドリングしているかしか分からないと予想されるため、
まずはOSとしてどのように扱っているかを調べる。


## 疑問

* ソケットに種類があるのか、それはどのように使うのか
* 通信時にエラーが発生する場合はどのような場合か
* 通信時に通信はどのように見えているのか
* パフォーマンスはどの程度なのか
* データのキャプチャはできるのか
* protocolのドキュメントはあるのか

## マニュアル

[UnixDomainSocketのマニュアル](https://linuxjm.osdn.jp/html/LDP_man-pages/man7/unix.7.html)

