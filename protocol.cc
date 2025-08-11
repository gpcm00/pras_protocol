#include <algorithm>
#include <cstring>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <mutex>
#include <condition_variable>

#include "crypto.hh"
#include "protocol.hh"


#define RETURN_STATE_NAME(s)       \
    case s:  return #s       

using namespace std;

constexpr size_t min(size_t a, size_t b) {
    return (a < b) ? a : b;
}

#define MIN_SZ(b1, b2)	min(sizeof(b1), sizeof(b2))

static uint16_t generate_random_port() {
    return 0xC000 + (std::rand() % 0x2000);
}

static inline bool no_data(SSL* ssl, ssize_t ret) {
	int err = SSL_get_error(ssl, ret);
	return (err == SSL_ERROR_WANT_READ);
}

static int open_socket(struct addrinfo hints, string port) {
    struct addrinfo *result, *rp;

    int sfd = -1;

    int status = getaddrinfo(NULL, port.c_str(), &hints, &result);
	if(status != 0) {
		throw runtime_error("getaddrinfo: " + string(gai_strerror(status)));       
	}

    for(rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if(sfd == -1) {
			continue;
		}

		int flags = fcntl(sfd, F_GETFL, 0);
		if (flags == -1) {
			perror("[!] fcntl: ");
			close(sfd);
			continue;
		}

		if (fcntl(sfd, F_SETFL, flags | O_NONBLOCK) == -1) {
			perror("[!] fcntl: ");
			close(sfd);
			continue;
		}
		
		if(bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
			break;
		}
		
		close(sfd);
    }
	
    freeaddrinfo(result);
	
	if(rp == NULL) {  
		throw runtime_error("Failed to bind: " + string(strerror(errno)));
	}

    return sfd;
}

static bool set_recv_timeout(int sockfd, unsigned minutes) {
    struct timeval tv;
    tv.tv_sec = minutes * 60;
    tv.tv_usec = 0;

    return (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0);
}


static inline bool reset_recv_timeout(int sockfd) {
	return set_recv_timeout(sockfd, 0);
}

static void convert_host_to_network_datapoint(struct datapoint* dp, size_t len) {
	for (size_t i = 0; i < len; i++) {
		dp->payload = htonl(dp->payload);
		dp->time = htonl(dp->time);
	}
}

static struct sockaddr change_port(struct sockaddr addr, uint16_t port) {
	sockaddr_in* temp = reinterpret_cast<sockaddr_in*>(&addr);
	temp->sin_port = htons(port);
	return addr;
}

