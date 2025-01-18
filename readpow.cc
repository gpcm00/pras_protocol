#include <cstdio>
#include <cerrno>
#include <cstring>

#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "protocol.hh"

using namespace std;

int main(int argc, char** argv) {
    if(argc != 3) {
        printf("usage: %s [PORT] [FILE PATH]\n", argv[0]);
    }

    char* port = argv[1];
	char* file = argv[2];

    int r = 0;
    int fd;

    struct timespec ts;

    struct datapoint dp;

    CP_Buffer buffer(5400);     // 30 minutes of samples

    Protocol consumer(string(port), buffer);

    thread network_protocol(ref(consumer));
    printf("Protocol initialized");

    while(true) {
        clock_gettime(CLOCK_MONOTONIC, &ts);

        fd = open(file, O_RDWR);
        if(fd == -1) {
            perror("open: ");
            exit(EXIT_FAILURE);
        }

        ssize_t n = 0;

        do {
            n = read(fd, &dp, sizeof(dp));
            if(n == -1) {
                perror("read: ");
                exit(EXIT_FAILURE);
            }

            if(n == sizeof(dp)) {
                buffer.put(dp);
            }

        } while(n != 0);

        close(fd);

        ts.tv_sec += 5;
        r = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);
        if(r != 0) {
            printf("nsleep: %s\n", strerror(r));
        }
    }

    return 0;
}