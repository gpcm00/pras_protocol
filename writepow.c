#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>


char message[] = {
	'S', 'E', 'T', '_', 'P', 'O', 'W', 'E', 'R', '_', '-', '_',
};

bool decode_message(char* buff) {
	unsigned i = sizeof(message);
	while(i--) {
		if(message[i] != buff[i]) 
			return false;
	}
	return true;
}

int main(int argc, char** argv) {
	if(argc != 3) {
		printf("usage: %s [PORT] [FILE PATH]\n", argv[0]);
	}

	char* port = argv[1];
	char* file = argv[2];

	struct addrinfo *result, *rp;
	int sfd, res;

	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	int status = getaddrinfo(NULL, port, &hints, &result);
	if(status != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));   
		exit(EXIT_FAILURE)   ;
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
		perror("bind: ");
		exit(EXIT_FAILURE);
	}

	res = listen(sfd, 1);
	if(res == 1) {
		perror("listen: ");
		exit(EXIT_FAILURE);
	}

	while(true) {
		char buffer[sizeof(message) + 1];
		struct sockaddr peer_addr;
		socklen_t peer_addr_len = sizeof(struct sockaddr_storage);
        int sockfd = accept(sfd, &peer_addr, &peer_addr_len);

		char client_ip[INET_ADDRSTRLEN];
		struct sockaddr_in *client_addr = (struct sockaddr_in *)&peer_addr;

		inet_ntop(AF_INET, &client_addr->sin_addr, client_ip, INET_ADDRSTRLEN);

		printf("Connection from open %s\n", client_ip);

		size_t n = 0;
		ssize_t rd = 0;
		do{
			rd = recv(sockfd, buffer + n, sizeof(buffer) - n, 0);

			if(rd == -1) {
				perror("recv: ");
				exit(EXIT_FAILURE);
			}

			n += rd;
		} while((n < sizeof(buffer)) && rd != 0);


		if(decode_message(buffer)) {
			int fd = open(file, O_RDWR);
			if(fd == -1) {
				perror("open: ");
				exit(EXIT_FAILURE);
			}

			char val = buffer[sizeof(message)];
			if(write(fd, &val, 1) == -1) {
				perror("write: ");
				exit(EXIT_FAILURE);
			}

			printf("Set power to 0x%X\n", val);
			close(fd);
		}
		printf("Connection from %s closed - %ld bytes received\n", client_ip, n);
		
		close(sockfd);
	}
	exit(EXIT_SUCCESS);
}