static SSL_CTX* create_context(string ca, string cert, string key) {
    SSL_CTX* context = SSL_CTX_new(TLS_server_method());
    if (context == NULL) {
        ERR_print_errors_fp(stderr);
		return NULL;
    }

	SSL_CTX_set_verify(context, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
    if (SSL_CTX_load_verify_locations(context, ca.c_str(), NULL) <= 0) {
		goto Context_Error_Handler;
	}

    if (SSL_CTX_use_certificate_file(context, cert.c_str(), SSL_FILETYPE_PEM) <= 0) {
        goto Context_Error_Handler;
    }

    if (SSL_CTX_use_PrivateKey_file(context, key.c_str(), SSL_FILETYPE_PEM) <= 0) {
        goto Context_Error_Handler;
    }

	return context;

Context_Error_Handler:
	ERR_print_errors_fp(stderr);
	SSL_CTX_free(context);
	return NULL;
}

Server_Protocol::Server_Protocol(string p, string sp, Base_Buffer& c, Base_Action_Table& a, 
										string ca, string cert, string key, string pssw, unsigned d) 
: port(p) , cp(c), at(a), duration(d) {
    struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

    tcp_sfd = open_socket(hints, port);
    if (tcp_sfd == -1) {
        throw runtime_error("Failed to open TCP socket: " + string(strerror(errno)));
    }

	context = create_context(ca, cert, key);
	if (context == NULL) {
		throw runtime_error("Failed to create context");
	}

	if (listen(tcp_sfd, 1) == -1) {
		throw runtime_error("Failed to listen on server side: " + string(strerror(errno)));
	}

	set_password(pssw.c_str(), pssw.length());

	running = false;

	state = listening;
}

void Server_Protocol::listening_state() {
    char* buffer = reinterpret_cast<char*>(&message);
	const size_t bufferlen = sizeof(message);
    memset(buffer, 0, bufferlen);
	memset(ip_string, 0, sizeof(ip_string));

	struct sockaddr peer_addr;
	socklen_t peer_addrlen = sizeof(peer_addr);

	struct pollfd pfd = {
		.fd = tcp_sfd,
		.events = POLLIN, 
		.revents = 0
	};

	int ret = poll(&pfd, 1, -1);

	// if poll fails, just restart
	if (ret == -1) {
		perror("[!] poll");
		state = listening;
		return;
	} else if (!(pfd.revents & POLLIN)) {
		fprintf(stderr, "[-] Unexpected revent: 0x%X\n", pfd.revents);
		state = listening;
		return;
	}
	
	peer_sfd = accept(tcp_sfd, &peer_addr, &peer_addrlen);
	if (peer_sfd == -1) {
		// if interrupted by a signal, just restart
		if (errno == EINTR) {
			state = listening;
			return;
		}
		perror("[!] accept");
		state = listening;
		return;
	}
	
	ssl = SSL_new(context);
    SSL_set_fd(ssl, peer_sfd);

	size_t n = 0;

	if (SSL_accept(ssl) <= 0) {
		// failed handshake is not a fatal error
		ERR_print_errors_fp(stderr);
		state = listening;
		goto SSL_Cleanup_Handler;
	}

	do {
		ssize_t nread = SSL_read(ssl, buffer + n, bufferlen - n);
		if (nread <= 0) {
			// connection closed but no error
			if (nread == 0) {
				state = listening;
			// there was a serious error while reading
			} else {
				ERR_print_errors_fp(stderr);
				error_msg = "Unable to receive message from peer: " + string(strerror(errno));
				state = error;
			}
			goto SSL_Cleanup_Handler;
		}
		
		n += nread;
	} while (n < bufferlen);
	
	message.port = ntohs(message.port);

	// ignore messages to weird ports and without the password
	if (message.port < 0xC000 || !search_password()) {
		state = listening;
		goto SSL_Cleanup_Handler;	
	}

	cp.restart();

    peer = peer_addr;
	if (peer.sa_family == AF_INET){
		struct sockaddr_in *addr_in = (struct sockaddr_in*)&peer;
		inet_ntop(AF_INET, &(addr_in->sin_addr), ip_string, sizeof(ip_string));
	} else {
		struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)&peer;
        inet_ntop(AF_INET6, &(ipv6->sin6_addr), ip_string, sizeof(ip_string));
	}

	printf("[+] Received a successful request from %s\n", ip_string);

	state = (state == error)? error : streaming;

	return;

SSL_Cleanup_Handler:
	SSL_shutdown(ssl);
    SSL_free(ssl);
    close(peer_sfd);
	return;
}

ssize_t Server_Protocol::send_key() {
	size_t n = 0;

	if (RAND_bytes(aes_key, sizeof(aes_key)) != 1) {
        return -1;
	}

	do {
		ssize_t nwrite = SSL_write(ssl, aes_key + n, sizeof(aes_key) - n);
		if (nwrite <= 0) {
			if (nwrite == -1) {
				ERR_print_errors_fp(stderr);
			}
			return nwrite;
		}
		n += nwrite;
	} while (n < sizeof(aes_key));

	return n;
}

bool Server_Protocol::wait_heartbeat() {
	size_t n = 0;
	struct command_struct cmd;
	char* buffer = reinterpret_cast<char*>(&cmd);
	size_t bufferlen = sizeof(cmd);
	memset(buffer, 0, bufferlen);

	struct pollfd pfd = {
		.fd = peer_sfd,
		.events = POLLIN, 
		.revents = 0
	};

	int ret = poll(&pfd, 1, duration * 60000);
	if (ret == -1) {
		perror("[!] poll");
		return false;
	} else if (ret == 0) {
		printf("[+] No heartbeat from %s\n", ip_string);
		return false;
	} else if (!(pfd.revents & POLLIN)) {
		fprintf(stderr, "[!] Unexpected revent (0x%X) from %s\n", pfd.revents, ip_string);
		return false;
	}

	do {
		ssize_t nread = SSL_read(ssl, buffer + n, bufferlen - n);
		if (nread <= 0) {
			// this means there was an actual error
			if (nread < 0 && !no_data(ssl, nread)) {
				ERR_print_errors_fp(stderr);
			}

			return false;
		}
		n += nread;
	} while (n < bufferlen);

	if (!at.call(cmd.command, cmd.payload, n)) {
		fprintf(stderr, "[!] Invalid command: 0x%X from %s\n", cmd.command, ip_string);
		return false;
	}
	return true;
}

