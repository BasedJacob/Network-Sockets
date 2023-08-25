# cmpt-300-stalk
If you see this and you are not an instructor, TA, or group member, you are committing AD!

## Usage
* `make` to compile
* `make clean` to delete executables and compilation artifacts
* `s-talk HOST_PORT REMOTE_NAME REMOTE_PORT`: note that REMOTE_NAME can be an IPv4 address or a hostname

## Technical challenges
The server thread (listener) and terminal input thread will block while waiting for input because we are using the blocking syscall read(). We use pthread_cancel() to interrupt the terminal input thread - this may result in a minor memory leak when our s-talk application is profiled using `valgrind`. If we had more time and knew about non-blocking I/O earlier we would have used an approach like the answer to [this question](https://stackoverflow.com/questions/5591780/unblocking-a-thread-from-another-thread). Otherwise, calling `sleep()` for a few seconds before shutdown helps but does not guarantee that our implementation will be free from memory leaks.

## Credits
* Jacob He
* Oliver Xie