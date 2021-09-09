#! /bin/bash

EXEC_PATH=./stats.sh
if [[ $# -gt 2 ]]
then
    START_I=$(($1+0))
    END_I=$(($2+0))
    for var in ${@:3}
        do
            echo "starting $var run..."
            echo "$EXEC_PATH $START_I $END_I $var"
            $EXEC_PATH $START_I $END_I $var
        done    
else 
    echo "use mult_stats.sh <start> <end> [<run_id>]"
fi