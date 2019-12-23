# Dina Nginx - Zookeeper 服务网关模块

本项目通过自定义Nginx模块实现服务网关。使用Zookeeper实现服务发现功能，可动态配置代理访问后端微服务。

## Nginx编译过程

Dina依赖nginx的`pcre`模块，因此编译安装nginx执行./configure时，*请勿添加*可选配置项`--without-http_rewrite_module`，否则Dina将仅能设定静态配置项。
在执行nginx的./configure时，还需要事先声明一个环境变量`ZOO_PATH`，该环境变量指定为Zookeeper的zookeeper-client-c路径。
在编译Dina的过程中，您需要预先编译好`libzookeeper.a`静态库，本文不赘述`libzookeeper.a`的编译过程。在本项目中，使用的是 zookeeper 3.5.6 的客户端静态库。

一个简易的Dina Nginx的编译执行shell脚本如下所示:
```
ZOO_PATH=/home/dev/zookeeper/zookeeper-client/zookeeper-client-c \
         ./configure --prefix=/home/dev/dina \
                     --without-http-cache \
                     --without-http_charset_module \
                     --without-http_gzip_module \
                     --without-http_ssi_module \
                     --without-http_userid_module \
                     --without-http_access_module \
                     --without-http_auth_basic_module \
                     --without-http_mirror_module \
                     --without-http_autoindex_module \
                     --without-http_status_module \
                     --without-http_geo_module \
                     --without-http_map_module \
                     --without-http_referer_module \
                     --without-http_fastcgi_module \
                     --without-http_uwsgi_module \
                     --without-http_scgi_module \
                     --without-http_grpc_module \
                     --without-http_memcached_module \
                     --without-http_limit_conn_module \
                     --without-http_limit_req_module \
                     --without-http_empty_gif_module \
                     --without-http_browser_module \
                     --without-http_upstream_hash_module \
                     --without-http_upstream_ip_hash_module \
                     --without-http_upstream_least_conn_module \
                     --without-http_upstream_random_module \
                     --without-http_upstream_keepalive_module \
                     --without-http_upstream_zone_module \
                     --without-mail_pop3_module \
                     --without-mail_imap_module \
                     --without-mail_smtp_module \
                     --without-stream_limit_conn_module \
                     --without-stream_access_module \
                     --without-stream_geo_module \
                     --without-stream_map_module \
                     --without-stream_split_clients_module \
                     --without-stream_return_module \
                     --without-stream_upstream_hash_module \
                     --without-stream_upstream_least_conn_module \
                     --without-stream_upstream_random_module \
                     --without-stream_upstream_zone_module \
                     --add-module=/home/dev/Dina \
                     && make \
                     && make install
```
## Dina Nginx Conf配置项

Dina 的配置项均在location下进行配置，分别包括 `dina_zk`、`dina_service`和`dina_action`三项，其中必填项为`dina_zk`和`dina_service`。

### dina\_zk 配置项

必填项，该配置项用于配置Zookeeper服务器IP及端口号，其格式为：`<IP>:<Port>[,<IP>:<Port>]...`。配置项用于让Dina获知Zookeeper的网络位置。

### dina\_service 配置项

必填项，该配置项用于配置Dina接收一个HTTP请求后获得到的服务名。服务名格式应与zookeeper中的路径位置一致。
Dina将根据捕获到的服务名向zookeeper询问该服务是否存在，并且获取在该服务名目录下是否存在子znode。
服务名目录下的子znode的名称为该服务的网络地址及服务端口号，格式为`<IP>:<Port>`，
若存在多个子znode，Dina将在这些子znode中随机选择一个转发HTTP请求，从而实现负载均衡。


### dina\_action 配置项

选填项，如果缺省该配置项，Dina将会把HTTP请求中的uri原样转发至对应的HTTP服务中。
如果填写相应的配置项，Dina在转发HTTP请求时，会将uri按照dina\_action的格式进行改写。
