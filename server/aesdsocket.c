#include <sys/types.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <syslog.h>
#include <pthread.h>
#include <time.h>

#define LISTEN_PORT 9000
#define BUF_SIZE    1024
#define TIMESTAMP_INTERVAL 10   /* seconds between timestamp appends */

static const char *fileName = "/var/tmp/aesdsocketdata.txt";

/* Only set a flag from the handler. volatile sig_atomic_t is the only type
 * that is safe to touch from both a signal handler and normal code. */
static volatile sig_atomic_t exit_requested = 0;

static int server_socket = -1;

/* The data file is shared by every client thread, so all access to it is
 * serialized with this mutex. */
static pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

/* One node per spawned client thread. The main loop tracks them in a list so
 * it can join finished threads and free their resources. */
struct client_thread {
    pthread_t            tid;
    int                  client_socket;
    struct sockaddr_in   client_address;
    bool                 complete;        /* set by the thread just before it returns */
    SLIST_ENTRY(client_thread) entries;
};

SLIST_HEAD(thread_list, client_thread);

static void signal_handler(int signal_id)
{
    (void)signal_id;          /* SIGINT and SIGTERM are treated the same */
    exit_requested = 1;
}

/* Appends a "timestamp:<RFC 2822 time>\n" line to the shared file every
 * TIMESTAMP_INTERVAL seconds. The append is done under file_mutex so it never
 * interleaves with a client's socket-data write. Sleeps in 1s steps so a
 * shutdown signal is noticed promptly. */
static void *timestamp_thread(void *arg)
{
    (void)arg;

    while (!exit_requested) {
        for (int i = 0; i < TIMESTAMP_INTERVAL && !exit_requested; i++)
            sleep(1);
        if (exit_requested)
            break;

        char stamp[128];
        time_t now = time(NULL);
        struct tm tm_now;
        localtime_r(&now, &tm_now);

        /* RFC 2822 compliant: e.g. "Tue, 29 Jun 2026 14:05:09 +0000" */
        size_t len = strftime(stamp, sizeof(stamp),
                              "timestamp:%a, %d %b %Y %H:%M:%S %z\n", &tm_now);
        if (len == 0)
            continue;

        pthread_mutex_lock(&file_mutex);
        FILE *fp = fopen(fileName, "a");
        if (fp != NULL) {
            fwrite(stamp, 1, len, fp);
            fclose(fp);
            printf("File Open and write success for timestamp\n");
        } else {
            syslog(LOG_ERR, "timestamp fopen failed: %s", strerror(errno));
            printf("File Open error for timestamp\n");
        }
        pthread_mutex_unlock(&file_mutex);
    }

    return NULL;
}

/* each client worker. Reads newline-terminated packets, appends each to the
 * shared file and echoes the whole file back, until the client disconnects
 * or a signal is caught. Then marks itself complete so main() can join it. */
static void *handle_client(void *arg)
{
    struct client_thread *node = arg;
    int client_socket = node->client_socket;

    syslog(LOG_INFO, "Accepted connection from %s",
           inet_ntoa(node->client_address.sin_addr));

    /* A "packet" is newline-terminated and may be arbitrarily long, so we
     * accumulate bytes in a growable buffer until we see '\n'. */
    char chunk[BUF_SIZE];
    char *packet = NULL;          /* dynamically grown accumulator */
    size_t packet_len = 0;
    ssize_t readCount;

    while (!exit_requested &&
           (readCount = recv(client_socket, chunk, sizeof(chunk), 0)) > 0) {

        char *tmp = realloc(packet, packet_len + (size_t)readCount);
        if (tmp == NULL) {
            syslog(LOG_ERR, "realloc failed: %s", strerror(errno));
            break;
        }
        packet = tmp;
        memcpy(packet + packet_len, chunk, (size_t)readCount);
        packet_len += (size_t)readCount;

        /* Process every complete newline-terminated packet currently in
         * the buffer. One recv() may hold several, or none. */
        char *nl;
        while ((nl = memchr(packet, '\n', packet_len)) != NULL) {
            size_t line_len = (size_t)(nl - packet) + 1;  /* include '\n' */

            /* Append the packet and echo the whole file back under the lock,
             * so concurrent clients never interleave or race on the file. */
            pthread_mutex_lock(&file_mutex);

            FILE *fp = fopen(fileName, "a");
            if (fp == NULL) {
                syslog(LOG_ERR, "fopen failed: %s", strerror(errno));
                pthread_mutex_unlock(&file_mutex);
                break;
            }
            fwrite(packet, 1, line_len, fp);
            fclose(fp);

            /* Spec behaviour: send the ENTIRE file back, once. */
            fp = fopen(fileName, "r");
            if (fp == NULL) {
                syslog(LOG_ERR, "fopen(read) failed: %s", strerror(errno));
                pthread_mutex_unlock(&file_mutex);
                break;
            }
            size_t n;
            while ((n = fread(chunk, 1, sizeof(chunk), fp)) > 0) {
                ssize_t sent = 0;
                while (sent < (ssize_t)n) {
                    ssize_t s = send(client_socket, chunk + sent, n - sent, 0);
                    if (s < 0) break;
                    sent += s;
                }
            }
            fclose(fp);

            pthread_mutex_unlock(&file_mutex);

            /* Drop the consumed packet, keep any trailing partial line. */
            memmove(packet, nl + 1, packet_len - line_len);
            packet_len -= line_len;
        }
    }

    free(packet);
    syslog(LOG_INFO, "Closed connection from %s",
           inet_ntoa(node->client_address.sin_addr));
    close(client_socket);

    node->complete = true;
    return NULL;
}

