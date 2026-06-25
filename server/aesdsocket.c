#include <sys/types.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <syslog.h>

#define LISTEN_PORT 9000
#define BUF_SIZE    1024

static const char *fileName = "/var/tmp/aesdsocketdata.txt";

/* Only set a flag from the handler. volatile sig_atomic_t is the only type
 * that is safe to touch from both a signal handler and normal code. */
static volatile sig_atomic_t exit_requested = 0;

static int server_socket = -1;

static void signal_handler(int signal_id)
{
    (void)signal_id;          /* SIGINT and SIGTERM are treated the same */
    exit_requested = 1;
}

int main(void)
{
    openlog("aesdsocket", 0, LOG_USER);

    /* Register SIGINT and SIGTERM. No SA_RESTART so blocking calls return
     * EINTR when a signal arrives, letting us shut down promptly. */
    struct sigaction new_action;
    memset(&new_action, 0, sizeof(new_action));
    new_action.sa_handler = signal_handler;

    if (sigaction(SIGTERM, &new_action, NULL) != 0 ||
        sigaction(SIGINT,  &new_action, NULL) != 0) {
        syslog(LOG_ERR, "sigaction failed: %s", strerror(errno));
        return -1;
    }

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        syslog(LOG_ERR, "socket failed: %s", strerror(errno));
        return -1;
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family      = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port        = htons(LISTEN_PORT);

    if (bind(server_socket, (struct sockaddr *)&server_address,
             sizeof(server_address)) < 0) {
        syslog(LOG_ERR, "bind failed: %s", strerror(errno));
        close(server_socket);
        return -1;
    }

    if (listen(server_socket, 10) < 0) {
        syslog(LOG_ERR, "listen failed: %s", strerror(errno));
        close(server_socket);
        return -1;
    }

    /* Outer loop: keep accepting clients until a signal is caught. */
    while (!exit_requested) {
        struct sockaddr_in client_address;
        socklen_t client_len = sizeof(client_address);

        int client_socket = accept(server_socket,
                                   (struct sockaddr *)&client_address,
                                   &client_len);
        if (client_socket < 0) {
            if (errno == EINTR)      /* interrupted by signal -> shut down */
                break;
            continue;                /* transient error, keep serving */
        }

        syslog(LOG_INFO, "Accepted connection from %s",
               inet_ntoa(client_address.sin_addr));

        /* Inner loop: read packets from this client until it disconnects
         * or a signal is caught. */
        char buffer[BUF_SIZE];
        ssize_t readCount;
        while (!exit_requested &&
               (readCount = recv(client_socket, buffer, sizeof(buffer), 0)) > 0) {

            /* Append received bytes to the file. fwrite with readCount avoids
             * the need to null-terminate and handles embedded NULs. */
            FILE *fp = fopen(fileName, "a+");
            if (fp == NULL) {
                syslog(LOG_ERR, "fopen failed: %s", strerror(errno));
                break;
            }
            fwrite(buffer, 1, (size_t)readCount, fp);
            fclose(fp);

            /* Spec behaviour: after each packet, send the ENTIRE file back. */
            fp = fopen(fileName, "r");
            if (fp == NULL) {
                syslog(LOG_ERR, "fopen(read) failed: %s", strerror(errno));
                break;
            }
            size_t n;
            while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
                ssize_t sent = 0;
                while (sent < (ssize_t)n) {
                    ssize_t s = send(client_socket, buffer + sent, n - sent, 0);
                    if (s < 0) break;
                    sent += s;
                }
            }
            fclose(fp);
        }

        syslog(LOG_INFO, "Closed connection from %s",
               inet_ntoa(client_address.sin_addr));
        close(client_socket);
    }

    /* Graceful shutdown: only here do we delete the data file. */
    syslog(LOG_INFO, "Caught signal, exiting");
    close(server_socket);
    remove(fileName);
    closelog();
    return 0;
}
