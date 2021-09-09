
# FBDD

**FBDD** *(Faulty Block Device Driver)* is a block oriented driver as the name suggests, it is used to inject faults, these are defined in **Fault Library** *(/fault/fault.h)*, into another **Block Device**, all operations made to this driver are forwarded to the other block device, for now only writes/reads. Before a fault is injected, it is necessary for **FServer** to handle a request regarding the fault, more information about the available requests can be found in */fsocket* README.md. 
**FBDD** uses **BDUS** driver callbacks to accomplish its porpuses, the core code of **FBDD** is from **BDUS** and it doesn't own none if it's rights, **FBDD** follows **BDUS** licenses and copyrights.
	
# Compiling
	$ make

# Execution
To **FBDD** to run it is needed another device to be ready so all operations are forwarded, if you ran *make* there is a executable named *"ram"* that creates a block device using **BDUS** with 1GB of *RAM*. To create this device run the following command: 

	$  ./ram

Most probably it will output a path to an available drive, let's assume it is *"/dev/bdus-0"*, if it isn't an error message and it is another path replace the assumed path. To run **FBDD** just use the following command:

	$ ./fbddriver /dev/bdus-0

If everything went as expected **FBDD** printed the path to it's driver, let's also assume the path is *"/dev/bdus-1"*,  if it isn't an error message and it is another path replace the assumed path. To inject faults a console program (*FConsole*) can be used to send requests to **FServer** so the fault can be injected in the furture or read/write operations to **FBDD**. More information about this program can be found in the folder */fconsole*.
