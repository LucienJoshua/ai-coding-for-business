#/bin/bash
lines=20
tail +$lines $0 >/tmp/tls_install.tar.gz
tar -jxvf /tmp/tls_install.tar.gz -C /tmp
cp -r /tmp/tls_install/tls /usr/local/bin
cp -r /tmp/tls_install/tlsconfig /usr/local/bin
cp -r /tmp/tls_install/zlog.conf /usr/local/bin
cp -r /tmp/tls_install/libvsoa /usr/local/lib
cp -r /tmp/tls_install/libzlog /usr/local/lib
cp -r /tmp/tls_install/tls.conf /etc/ld.so.conf.d
cp -r /tmp/tls_install/libzlog.conf /etc/ld.so.conf.d
ldconfig
cp -r /tmp/tls_install/tls.service /etc/systemd/system
systemctl daemon-reload
systemctl enable tls
systemctl start tls
rm -rf /tmp/tls_install
rm -rf /tmp/tls_install.tar.gz
exit 0
