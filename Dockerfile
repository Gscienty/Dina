FROM alpine:latest

ADD . /tmp/dina-module
RUN apk sed -i 's/dl-cdn.alpinelinux.org/mirrors.aliyun.com/g' /etc/apk/repositories \
        && apk --no-cache add curl cmake gcc openjdk8 autoconf automake libtool libc-dev cppunit-dev make pcre-dev pcre \
        && curl -o /tmp/apache-zookeeper-3.5.6.tar.gz https://mirrors.tuna.tsinghua.edu.cn/apache/zookeeper/zookeeper-3.5.6/apache-zookeeper-3.5.6.tar.gz \
        && curl https://mirrors.tuna.tsinghua.edu.cn/apache/ant/binaries/apache-ant-1.9.14-bin.tar.gz -o /tmp/apache-ant-1.9.14-bin.tar.gz \
        && tar zxvf /tmp/apache-zookeeper-3.5.6.tar.gz -C /tmp/ \
        && tar zxvf /tmp/apache-ant-1.9.14-bin.tar.gz -C /tmp/ \
        && cd /tmp/apache-zookeeper-3.5.6 \
        && /tmp/apache-ant-1.9.14/bin/ant compile_jute \
        && cd /tmp/apache-zookeeper-3.5.6/zookeeper-client/zookeeper-client-c \
        && autoreconf -if \
        && ./configure --disable-shared \
        && make -i \
        && curl http://nginx.org/download/nginx-1.17.7.tar.gz -o /tmp/nginx-1.17.7.tar.gz \
        && tar zxvf /tmp/nginx-1.17.7.tar.gz -C /tmp/ \
        && cd /tmp/nginx-1.17.7 \
        && ZOO_PATH=/tmp/apache-zookeeper-3.5.6/zookeeper-client/zookeeper-client-c \
        ./configure \
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
        --add-module=/tmp/dina-module \
        && make \
        && make install \
        && cp /tmp/dina-module/nginx.template.conf /usr/local/nginx/conf/nginx.template.conf \
        && apk del curl cmake gcc openjdk8 autoconf automake libtool libc-dev cppunit-dev make pcre-dev
EXPOSE 8080
CMD envsubst < /usr/local/nginx/conf/nginx.template.conf > /usr/local/nginx/conf/nginx.conf && /usr/local/nginx/sbin/nginx -g "daemon off;"
