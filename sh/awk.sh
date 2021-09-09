#! /bin/bash
DEDIS_DIR=../dedisbench-private
DATA_DIR=../data
AWK_PIDSTAT_EXEC=../gawk/pidstat.awk
AWK_PIDSTAT_ALL_EXEC=../gawk/statistics.awk
AWK_TP_LAT_EXEC=../gawk/throughput_latency.awk

exec_printable(){
    EXEC=$1
    echo $EXEC
    $EXEC
}

num_sqrt(){
    $SQRT = $(echo "$number" | awk '{print sqrt($1)}')
    return $SQRT
}

if [[ $# -eq 3 ]]
    then
        RUN_ID=$3
        RUN_DATA_DIR=$DATA_DIR/$RUN_ID
        AWK_DIR=$RUN_DATA_DIR/awk
	PIDSTAT_DIR=$RUN_DATA_DIR/pidstat
	DEDIS_DATA_DIR=$RUN_DATA_DIR/dedis
	TOTAL_THROUGHPUT=0
	TOTAL_LATENCY=0
        for i in `seq $1 $2`
            do
                AWK_OUT=$AWK_DIR/awk_$i
	 	DEDIS_DATA_FILE=$RUN_DATA_DIR/dedis/dedis_$i
                exec_printable "$AWK_PIDSTAT_EXEC -v out=$AWK_OUT $PIDSTAT_FILE $PIDSTAT_DIR/pidstat_*"
                echo "grep -o -E \"Throughput: [0-9]+\.[0-9]*?\" \"$DEDIS_DATA_FILE\" | awk '{print \$2}'"
                TROUGHPUT[$i]=`grep -o -E "Throughput: [0-9]+\.[0-9]*?" "$DEDIS_DATA_FILE" | awk '{print $2}'`
                LATENCY[$i]=`grep -o -E "Latency: [0-9]+\.[0-9]*?" "$DEDIS_DATA_FILE" | awk '{print $2}'`
            done
    DEDIS_THROUGHPUT_OUT=$DEDIS_DATA_DIR/troughput
    echo 'printf \"%s\n\" "${TROUGHPUT[@]}" > $DEDIS_THROUGHPUT_OUT'
    printf "%s\n" "${TROUGHPUT[@]}" > $DEDIS_THROUGHPUT_OUT
    DEDIS_LATENCY_OUT=$DEDIS_DATA_DIR/latency
    echo "printf \"%s\n\" "${LATENCY[@]}" > $DEDIS_LATENCY_OUT"
    printf "%s\n" "${LATENCY[@]}" > $DEDIS_LATENCY_OUT
    GNUPLOT_CPU="$GNUPLOT_DIR/cpu_mean.png"
    GNUPLOT_MEM="$GNUPLOT_DIR/mem_mean.png"
    AWK_ALL_OUT=$AWK_DIR/all
    AWK_GENERAL_OUT=$AWK_DIR/cpu_mem
    AWK_ALL_IN=$AWK_DIR/awk_*
    exec_printable "$AWK_PIDSTAT_ALL_EXEC -v out=$AWK_ALL_OUT -v general_out=$AWK_GENERAL_OUT $AWK_ALL_IN" 
    AWK_TP_LAT_IN="$DEDIS_DATA_DIR/troughput $DEDIS_DATA_DIR/latency"
    AWK_TP_LAT_OUT=$AWK_DIR/tp_lat
    exec_printable "$AWK_TP_LAT_EXEC -v out=$AWK_TP_LAT_OUT $AWK_TP_LAT_IN"
    echo "gnuplot -e \"set term png; set output \\\"$GNUPLOT_CPU\\\";plot \\\"$AWK_ALL_OUT\\\" u 1:2 with lines\""
    gnuplot -e "set term png; set output \"$GNUPLOT_CPU\";plot \"$AWK_ALL_OUT\" u 1:2 with lines"
    gnuplot -e "set term png; set output \"$GNUPLOT_MEM\";plot \"$AWK_ALL_OUT\" u 1:3 with lines"
else
    echo "wrong input"
fi
