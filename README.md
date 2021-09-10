# AchillesBench

Achilles is a prototype-, software-, and Linux-based fault-injection benchmark for local storage systems, it supports the evaluation of realistic storage deployments when subjected to different types of failures at the block device layer. Moreover, AchillesBench aims at assessing the impact of faults in both the performance and reliability of storage systems that resort to commonly used storage interfaces, namely file system (POSIX) and block device APIs. Besides evaluating reliability and performance AchillesBench can be used to find the "Achilles heel" of storage systems.

This project is composed of two different components, the benchmark, built on top of DEDISBench, and FBDD (Faulty block device driver), a virtual block device resorting to BDUS (Block device in user-space). More information about these components can be found in their respective foulders' README.md.

# Faults

AchillesBench supports the injection of several types of faults, namely: data corruption, I/O requests delays, and I/O errors. This is possible by intercepting I/O requests from the SUT to the underlying block device layer and injecting these different faults at run time.

# Dependencies
AchilesBench uses XXH3_128 hash and libyaml.
To install XXH3_128 hash check [xxHash github](https://github.com/Cyan4973/xxHash) and libyaml use the following command (or equivalent for your Linux distribution):
```
sudo apt-get install libyaml-dev
```
