# LockCheck- Deadlock and Starvation detection tool

## Description
LockCheck started as a university project for the Operating Systems course. Now I work on it when I find the time.
At the moment it can only detect deadlocks in multithreaded applications that use the pthread and semaphore APIs(as the resources used can be mutexes and semaphores). In order to detect deadlock a modified version of the banker's algorithm was used.

## Usage
1. git clone https://github.com/lucan8/LockCheck.git
2. Compile the shared library: gcc -fPIC -shared -o sys_hook.so sys_hook.c -ldl
3. Compile the main file: g++ lock_check.cpp -o lock_check
4. Run: ./lock_check path_to_executable

### App Structure
- lock_check.cpp: Main source file(currently all classes and helper functions are there)
- sys_hook.c: Contains all of the hooked relevant mutex, semaphore and threads functions.
- stefan_sys_hook.c: template version of sys_hook.c(not tested)
- log_file.txt: Generated at runtime(if needed), used for one way communication between the user process and the deadlock detection process
- parse_protocol.txt: The layout of instructions used in the log_file
- deadlk_check_file.txt: Shows all deadlock checks made
- p_debug_log.txt: Debug file used to check whether the deadlock detection process reads correctly from the log_file
