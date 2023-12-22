#!/bin/bash

# Compile StorageServerModule
gcc StorageServerModule.c -pthread -o s

# Compile NamingServerModule
gcc NamingServerModule.c -pthread -o n

# Compile clientfunctions
gcc clientfunctions.c -o client

# Provide execution permissions to the compiled binaries
chmod +x s n client

rm ./src/s 
rm ./src1/s
rm ./src2/s 

cp s ./src/s
cp s ./src1/s
cp s ./src2/s
