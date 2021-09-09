#!/usr/bin/gawk -f

BEGIN { FS= "[ ]"; RS = "\n"; time=1.0;
    print "#time ~CPU% ~MEM%" > out
}

FNR > 1 {
    time = FNR - 1
    CPU[time]+=$2+0
    count[time]++
    MEM[time]+=$3+0 
    next
}

{ }

END {
    CPU_TOT=0;
    MEM_TOT=0;
    for(i in count){
        CPU_TOT += CPU[i]/count[i]
        MEM_TOT += MEM[i]/count[i]
        print i" "CPU[i]/count[i]" "MEM[i]/count[i] > out
    }
    CPU_MEAN = CPU_TOT / length(count);
    MEM_MEAN = MEM_TOT / length(count);
    CPU_DIF_STD = 0;
    MEM_DIF_STD = 0;
    for(i in count){
        CPU_DIF_STD += (CPU[i]/count[i] - CPU_MEAN) ^2
        MEM_DIF_STD += (MEM[i]/count[i] - MEM_MEAN) ^2
    }
    CPU_STD_DESVIATION = sqrt(CPU_DIF_STD/length(count))
    MEM_STD_DESVIATION = sqrt(MEM_DIF_STD/length(count))
    print "#cpu_mean mem_mean cpu_std_desviation mem_std_destiation" > general_out
    print CPU_MEAN" "CPU_STD_DESVIATION" "MEM_MEAN" "MEM_STD_DESVIATION > general_out
}
