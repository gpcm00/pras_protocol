#include <algorithm>
#include <cstring>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <mutex>
#include <condition_variable>

#include "protocol.hh"

#define __len__(arr) (sizeof(arr)/sizeof(arr[0]))

#define __malloc_struct__(s, z)          \
    (struct s*)malloc(z*sizeof(struct s))

#define RETURN_STATE_NAME(s)       \
    case s:  return #s       

using namespace std;

static char start_stream_msg[] {
    'P','O','W','E','R','M','O','D','U','L','E','_',
    'A','D','C','_','D','E','V','I','C','E','_',
    'S','T','R','E','A','M','_',
    'R','E','Q','U','E','S','T','_',
};

static const size_t request_size_offs = sizeof(start_stream_msg);
static const size_t request_size = request_size_offs + ID_SZ;

static void log_connection_open(struct sockaddr sa, unsigned duration) {
    char client_ip[INET_ADDRSTRLEN];
    struct sockaddr_in *client_addr = (struct sockaddr_in *)&sa;
    inet_ntop(AF_INET, &client_addr->sin_addr, client_ip, INET_ADDRSTRLEN);
    printf("Streaming to address %s for %d\n", client_ip, duration);
}

static bool cmp(char* buf) {
    for(size_t i = 0; i < request_size_offs; i++) {
        if(buf[i] != start_stream_msg[i])
            return false;
    }
    return true;
}

static void cpyid(char* dst, char* src) {
    src += request_size_offs;
    size_t l = ID_SZ;
    while(l--) *(dst + l) = *(src + l);
}

string statename(protocol_state_t s) {
    switch(s)
    {
    RETURN_STATE_NAME(error);
    RETURN_STATE_NAME(listening);
    RETURN_STATE_NAME(start_timeout);
    RETURN_STATE_NAME(streaming);
    RETURN_STATE_NAME(clean_timeout);
    default:
        return "wrong state";
    }
}

void init_timer(Protocol& p) {
    unique_lock<mutex> lock(p.mut);
    while(!p.timeout) {
        p.cv.wait_for(lock, chrono::minutes(p.duration));
        p.timeout = true;
    }
    p.cp.stop();    // release listen
}

Protocol::Protocol(string p, CP_Buffer& c) : port(p) , cp(c) {
    struct addrinfo *result, *rp;
		
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;
		
	int status = getaddrinfo(NULL, port.c_str(), &hints, &result);
	if(status != 0) {
		throw runtime_error("getaddrinfo: " + string(gai_strerror(status)));       
	}
	
	for(rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if(sfd == -1) {
			continue;
		}
		
		if(bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
			break;
		}
		
		close(sfd);
    }
	
	freeaddrinfo(result);
	
	if(rp == NULL) {  
		throw runtime_error("failed to bind: " + string(strerror(errno)));
	}

    state = listening;
    running = false;
    timeout = false;
    duration = 15;

	error_msg = "no error";
}

void Protocol::listen() {
    char inbuf[request_size];
    memset(inbuf, 0, sizeof(inbuf));

	struct sockaddr peer_addr;
	socklen_t peer_addrlen = sizeof(peer_addr);
	ssize_t nread = recvfrom(sfd, inbuf, sizeof(inbuf), 0, &peer_addr, &peer_addrlen);
	if(nread == -1) {
		error_msg = "recvfrom: " + string(strerror(errno));
        state = error;
		return;
	}

    if(!cmp(inbuf)) {
        error_msg = "Invalid request: " + string(inbuf);
        state = error;
		return;
    }

    cpyid(id, inbuf);
    peer = peer_addr;
}

void Protocol::stream() {
    socklen_t peer_addrlen = sizeof(peer);

    size_t len = cp.get(dp) * sizeof(struct datapoint);
    if(sendto(sfd, (void*)dp, len, 0, &peer, peer_addrlen) != len) {
		error_msg = "sendto " + string(strerror(errno));
        state = error;
	}
}

void Protocol::error_handler() {
    fprintf(stderr, "%s\n", error_msg.c_str());
    error_msg = "no error";
}

void Protocol::operator()() {
    if(running) {
        throw runtime_error("Cannot attach multiple protocols on the same port");
    }

    thread timer_thread;

    running = true;

    dp = __malloc_struct__(datapoint, cp.sz());
    if(dp == NULL) {
        throw runtime_error("malloc: " + string(strerror(errno)));
    }

    state = listening;

    while(running) {
        switch(state)
        {
        case listening: 
            listen();               // blocks until connection is received
            if(state != error) {
                state = start_timeout;
            }
            break;

        case start_timeout:
            timeout = false;
            timer_thread = thread(init_timer, ref(*this));
            state = streaming;

            log_connection_open(peer, duration);
            
            // no break needed

        case streaming:
            memset(dp, 0, cp.sz() * sizeof(struct datapoint));
            if(!timeout) {
                stream();
            } else {
                state = clean_timeout;
            }
            break;

        case clean_timeout:
            if(timer_thread.joinable()) {
                timer_thread.join();
            }
            cp.restart();
            state = listening;
            break;

        case error:
        default:
            error_handler();
            if(timer_thread.joinable()) {
                unique_lock<mutex> lock(mut);
                timeout = true;
                lock.unlock();
                cv.notify_all();
                timer_thread.join();
            }
            cp.restart();
            state = listening;
            break;
        }
    }

    free(dp);
}