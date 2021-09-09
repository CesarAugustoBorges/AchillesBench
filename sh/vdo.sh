if [ ! -z $1 ]
then
    vdo create --device=$1 --name=vdobdus --vdoLogicalSize=699G --writePolicy=async --compression=disabled
else    
    echo "use vdo.sh <underlying-device>"
fi
