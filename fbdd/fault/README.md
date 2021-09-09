# Fault Library

**Faut Library** is used by **BDUS** callbacks to inject the faults into **BDUS** device.
All these faults are injected in the *fbd\_check_\and\_inject\_fault* function found in *fbd_structs.h*, this called by **BDUS** write/read callbacks.

# Current faults supported:

- **Slow disk**: slows the read or write operation done to the underlying device, the time in which the operation is slowed is passed in miliseconds.
- **Bit Flip**: for the current version this fault flips the rightest bit of the desired block, this fault can be used for write or read operations.
- **Medium Error**: returns a medium error to the system call used to write or read in the specified block. This error makes a certain block unreachable.
