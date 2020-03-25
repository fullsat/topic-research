
```
docker run --name lpicdns -v $(pwd)/bind:/etc/bind -p 53:53 -p 53:53/udp  -d lpic/dns named -f -c /etc/bind/named.conf
```
