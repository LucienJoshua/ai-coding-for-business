rm -rf ../libzlog
mkdir ../libzlog
rm -rf ./src/build
make clean
make PREFIX=./build
make PREFIX=./build install
cp -r ./src/build/* ../libzlog

