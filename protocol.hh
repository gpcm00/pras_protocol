#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <sys/socket.h>

#include "cp_buffer.hh"

#define ID_SZ   10

typedef enum{
    error =-1, 
    listening, start_timeout, streaming, clean_timeout,
} protocol_state_t;

class Protocol {
	std::string port;
    struct sockaddr peer;
	int sfd;
    
	std::string error_msg;
    bool running;

    protocol_state_t state;

    bool timeout;
    unsigned duration;
    std::mutex mut;
    std::condition_variable cv;

    char id[ID_SZ];

    CP_Buffer& cp;
    struct datapoint* dp;

    void listen();
    void stream();
    void error_handler();

public:
	Protocol(std::string p, CP_Buffer& c);
    ~Protocol() {
        running = false;
        cp.stop();
    }	

    void operator()();

    void stop() { 
        running = false;
        cp.stop(); 
    };

    void change_duration(unsigned d) {
        duration = d;
    };

    protocol_state_t current_state() { 
        return state;
    };

    friend void init_timer(Protocol& p);
};

std::string statename(protocol_state_t s);