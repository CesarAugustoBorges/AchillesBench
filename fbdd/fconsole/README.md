
# FConsole

**FConsole** *(FaultyConsole)* is a simple command based program used to write/read directly to/from block devices, the main porpuse of this program is to be used with **FBDD** *(Faulty Block Device Driver)*. All writes are made from a pointer named **offSet** and all read are made from another pointer named **lastWrite**. All arguments without the prefix '-' are ignored and don't affect the execution in any way.
Before explaining how **FConsole** works these are all current valid arguments:
- *-d | --device \<device_path\>* : Defines the device in which the write/read operations will be made.
- *-w | --write \<content\>* : writes the content to the device.\*\*1
- *-r | --read* : read the last written content in the device
- *-c | --check \<content\>* : compares the last written content in the device with the specified content in the argument.\*\*1
- *-a | -- append* : Sets the write/read mode to appending, writes are appended, **offSet** is incremented as needed and a pointer to last write is saved.
- *-p | --prefix* : Sets the write/read mode to prefix, writes and reads are always done in **offSet** = 0. This mode is set as the **default** one.
- *-o | --offSet*\<index\>* : changes **offSet** pointer to **index**, use it carefully, writes are made from this pointer, if **index** is not a number it will be set to 0 by **atoi**.
- *-i | --lastWrite\<index\>* : changes **lastWrite** pointer to index, use it carefully, reads are made from this pointer, if **index** is not a number it will be set to 0 by **atoi**.
- *-b | --bit_w (S:O)* : adds a bit flip fault into the block with offSet=**O** and size=**S**, when **FBDD** receives a write.
- *-B | --bit_r (S:O)* : adds a bit flip fault into the block with offSet=**O** and size=**S**, when **FBDD** receives a read.
- *-s | --disk_w (S:O)T* : adds a slow disk fault into the block with offSet=**O** and size=**S**, slowing the operation for T milliseconds, when **FBDD** receives a write.
- *-S | --disk_r (S:O)T* : adds a slow disk fault into the block with offSet=**O** and size=**S**, slowing the operation for T milliseconds,  when **FBDD** receives a read.
- *-m | --medium_w(S:O)* : adds a medium error into the block with offSet=**O** and size=**S**, when **FBDD** receives a write.
- *-M | --medium_r(S:O)* : adds a medium error into the block with offSet=**O** and size=**S**, when **FBDD** receives a read.

> \*\*1 \<content\> can be replaced with (S:O)\<replicate\> creating a block off size=**S**, it's content is filled with \<replicate\> as many times as needed, then this block will be used at offSet=**O**. For example \"(4:0)AB\" will result in the block "\ABAB\" with offSet=**O**.
  
The only required argument is -d, it is necessary to known the device to which the write/read operations are directed to. However, using only this argument doesn't have any effect of the device, it only checks if it possible to open it or not.
**All** these arguments can be seen as commands, for exemple it is possible to use the following command *"$ ./fconsole \<device_path\> -w AAAA -w BBBB -o 3 -w CC -o 0 -w D"* that should write in the device the content AAACCDB in the **offSet** = 0;
# Examples
Lets suppose the device **/dev/bdus-0** exists, it is functional, has permission for write and read and it has at least 4k bytes of space.

## Writing

- Writing the content "AAAA"
	>$ ./fconsole -d /dev/bdus-0 -w AAAA
- Writing the content "AAAB" with the current content "AAAA"
	>$ ./fconsole -d /dev/bdus-0 -w AAAB
	or
	$./fconsole -d /dev/bdus-0 -o 3 -w B
- Writing the content "AAABAAAC"
	>$ ./fconsole -d /dev/bdus-0 -w AAABAAAC
	or
	$ ./fconsole -d /dev/bdus-0 -a -w AAAB -w AAAC

## Reading

- Reading with the current content "AAAA"
	>$ ./fconsole -d /dev/bdus-0 -r
- Reading after the command * "$./fconsole -d /dev/bdus-0 -w AAABAAAB -r"*
	> returns AAAABAAAB
- Reading with the command *"$/fconsole -d /dev/bdus-0 -a -w AAAB -w AAAC" -r*
	> returns AAAC, because it was the last write
- Reading with the command *"$/fconsole -d /dev/bdus-0 -p -w AAAB -w AAAC -r"*
	> returns AAAC, like in the previous example, last example has the content AAABAAAC in the device and in this example the device has the content AAAC due to the changes of APPEND mode to PREFIX mode.
	
## Adding faults
 - Injecting a bit flip on write to a block with size=4096 and offSet=0
 	>$ ./fconsole -d /dev/bdus-0 -b '(4096:0)'
 - Injecting a slow disk on read to a block with size=4096, offSet=0 and the delay=2000 milliseconds
 	>$ ./fconsole -d /dev/bdus-0 -S '(4096:0)2000'
 - Injecting a medium error on write to a block with size=4096 and offSet=0
 	>$ ./fconsole -d /dev/bdus-0 -m '(4096:0)'

# Compiling
   $ make

# Know limitations
   read or writes operations are limited to 4k bytes, since it is assumed the block size is 4k.
    -d option should be declared before any write/read or any fault requests.
    -o -i options don't have any cap (this will be solved on further updated)
