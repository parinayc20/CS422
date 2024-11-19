#!/bin/bash

./mipc ~/Ksim/Bench/testcode/asm-sim/example
cat mipc.log > data/asm-sim.log

./mipc ~/Ksim/Bench/testcode/c-sim/example
cat mipc.log > data/c-sim.log

./mipc ~/Ksim/Bench/testcode/endian/endian
cat mipc.log > data/endian.log

./mipc ~/Ksim/Bench/testcode/factorial/factorial
cat mipc.log > data/factorial.log

./mipc ~/Ksim/Bench/testcode/fib/fib
cat mipc.log > data/fib.log

./mipc ~/Ksim/Bench/testcode/hello/hello
cat mipc.log > data/hello.log

./mipc ~/Ksim/Bench/testcode/host/host
cat mipc.log > data/host.log

./mipc ~/Ksim/Bench/testcode/ifactorial/ifactorial
cat mipc.log > data/ifactorial.log

./mipc ~/Ksim/Bench/testcode/ifib/ifib
cat mipc.log > data/ifib.log

./mipc ~/Ksim/Bench/testcode/log2/log2
cat mipc.log > data/log2.log

./mipc ~/Ksim/Bench/testcode/msort/msort
cat mipc.log > data/msort.log

./mipc ~/Ksim/Bench/testcode/rfib/rfib
cat mipc.log > data/rfib.log

./mipc ~/Ksim/Bench/testcode/Subreg/subreg
cat mipc.log > data/subreg.log

./mipc ~/Ksim/Bench/testcode/towers/towers
cat mipc.log > data/towers.log

./mipc ~/Ksim/Bench/testcode/vadd/vadd
cat mipc.log > data/vadd.log
