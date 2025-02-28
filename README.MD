# LockCheck- Deadlock and Starvation detection tool

## Description
LockCheck started as a university project for the Operating Systems course. Now I work on it when I find the time
At the moment it can only detect deadlocks in multithreaded applications that use the pthread and semaphore APIs(as the resources used can be mutexes and semaphores). In order to detect deadlock a modified version of the banker's algorithm was used.

## Usage
- git clone https://github.com/lucan8/LockCheck.git
- In the working directory of the project run: ./lock_check path_to_your_executable

### App Structure
- lock_check.cpp: Main source file(currently all classes and helper functions are there)
- sys_hook.c: Contains all of the hooked relevant mutex, semaphore and threads functions for keeping track of resource allocations and requests
- stefan_sys_hook.c: template version of sys_hook.c(not tested)