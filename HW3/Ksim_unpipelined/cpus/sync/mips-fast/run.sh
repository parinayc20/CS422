#!/bin/bash

cd $SIMDIR/Bench/lib
gmake
cd $SIMDIR/Bench/testcode
for d in */; do
        cd $d
        gmake
        cd ../
done

cd $SIMDIR/cpus/sync/mips-fast_unpipelined
# gmake clobber clean
gmake

for d in $SIMDIR/Bench/testcode/*/; do
        for file in "$d"*; do
                if [ -f "$file" ]; then
                        if [[ $file == */boot.image ]]; then
                                continue
                        fi

                        if [[ $file == *.image ]]
                        then
                                echo "----------------------------------------------------------------------------------"
                                ./mipc ${file%%.*}

                                # Echo stats in other file
                                LOGBASE=$SIMDIR/cpus/sync/mips-fast_unpipelined/logs_unpipelined
                                LOG=$(basename $d)
                                LOGFILE=$LOGBASE/${LOG%%.*}.log
                                if [ -f $LOGFILE ]; then
                                        cp $SIMDIR/cpus/sync/mips-fast_unpipelined/mipc.log $LOGFILE
                                else
                                        touch $LOGFILE
                                        cp $SIMDIR/cpus/sync/mips-fast_unpipelined/mipc.log $LOGFILE
                                fi
                                echo "----------------------------------------------------------------------------------"
                        fi
                fi
        done
done
