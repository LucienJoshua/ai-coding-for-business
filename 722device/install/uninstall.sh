#/bin/bash
systemctl stop tls
echo "stop tls service"
systemctl disable tls
echo "disable tls service"
rm -rf /etc/systemd/system/tls.service
echo "remove tls.service"
rm -rf /usr/local/bin/tls
echo "delete tls elf"
rm -rf /usr/local/bin/tlsconfig
echo "delete tls config files"
rm -rf /usr/local/lib/libvsoa
echo "delete tls libs"
rm -rf /usr/local/bin/zlog.conf
rm -rf /usr/local/lib/libzlog

rm -rf /etc/ld.so.conf.d/tls.conf
rm -rf /etc/ld.so.conf.d/libzlog.conf
echo "uninstall tls finish"
exit 0
