#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <cstdio>

#include <algorithm>

#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <thread>

#include "server.hh"


#define BUFLEN		64
#define LEN(arr) 	(sizeof(arr)/sizeof(arr[0]))

using namespace std;



int main() {
    int flags = O_TRUNC | O_CREAT | O_RDWR;
    int mode = 0777;

    int fd1 = open("fd1.txt", flags, mode);
    int fd2 = open("fd2.txt", flags, mode);
    int fd3 = open("fd3.txt", flags, mode);
    int fd4 = open("fd4.txt", flags, mode);
    int fd5 = open("fd5.txt", flags, mode);

    Worker w;
    w.add_pipe("fd1", fd1);
    w.add_pipe("fd2", fd2);
    w.add_pipe("fd3", fd3);
    w.add_pipe("fd4", fd4);
    w.add_pipe("fd5", fd5);

    thread t[5];

    struct sockaddr sa;
    for(char i = 0; i < sizeof(sa); i++) *((char*)&sa+i) = 'a' + i;
    string msg = "\n\nFinal message\n";

    t[0] = thread(ref(w), sa, "fd1" + msg);
    t[1] = thread(ref(w), sa, "fd2" + msg);
    t[2] = thread(ref(w), sa, "fd3" + msg);
    t[3] = thread(ref(w), sa, "fd4" + msg);
    t[4] = thread(ref(w), sa, "fd5" + msg);

    for(int i = 0; i < 5; i++) t[i].join();
}
