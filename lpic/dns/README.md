## 背景/同期

LPIC3まで取りたくなった。
LPIC201はクリアしてるから、まずはLPIC202をクリアしたい。

BINDを利用しないからBINDのことがさっぱり分からない。
ついでにDNSに関して曖昧な部分もあるためその点に関して調べて起きたい。
突き詰めると奥が深すぎるので試験範囲内で疑問点をしらみつぶしにつぶしておく。

## 目的

* 試験範囲確認

## 試験範囲確認

* DNSの仕組み
* nslookup/dig/hostコマンド全般
* BIND
* rndcコマンド
* BINDのセキュリティ
  * ゾーン転送制限
  * 再帰問い合わせ制限
  * バージョン番号隠蔽
  * 一般ユーザでのBIND実行
  * DNSSEC
  * TSIG

## 追加の疑問

* お名前comで管理できるのはどの範囲
* 教本以外でのセキュリティは？
* Linux上での名前関連の設定

## DNSの仕組み

```
Client -> キャッシュサーバ -> .
                           -> .jp
                           -> example.jp
```

ゾーン情報を持つものがコンテンツサーバ

実際に階層的に問い合わせするのがキャッシュサーバ


## nslookup/dig/hostコマンド全般

マニュアル見ろって話

* ネームサーバ指定での検索

```
dig @8.8.8.8 google.com a
nslookup -type=A google.com 8.8.8.8
```

* digの良い感じの使い方

アンサーセクションだけ出力してくれる

マニュアルで `+[no]hogehoge` と書かれている部分を見れば、何が出力/抑制できるかが分かる

```
dig +noall +answer google.com
```

## BIND

srcを参照

家庭用のプライベートDNSを構築してみた。


## rndcコマンド

現在の状態

```
rndc status
```

ゾーン削除

```
rndc delzone furupaka
```

DNSキャッシュサーバのキャッシュ削除

```
rndc flush
```

DNSキャッシュサーバのキャッシュをダンプ(DBは勝手に作られる)

```
rndc dumpdb
```

### BINDのセキュリティ

```
DNS Contents(master) ==1.== DNS Contents(slave)
|3.                              |
DNS Cache ------------------------
|
(DNS Forwarder)
|2.
Client
```

1. ゾーン転送制限

masterにて

```
zone "hogehoge" {
  allow-transfer {
    xx.xx.xx.xx;
  };
};
```

2. 再帰問い合わせ制限

```
options {
  allow-query { xx.xx.xx.xx/24; };
  allow-recursion { xx.xx.xx.xx/24; };
};

zone "example.com" {
  allow-query { any; };
};
```

3. バージョン番号隠蔽

```
options {
  version { none; };
};
```

4. 一般ユーザでのBIND実行

```

```

5. DNSSEC

キャッシュがゾーンの情報を問い合わせるときに正当性を確認する

鍵の種類

* ZSK.pub/ZSK.key
* KSK.pub/KSK.key

レコードの種類

* DNSKEY = ZSK.pub(256)/KSK.pub(257)
* DS = KSK.pubのハッシュ
* RRSIG = 例えばexample.comのAレコードををZSK.keyで署名したもの
* NSEC = 不存在を証明するためのレコード

復号までの流れ

* example.comのAレコードを問い合わせる
* example.comのRRSIGを問い合わせる
* example.comのDNSKEY(ZSK.pub)を問い合わせる
* DNSKEY(ZSK.pub)からRRSIGレコードの署名が公開鍵で復号できるか確認する
---
* example.comのDNSKEY(ZSK.pubとKSK.pubどちらも)を問い合わせる
* example.comのDNSKEY(ZSK.pub)に対するRRSIGレコードを問い合わせる
* comにexample.comのDSレコードを問い合わせる
* KSK.pubでRRSIGレコードの署名が復号できるか確認する
* DSレコードとDNSKEY(KSK.pub)のハッシュを比較する
---
* comのDNSKEY(ZSK.pubとKSK.pubどちらも)を問い合わせる
* comのDNSKEYに対するRRSIGレコードを問い合わせる
* .にcomのDSレコードを問い合わせる
* KSK.pubでRRSIGレコードの署名が復号できるか確認する
* DSレコードとDNSKEY(KSK.pub)のハッシュを比較する
---
* .のDNSKEY(ZSK.pub)を問い合わせる
* .のDNSKEY(ZSK.pub)に対するRRSIGレコードを問い合わせる
* キャッシュサーバに存在する.のKSK.pubでRRSIGレコードの署名が復号できるか確認する

6. TSIG

## お名前comで管理できるのはどの範囲

サービスを見てみると、基本的にmasterで動作する。

* masterに対するスレーブのためのサービス
* ゾーン転送したあとのセカンダリネームサーバとしてのサービス
* DNSSECなどもあった
* 基本的にゾーン情報を持ったネームサーバとして利用できる
* そのままゾーン転送して例えばRoute53で管理することも可能

## 教本以外でのセキュリティは？

DoHなどはみたが、まだ仕様策定期っぽいので今回は見送り。

## Linux上での名前関連の設定

* /etc/resolver.conf で nameserver XX.XX.XX.XXで割と十分


