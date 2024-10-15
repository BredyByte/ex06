#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/select.h>

const int MAX_CLIENTS = 1024;
const int BUFFER_SIZE = 1024;


typedef struct s_client {
    int fd;
    int id;
} t_client;


t_client *clients;

int next_id = 0;

void exit_error(char *message) {
	if (message) {
		write(2, message, strlen(message));
	} else {
		write(2, "Fatal error", 11);
	}

	write(2, "\n", 1);
	exit(1);
}

void broadcast_message(int sender_fd, char *message, int exclude_fd) {
	(void)sender_fd; // ????
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd != -1 && clients[i].fd != exclude_fd) {
            send(clients[i].fd, message, strlen(message), 0);
        }
    }
}

void add_client(int listening) {
	int new_client;
	struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

	new_client = accept(listening, (struct sockaddr *)&client_addr, &client_len);
    if (new_client == -1) {
        exit_error(NULL);
    }

	int client_id = next_id++;

	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (clients[i].fd == -1) {
			clients[i].fd = new_client;
			clients[i].id = client_id;

			char buffer[BUFFER_SIZE];
			sprintf(buffer, "server: client %d just arrived\n", client_id);
			broadcast_message(new_client, buffer, -1);
			break;
		}
    }
}

void remove_client(int client_fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd == client_fd) {
            char buffer[BUFFER_SIZE];
            sprintf(buffer, "server: client %d just left\n", clients[i].id);
            broadcast_message(client_fd, buffer, -1);

            close(clients[i].fd);
            clients[i].fd = -1;
            break;
        }
    }
}


void handle_client_message(int client_fd) {
    char buffer[BUFFER_SIZE];
    int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

    if (bytes_received <= 0) {
        remove_client(client_fd);
        return;
    }

    buffer[bytes_received] = '\0';

    int client_id;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd == client_fd) {
            client_id = clients[i].id;
            break;
        }
    }

    char message[BUFFER_SIZE + 50];
    char *line = strtok(buffer, "\n");
    while (line) {
        sprintf(message, "client %d: %s\n", client_id, line);
        broadcast_message(client_fd, message, client_fd);
        line = strtok(NULL, "\n");
    }
}


int main(int argc, char** argv) {

	if (argc != 2) {
		exit_error("Wrong number of arguments");
	}

	clients = malloc(MAX_CLIENTS * sizeof(t_client));
	if (clients == NULL) {
		exit_error(NULL);
	}

	int listening = socket(PF_INET, SOCK_STREAM, 0);
	if (listening == -1) {
		exit_error(NULL);
	}

	struct sockaddr_in server_addr;

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(argv[1]));
	server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);;
	memset(&(server_addr.sin_zero), '\0', 8);


	if(bind(listening, (const struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
		exit_error(NULL);
	}

	if (listen(listening, SOMAXCONN) == -1) {
		exit_error(NULL);
	}

	for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1;
    }

	fd_set read_fds;
	FD_ZERO(&read_fds);
    int max_fd = listening;

	while(1) {
		FD_ZERO(&read_fds);
		FD_SET(listening, &read_fds);

		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (clients[i].fd != -1) {
				FD_SET(clients[i].fd, &read_fds);
				if (clients[i].fd > max_fd) {
					max_fd = clients[i].fd;
				}
			}
		}

		if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) == -1) {
			exit_error(NULL);
		}

		if (FD_ISSET(listening, &read_fds)) {
			add_client(listening);
		}

		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (clients[i].fd != -1 && FD_ISSET(clients[i].fd, &read_fds)) {
				handle_client_message(clients[i].fd);
			}
        }
	}

	return 0;
}
