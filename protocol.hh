#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <optional>
#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <openssl/ssl.h>


#include <sys/socket.h>

#include "cp_buffer.hh"
#include "action_table.hh"

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif

#define SSSP_PACKET_SIZE 32

typedef enum{
    error = -1, 
    listening, streaming, restart,
} protocol_state_t;

struct packet_format {
    uint16_t port;        
    uint8_t payload[SSSP_PACKET_SIZE-sizeof(port)];
};

struct command_struct {
	uint8_t command;
	uint8_t payload[SSSP_PACKET_SIZE-sizeof(command)];
};

class Server_Protocol {
    
	std::string port;
    std::string stream_port;
    struct sockaddr peer;
	int udp_sfd, tcp_sfd, peer_sfd;
    unsigned duration;

    char ip_string[INET6_ADDRSTRLEN];
    
	std::string error_msg;
    bool running;

    protocol_state_t state;

    SSL_CTX* context;
    SSL* ssl;
    bool stream;
    
    struct packet_format message;

    struct {
        char sequence[sizeof(message.payload)];
        size_t size;
    } password;

    Base_Buffer& cp;
    struct datapoint* dp;

    Base_Action_Table& at;

    uint8_t aes_key[32];

    ssize_t send_key();
    bool wait_heartbeat();
    void stream_data();

    void run_state_machine();

    void listening_state();
    void streaming_state();
    void error_handler();

    bool search_password();


public:
	Server_Protocol(
        std::string p,              // port
        std::string sp,             // stream port
        Base_Buffer& c,             // data buffer
        Base_Action_Table& a,       // action table
        std::string ca,             // certificate authority
        std::string cert,           // certificate
        std::string key,            // key
        std::string pssw,           // password
        unsigned d                  // duration
    );
    
    ~Server_Protocol() {
        running = false;
        SSL_CTX_free(context);

        cp.stop();
    }	

    void operator()();

    std::optional<std::thread> start();
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

    std::string current_state_name() {
        switch (state)
        {
        case listening:
            return std::string("listening");
        
        case streaming:
            return std::string("streaming");
        
        default:
            return std::string("invalid");
        }
    }

    void get_packet(packet_format* out) {
        memcpy(out, &message, sizeof(message));
    }

    bool set_password(const char* key, size_t keylen);

    bool is_running() {return running; }
};

// class Client_Protocol {
//     std::string port;
//     std::string stream_port;
// }

#endif