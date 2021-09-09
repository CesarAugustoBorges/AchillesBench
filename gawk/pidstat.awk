#!/usr/bin/gawk -f

BEGIN { FS= "[ ]+"; RS = "\n";
    print "#time CPU% MEM%" > out
    last_time = 0;
    initial_time = 0;
    pid_parent = 0; 
}

NR == 4 {
    initial_time = $1;
    pid_parent = $3;
}

NR > 2 && $1 ~ /[0-9]+/ {
    if(last_time != $1 && last_time){
        time = last_time+1-initial_time;
        print_statistics(time);
    }
    last_time = $1;
    if(is_valid_pid($3)){
        CPU[last_time-initial_time] += fix_number($8)
        MEM[last_time-initial_time] += fix_number($14)
    }
    next
}

{ }

END {
    print_statistics(last_time+2-initial_time);
}

function rm_last_char(str){
    return substr(str, 1, length(str)-1)
}

function ch_number_pontuation(str){
    gsub(",", ".", str)
    return str;
}

function fix_number(str){
    return ch_number_pontuation(rm_last_char(str));
}

function is_valid_pid(pid){
    return pid == pid_parent || pid > pid_parent + 2
}

function print_statistics(time){
    print time" "CPU[last_time-initial_time]" "MEM[last_time-initial_time] > out
}