int main(int argc, char **argv)
{
	//check system is invoked with deamon argument
	bool daemon_mode = (argc == 2 && strcmp(argv[1], "-d") == 0);

	//open syslog with LOG_USER Flag
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

	//create server_socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        syslog(LOG_ERR, "socket failed: %s", strerror(errno));
        return -1;
    }

	//set socket option to reuse
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family      = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port        = htons(LISTEN_PORT);

	//bind the server socket. 
    //in socket-test.sh just-killed previous instance can still hold the port for a moment 
    //which makes bind() fail with EADDRINUSE and the
	//process exit -1 (255). Retry for a few seconds
	//only give up on a persistent or non-EADDRINUSE error.
    int bind_rc = -1;
    for (int attempt = 0; attempt < 5 && !exit_requested; attempt++) {
        bind_rc = bind(server_socket, (struct sockaddr *)&server_address,
                       sizeof(server_address));
        if (bind_rc == 0 || errno != EADDRINUSE)
            break;
        syslog(LOG_WARNING, "bind: port %d in use, retrying (%d)",
               LISTEN_PORT, attempt);
        usleep((10*1000*1000));   /* Wait for sometime to release the port from previous kill */
    }

    if (bind_rc < 0) {
        syslog(LOG_ERR, "bind failed: %s", strerror(errno));
        close(server_socket);
        return -1;
    }

	//listen in server socker foer any client request
    if (listen(server_socket, 10) < 0) {
        syslog(LOG_ERR, "listen failed: %s", strerror(errno));
        close(server_socket);
        return -1;
    }

	// for the application after listen() succeeds:
    if (daemon_mode) {
        pid_t pid = fork();
        if (pid < 0) { syslog(LOG_ERR, "fork failed: %s", strerror(errno)); return -1; }
        if (pid > 0) return 0;           // parent returns -> shell continues
        setsid();
        chdir("/");
    }

    /* Track every spawned client thread so we can join finished ones and
     * free their nodes. */
    struct thread_list threads;
    SLIST_INIT(&threads);

    /* Start the background thread that appends a timestamp every 10s. */
    pthread_t timestamp_tid;
    bool timestamp_running =
        (pthread_create(&timestamp_tid, NULL, timestamp_thread, NULL) == 0);
    if (!timestamp_running)
        syslog(LOG_ERR, "timestamp pthread_create failed: %s", strerror(errno));

    // keep accepting clients until a signal is caught.
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

        /* Spawn a dedicated thread for this connection. */
        struct client_thread *node = malloc(sizeof(*node));
        if (node == NULL) {
            syslog(LOG_ERR, "malloc failed: %s", strerror(errno));
            close(client_socket);
            continue;
        }
        node->client_socket  = client_socket;
        node->client_address = client_address;
        node->complete       = false;

        if (pthread_create(&node->tid, NULL, handle_client, node) != 0) {
            syslog(LOG_ERR, "pthread_create failed: %s", strerror(errno));
            close(client_socket);
            free(node);
            continue;
        }
        SLIST_INSERT_HEAD(&threads, node, entries);

        /* Reap any threads that have already finished, releasing their
         * resources without blocking the accept loop. */
        struct client_thread *it = SLIST_FIRST(&threads);
        while (it != NULL) {
            struct client_thread *next = SLIST_NEXT(it, entries);
            if (it->complete) {
                pthread_join(it->tid, NULL);
                SLIST_REMOVE(&threads, it, client_thread, entries);
                free(it);
            }
            it = next;
        }
    }

    /* Graceful shutdown: join every outstanding client thread. shutdown()
     * unblocks any thread still parked in recv() so the join can complete. */
    struct client_thread *it;
    SLIST_FOREACH(it, &threads, entries)
        shutdown(it->client_socket, SHUT_RDWR);

    while (!SLIST_EMPTY(&threads)) {
        it = SLIST_FIRST(&threads);
        pthread_join(it->tid, NULL);
        SLIST_REMOVE_HEAD(&threads, entries);
        free(it);
    }

    /* Stop the timestamp thread (exit_requested is already set). */
    if (timestamp_running)
        pthread_join(timestamp_tid, NULL);

    /* Delete the data file at the last after receiving terminate signal. */
    syslog(LOG_INFO, "Caught signal, exiting");
    close(server_socket);
    remove(fileName);
    pthread_mutex_destroy(&file_mutex);
    closelog();
    return 0;
}
