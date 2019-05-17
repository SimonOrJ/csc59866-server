HTTP Server Software
====================

CSC 59866: Group 1

This program was written for CSC 59866 class by group 1.


## How to compile

Make sure the machine has gcc installed.  Then run:

```
gcc server.cpp -lstdc++ -o server
```


## Usage

Usage: `./server [port=80] [root=html]`

To run the program with default configuration, run

```
./server
```

Make sure you have permission to execute the program.  The program will run
on port `8080` and the root directory will be `./html/`.

To run this on port 80, you must define the following parameters.

```
sudo ./server 80
```

You require superuser status to be able to bind to port 80 for security
reasons!

If you want to make the root directory different (to `./abcd/` for example),
you should define the second parameter:

```
sudo ./server 8080 abcd
```

**You must leave out the trailing slash** for defining directory for best
results.

Finally, to shut down the server, press `^C` (`[Ctrl] + [C]`).