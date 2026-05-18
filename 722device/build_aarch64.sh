#/bin/bash
echo "Build the tls setup program"

echo "Build the zlog lib..."
cd ./third
cd ./zlog-1.2.15
sh ./build_zlog.sh
cd ../
cd ../

echo "Build the tls app..."
rm -rf ./output
mkdir ./output
mkdir ./output/tls_install
mkdir ./output/tls_install/libvsoa
mkdir ./output/tls_install/libzlog
make clean
make

cd ./bin
mkdir tlsconfig
mkdir tlslog
cd ..
cp -r ./xmlfile/* ./bin/tlsconfig
cp -r ./com/zlog.conf ./bin/
chmod 777 ./bin/zlog.conf

cp -r ./bin/tls ./output/tls_install
cp -r ./bin/tlsconfig ./output/tls_install
cp -r ./bin/zlog.conf ./output/tls_install
cp -r ./third/libvsoa/libs/aarch64/* ./output/tls_install/libvsoa
cp -r ./third/libzlog/lib/* ./output/tls_install/libzlog

cp -r ./install/tls.conf ./output/tls_install
cp -r ./install/libzlog.conf ./output/tls_install
cp -r ./install/tls.service ./output/tls_install
cp -r ./install/install.sh ./output	
cp -r ./install/uninstall.sh ./output

cd ./output
tar -jcvf  tls_install.tar.bz2 tls_install
cat install.sh tls_install.tar.bz2 > tls_install.run
#rm -rf tls_install
#rm -rf tls_install.tar.bz2
#rm -rf install.sh
chmod 777 tls_install.run

installname=TLS_install
currenttime=$(date "+%Y%m%d_%H%M%S")
split=_
postfix=.run
filename=$installname$split$currenttime$postfix
mv ./tls_install.run ./$filename

echo "Build all finish"