void Server_Protocol::stream_data() {
	size_t dp_len = cp.size();

	// using RAII is annoying here
	size_t len = dp_len * sizeof(datapoint);
	struct aes_gcm_payload* cifer_dp = (aes_gcm_payload*)malloc(len + sizeof(aes_gcm_payload));
	struct datapoint* dp = (datapoint*)malloc(len);

	uint8_t* dp_buffer = reinterpret_cast<uint8_t*>(dp);
	uint8_t* cifer_dp_buffer = reinterpret_cast<uint8_t*>(cifer_dp);
	struct sockaddr client_addr = change_port(peer, message.port);

	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	stream_port = std::to_string(generate_random_port());
	udp_sfd = open_socket(hints, stream_port);


	while (stream) {
		memset(dp, 0, len);
		memset(cifer_dp->payload, 0, len);
		len = cp.get(dp);
		if (!(len > 0)) {
			continue;
		}
		convert_host_to_network_datapoint(dp, len);
		len = aes_gcm_encrypt(dp_buffer, len * sizeof(datapoint), aes_key, cifer_dp);
		// throw away message if encryption failed
		if (len == -1) {
			len = dp_len * sizeof(datapoint);
			continue;
		}

		size_t total_len = len + sizeof(aes_gcm_payload);
		size_t n = 0;
		do {
			ssize_t nwrite = sendto(udp_sfd, cifer_dp_buffer + n, total_len - n, 0,
														 &client_addr, sizeof(client_addr));
			// on error, just throw away the message
			if (nwrite == -1) {
				len = dp_len * sizeof(datapoint);	// to clear dp and cifer_dp
				break;
			}
			n += nwrite;
		} while (n < total_len);
	}

	free(dp);
	free(cifer_dp);
	close(udp_sfd);
}

void Server_Protocol::streaming_state() {
	stream = true;
	thread streaming_thread = thread(&Server_Protocol::stream_data, this);
	do {
		ssize_t n = send_key();
		if (n <= 0) {
			if (n == -1) {
				state = error;
				error_msg = "Error sending key to peer: " + string(strerror(errno));
			}
			stream = false;
			break;
		}

		stream = wait_heartbeat();
	} while (stream);

	cp.stop();
	if (streaming_thread.joinable()) {
		streaming_thread.join();
	}

	printf("[+] Stopped streaming to %s\n", ip_string);

	SSL_shutdown(ssl);
    SSL_free(ssl);
    close(peer_sfd);

	state = (state == error)? error : listening;
}

void Server_Protocol::run_state_machine() {
	while (running) {
		switch (state) {
			case listening:
				listening_state();
				break;
			case streaming:
				streaming_state();
				break;
			case error:
				error_handler();
				break;
			default:
				throw runtime_error("Unknown state");
		}
	}
}

optional<thread> Server_Protocol::start() {
	if (!running) {
		running = true;
		return thread(&Server_Protocol::run_state_machine, this);
		// run_state_machine();
	}
	return nullopt;
}

void Server_Protocol::error_handler() {
	// TODO: improve error handling logic

	fprintf(stderr, "[-] %s\n", error_msg.c_str());
	state = listening;
}

bool Server_Protocol::search_password() {
	for (size_t i = 0; i <= sizeof(message.payload) - password.size; i++) {
		if (memcmp(&message.payload[i], password.sequence, password.size) == 0) {
			return true;
		}
	}
	return false;
}

bool Server_Protocol::set_password(const char* key, size_t keylen) {
	// bool done = false;
	if (keylen > sizeof(password.sequence)) {
		return false;
	}

	memcpy(password.sequence, key, keylen);
	memset(password.sequence + keylen, 0, sizeof(password.sequence) - keylen);
	
	password.size = keylen;

	return true;
}

