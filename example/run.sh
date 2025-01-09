#!/bin/bash

function server()
{
exe="./build/example1"
cmake --build build --target main
if [[ -f $exe ]]; then
    $exe 9090 &
else
    echo "The executable file is not exists."
fi
}

function client()
{
    cmake --build build --target client
    for ((i = 1; i <= 10; i++)); do
        ./build/client $i &
        sleep 1
    done
}

function main()
{
    cd ../
    server > server.log
    client > client.log
    cd -
}

main
