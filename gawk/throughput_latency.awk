#!/usr/bin/gawk -f

BEGIN { FS= " "; RS = "\n"; TP_DIF_STD=0; LAT_DIF_STD=0; TP_COUNT=0; LAT_COUNT=0;}

FILENAME ~ ".*troughput" {
    TP[TP_COUNT]=$1
    #print TP[TP_COUNT]
    #print "\""$1"\""
    TP_COUNT++
    next
}

FILENAME ~ ".*latency" {
    LAT[LAT_COUNT]=$1
    LAT_COUNT++
    #print $1
}

END {
    TP_TOT = 0.0;
    LAT_TOT= 0.0;
    for(i in TP){
	TP_TOT += TP[i];
	LAT_TOT += LAT[i];
    }
    #printf "total: %.3f\n" , TP_TOT
    #print LAT_TOT
    TP_MEAN = TP_TOT / TP_COUNT;
    LAT_MEAN = LAT_TOT / LAT_COUNT;
    #print TP_MEAN
    #print LAT_MEAN
    TP_DIF_STD = 0;
    LAT_DIF_STD = 0;
    for(i in TP){
        TP_DIF_STD += (TP[i]- TP_MEAN) ^2
        LAT_DIF_STD += (LAT[i] - LAT_MEAN) ^2
    }
    TP_STD_DESVIATION = sqrt(TP_DIF_STD/TP_COUNT)
    LAT_STD_DESVIATION = sqrt(LAT_DIF_STD/LAT_COUNT)
    print "#tp_mean lat_mean tp_std_desviation lat_std_destiation" > out
    printf "%.3f %.3f %.3f %.3f\n", TP_MEAN, LAT_MEAN, TP_STD_DESVIATION, LAT_STD_DESVIATION > out
}
