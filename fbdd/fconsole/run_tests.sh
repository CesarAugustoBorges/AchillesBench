#!/bin/bash

BLOCK_1="4096:0"
BLOCK_2="4096:4096"
BLOCK_3="4096:8192"
BLOCK_4="4096:12288"

BLOCK_9="4096:32768"
BLOCK_10="4096:36864"
BLOCK_11="4096:40960"

HASH_1="(4096)A"
HASH_2="(4096)B"
HASH_3="(4096)C"
HASH_4="(4096)ABC"

if [ ! -z $1 ]
then
    #It is supposed that the device in $1 is fresh, that it, 
    #no faults injeted and its content its only 0"s

    echo "/************************************************************/"
    echo "/****************** Block Mode Tests ************************/"
    echo "/************************************************************/"
    
    echo "-------------------- Basic operations ------------------------"
    #simple writting and check in B1
    echo -w "($BLOCK_1)A" -c "==($BLOCK_1)A"
    sudo ./fconsole -d $1 -w "($BLOCK_1)A" -c "==($BLOCK_1)A"

    #simple writting and checking in B3
    echo -w "($BLOCK_3)C" -c "==($BLOCK_3)C"
    sudo ./fconsole -d $1 -w "($BLOCK_3)C" -c "==($BLOCK_3)C"

    echo "---------------------- Write faults --------------------------"

    #adding write bit flip and checking content is not equal in B2
    echo -w "($BLOCK_2)ABCD" -c "==($BLOCK_2)ABCD"
    sudo ./fconsole -d $1 -b "($BLOCK_2)" -w "($BLOCK_2)ABCD" -c "!=($BLOCK_2)ABCD"

    #adding write slow disk and checking content is not equal in B4
    echo -s "($BLOCK_4)5000" -w "==($BLOCK_4)ABCD" -c "==($BLOCK_4)ABCD"
    sudo ./fconsole -d $1 -s "($BLOCK_4)2000" -w "($BLOCK_4)ABCD" -c "==($BLOCK_4)ABCD"

    #adding write medium error and checking if an error ocurred in B1
    echo -m "($BLOCK_1)" -w "($BLOCK_1)A" -e true
    sudod ./fconsole -d $1 -m "($BLOCK_1)" -w "($BLOCK_1)A" -e true

    echo "---------------------- Read faults ---------------------------"

    #adding read bit flip error and cheching content is not equal in B3
    echo -B "($BLOCK_3)" -c "!=($BLOCK_3)C"
    sudo ./fconsole -d $1 -B "($BLOCK_3)" -c "!=($BLOCK_3)C"

    #adding read slow disk and checking content is not equal in B4
    echo -S "($BLOCK_4)2000" -c "==($BLOCK_4)ABCD"
    sudo ./fconsole -d $1 -S "($BLOCK_4)2000" -c "==($BLOCK_4)ABCD"

    #adding read medium error in B3 
    echo -M "($BLOCK_3)" -r "($BLOCK_3)" -e true
    sudo ./fconsole -d $1 -M "($BLOCK_3)" -r "($BLOCK_3)" -e true

    echo "/************************************************************/"
    echo "/******************* Hash Mode Tests ************************/"
    echo "/************************************************************/"

    echo -H -s "$HASH_1" -w "($BLOCK_9)A"
    sudo ./fconsole -d $1 -H -s "2000$HASH_1" -w "($BLOCK_9)A"

    echo -H -S "$HASH_1" -c "==($BLOCK_9)A"
    sudo ./fconsole -d $1 -H -S "2000$HASH_1" -c "==($BLOCK_9)A"

    echo -H -m "$HASH_2" -w "($BLOCK_10)B" -e true
    sudo ./fconsole -d $1 -H -w "($BLOCK_10)B" -m "$HASH_2" -w "($BLOCK_10)B" -e true

    echo -H -M "$HASH_2" -r "($BLOCK_10)" -e true
    sudo ./fconsole -d $1 -H -M "$HASH_2" -c "==($BLOCK_10)B" -e true
 
    echo -w "($BLOCK_11)C" -H -B "$HASH_3" -c "!=($BLOCK_11)C"
    sudo ./fconsole -d $1 -w "($BLOCK_11)C" -H -B "$HASH_3" -c "!=($BLOCK_11)C"

    echo -H -b "$HASH_3" -w "($BLOCK_11)C" -c "!=($BLOCK_11)C"
    sudo ./fconsole -d $1 -H -b "$HASH_3" -w "($BLOCK_11)C" -c "!=($BLOCK_11)C"
else 
    echo "You must specify the device path"
fi

