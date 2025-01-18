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

// test with echo -n "$MESSAGE" | nc -u -w1 localhost 80 (set MESSAGE=whatever you want)

int main() {
    
    Server s("80");
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
        }
    }
}
