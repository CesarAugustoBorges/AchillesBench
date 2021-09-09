#! /bin/bash
DEDIS_DIR=../dedisbench-private
DATA_DIR=../data
AWK_PIDSTAT_EXEC=../gawk/pidstat.awk
AWK_PIDSTAT_ALL_EXEC=../gawk/statistics.awk

exec_printable(){
    EXEC=$1
    echo $EXEC
    $EXEC
}

if [[ $# -eq 3 ]] 
then
    declare -a TROUGHPUT
    declare -a LATENCY
    START_I=$(($1+0))
    END_I=$(($2+0))
    RUN_ID=$3
    DEDIS_CONF_FILE=$DEDIS_DIR/conf/$3.ini
    if [[ -f $DEDIS_CONF_FILE ]]
    then
        RUN_DATA_DIR=$DATA_DIR/$RUN_ID
        PIDSTAT_DIR=$RUN_DATA_DIR/pidstat
        AWK_DIR=$RUN_DATA_DIR/awk
        GNUPLOT_DIR=$RUN_DATA_DIR/plots
        DEDIS_DATA_DIR=$RUN_DATA_DIR/dedis
        exec_printable "mkdir -p $RUN_DATA_DIR"
        exec_printable "mkdir -p $PIDSTAT_DIR"
        exec_printable "mkdir -p $AWK_DIR"
        exec_printable "mkdir -p $GNUPLOT_DIR"
        exec_printable "mkdir -p $DEDIS_DATA_DIR"
        for i in `seq $1 $2`
            do
                echo "Round $i"
                exec_printable "cd $DEDIS_DIR"
                PIDSTAT_FILE=$PIDSTAT_DIR/pidstat_$i
                DEDIS_DATA_FILE="$DEDIS_DATA_DIR/dedis_$i"
                DEDIS_RESULTS="$DEDIS_DATA_DIR/$i"
                exec_printable "mkdir -p $DEDIS_RESULTS"
                echo "./dedis_pidstat $DEDIS_DATA_FILE $PIDSTAT_FILE ./conf/$RUN_ID.ini"
                ./dedis_pidstat $DEDIS_DATA_FILE $PIDSTAT_FILE ./conf/$RUN_ID.ini
                exec_printable "cp -R ./results $DEDIS_RESULTS"
                DEDIS_EXTRA_RESULTS="./results/*0"
                exec_printable "rm $DEDIS_EXTRA_RESULTS"
                exec_printable "cd ../sh"
            done
        echo "Done"
    else
        echo "DEDISbench configuration $DEDIS_CONF_FILE file does not exists"
    fi
else
    echo "use sudo ./stats <start_index> <end_index> <run_id>"
fi
