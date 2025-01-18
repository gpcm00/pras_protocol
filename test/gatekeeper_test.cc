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

char msg[] = "Send this message back";

/*
int main() {

    Server s("80");
    int flags = O_TRUNC | O_CREAT | O_RDWR;
    int mode = 0777;

    // Gatekeeper gk(s);

    // thread t(gk, fd);

    while(true) {
        auto tup = s.receive();

        if(tup == nullptr) {
            printf("nullptr\n");
            continue;
        }
        
        struct sockaddr addr = get<0>(*tup);     
        socklen_t addrlen = sizeof(addr);
        char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

        int s = getnameinfo(&addr, addrlen, hbuf, sizeof(hbuf), sbuf,
                        sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);

        if (s == 0) {
            printf("host=%s, serv=%s\n", hbuf, sbuf);
            printf("Message\n\t%s\n", get<1>(*tup).c_str());
        } else {
            printf("getnameinfo: %s\n", gai_strerror(s));
            continue;
        }

        char txtmsg[sizeof(addr) + sizeof(msg)];
        for(int i = 0; i < sizeof(addr); i++) {
            txtmsg[i] = *((char*)&addr + i);
        }

        for(int i = 0; i < sizeof(msg); i++) {
            txtmsg[i + sizeof(addr)] = msg[i];
        }

        int fd = open("example.txt", flags, mode);

        write(fd, txtmsg, sizeof(addr) + sizeof(msg));

        close(fd);
    }
}

*/



int main() {
    Server s("80");
    int flags =  O_RDONLY;
    int mode = 0777;

    int fd = open("recv_pipe.txt", flags, mode);

    printf("open file\n");

    Gatekeeper gk(s);

    thread t(gk, fd);
    printf("sleeping\n");
    this_thread::sleep_for(std::chrono::seconds(5));

    printf("terminating\n");

    gk.terminate();

    return 0;
    
}