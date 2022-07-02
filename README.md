# igor.technology Video Service and Library (ITVSL)

## Install Protobuf

sudo apt install protobuf-compiler

## Compile Proto

First proto files must be compile and moved to `itvsl` root project. (/itvsl folder)

```
cd proto
cmake .
make
cp itvslprotocol.pb.* ../
```

## Debug with Valgrind

```
rm valgrind-out.txt
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=valgrind-out.txt ./myapp
```
