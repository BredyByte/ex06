#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>

fd_set read_fds, write_fds, all_fds;
int client_count = 0, max_fd = 0;
int ids[65536];
char *msgs[65536];
char intro_msg[42];
char read_buffer[1001];

void exit_error(const char *message) {
	if (message) {
		write(2, message, strlen(message));
	} else {
		write(2, "Fatal error", 11);
	}

	write(2, "\n", 1);
	exit(1);
}

void free_and_close_all() {
    for (int fd = 0; fd <= max_fd; fd++) {
        if (FD_ISSET(fd, &all_fds)) {
            free(msgs[fd]);
            close(fd);
        }
    }
    exit_error(NULL);
}

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				free_and_close_all();
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		free_and_close_all();
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}


void broadcast_msg(int sender_fd, const char *msg) {
    for (int fd = 0; fd <= max_fd; fd++) {
        if (FD_ISSET(fd, &write_fds) && fd != sender_fd) {
            send(fd, msg, strlen(msg), 0);
        }
    }
}

void send_msg(int fd) {
    char *msg;
    while (extract_message(&msgs[fd], &msg)) {
        bzero(intro_msg, 42);
        sprintf(intro_msg, "client %d: ", ids[fd]);
        broadcast_msg(fd, intro_msg);
        broadcast_msg(fd, msg);
        free(msg);
    }
}

void add_client(int sockfd) {
    struct sockaddr_in cli;
    int connfd;
    socklen_t len = sizeof(cli);
    connfd = accept(sockfd, (struct sockaddr *)&cli, &len);
    if (connfd >= 0) {
        if (connfd > max_fd) max_fd = connfd;
        ids[connfd] = client_count++;
        msgs[connfd] = NULL;
        FD_SET(connfd, &all_fds);
        bzero(intro_msg, 42);
        sprintf(intro_msg, "server: client %d just arrived\n", ids[connfd]);
        broadcast_msg(connfd, intro_msg);
    }
}

void recv_message(int fd) {
    bzero(read_buffer, 1001);
    int read_bytes = recv(fd, read_buffer, sizeof(read_buffer) - 1, 0);
    if (read_bytes <= 0) {
        bzero(intro_msg, 42);
        sprintf(intro_msg, "server: client %d just left\n", ids[fd]);
        broadcast_msg(fd, intro_msg);
        free(msgs[fd]);
        FD_CLR(fd, &all_fds);
        close(fd);
    } else {
        read_buffer[read_bytes] = '\0';
        msgs[fd] = str_join(msgs[fd], read_buffer);
        send_msg(fd);
    }
}

int main(int argc, char **argv) {
    int sockfd;
    struct sockaddr_in servaddr;

    if (argc != 2) {
        exit_error("Wrong number of arguments");
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        exit_error(NULL);
    }

    max_fd = sockfd;
    FD_SET(sockfd, &all_fds);
    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(2130706433);
    servaddr.sin_port = htons(atoi(argv[1]));

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) {
        exit_error(NULL);
    }

    if (listen(sockfd, 10) != 0) {
        exit_error(NULL);
    }

    while (1) {
        read_fds = write_fds = all_fds;

        if (select(max_fd + 1, &read_fds, &write_fds, 0, 0) < 0) continue;

        for (int fd = 0; fd <= max_fd; fd++) {
            if (!FD_ISSET(fd, &read_fds)) continue;

            if (fd == sockfd) {
                add_client(sockfd);
            } else {
                recv_message(fd);
            }
        }
    }

    return 0;
}
