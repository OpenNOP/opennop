#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <pthread.h> // for multi-threading
#include <unistd.h> // for sleep function
#include <ifaddrs.h> // for getting ip addresses
#include <netdb.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <arpa/inet.h> // for getting local ip address
#include <linux/types.h>

#include "opennopd.h"
#include "fetcher.h"
#include "logger.h"
#include "sessioncleanup.h"
#include "sessionmanager.h"
#include "help.h"
#include "signals.h"
#include "worker.h"
#include "healthagent.h"
#include "counters.h"
#include "memorymanager.h"
#include "climanager.h"
#include "compression.h"
#include "version.h"
#include "ipc.h"
#include "wccpv2.h"
#include "deduplication.h"

#define DAEMON_NAME "opennopd"
#define PID_FILE "/var/run/opennopd.pid"
#define LOOPBACKIP 16777343UL // Loopback IP address 127.0.0.1.
/* Global Variables. */
int servicestate = RUNNING; // Current state of the service.
static __u32 localID = 0; // Variable to store eth0 IP address used as the device ID.
int isdaemon = true; // Determines how to log the messages and errors.

int main(int argc, char *argv[]) {
    pthread_t t_healthagent; // thread for health agent.
    pthread_t t_cli; // thread for cli.
    pthread_t t_counters; // thread for performance counters.
    pthread_t t_memorymanager; // thread for the memory manager.
    struct ifaddrs *ifaddr, *ifa;
    __u32 tempIP = 0;
    int s;
    int i;
    char message[LOGSZ];
    char strIP[20];
    char host[NI_MAXHOST];

#if defined(DEBUG)

    int daemonize = false;
#else

    int daemonize = true;
#endif

    /* Setup signal handling */
    signal(SIGHUP, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);
    signal(SIGPIPE,SIG_IGN); // Ignore SIGPIPE on write errors.

    int c;
    while ((c = getopt(argc, argv, "nh|help")) != -1) {
        switch (c) {
        case 'h':
            PrintUsage(argc, argv);
            exit(0);
            break;
        case 'n':
            daemonize = 0;
            isdaemon = false;
            break;
        default:
            PrintUsage(argc, argv);
            break;
        }
    }

    sprintf(message, "Initialization: %s daemon starting up.\n", DAEMON_NAME);
    logger(LOG_INFO, message);

    sprintf(message, "Initialization: Version %s.\n", VERSION);
    logger(LOG_INFO, message);

    /*
     * Get the numerically highest local IP address.
     * This will be used as the acceleratorID.
     */
    if (getifaddrs(&ifaddr) == -1) {
        sprintf(message, "Initialization: Error opening interfaces.\n");
        logger(LOG_INFO, message);
        exit(EXIT_FAILURE);
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) { // loop through all interfaces.

        if (ifa->ifa_addr != NULL) {

            if (ifa->ifa_addr->sa_family == AF_INET) { // get all IPv4 addresses.
                s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                                host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);

                if (s != 0) {
                    exit(EXIT_FAILURE);
                }

                inet_pton(AF_INET, (char *) &host, &tempIP); // convert string to decimal.

                /*
                 * Lets fine the largest local IP, and use that as localID
                 * Lets also exclude 127.0.0.1 as a valid ID.
                 */
                if ((tempIP > localID) && (tempIP != LOOPBACKIP)) {
                    localID = tempIP;
                }
            } // end get all IPv4 addresses.
        } // end ifa->ifa_addr NULL test.
    } // end loop through all interfaces.

    if (localID == 0) { // fail if no usable IP found.
        inet_ntop(AF_INET, &tempIP, strIP, INET_ADDRSTRLEN);
        sprintf(message, "Initialization: No usable IP Address. %s\n", strIP);
        logger(LOG_INFO, message);
        exit(EXIT_FAILURE);
    }

#if defined(DEBUG)
    setlogmask(LOG-UPTO(LOG_DEBUG));
    openlog(DAEMON_NAME, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER);
#else

    setlogmask(LOG_UPTO(LOG_INFO));
    openlog(DAEMON_NAME, LOG_CONS, LOG_USER);
#endif

    /* Our process ID and Session ID */
    pid_t pid, sid;
    if (daemonize) {
        sprintf(message, "Initialization: Daemonizing the %s process.\n",
                DAEMON_NAME);
        logger(LOG_INFO, message);

        /* Fork off the parent process */
        pid = fork();
        if (pid < 0) {
            exit(EXIT_FAILURE);
        }

        /* If we got a good PID, then
         we can exit the parent process. */
        if (pid > 0) {
            exit(EXIT_SUCCESS);
        }

        /* Change the file mode mask */
        umask(0);

        /* Create a new SID for the child process */
        sid = setsid();
        if (sid < 0) {
            /* Log the failure */
            exit(EXIT_FAILURE);
        }

        /* Change the current working directory */
        if ((chdir("/")) < 0) {
            /* Log the failure */
            exit(EXIT_FAILURE);
        }

        /* Close out the standard file descriptors */
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }

    /*
     * Starting up the daemon.
     */

    initialize_sessiontable();
    init_deduplication();

    if (get_workers() == 0) {
        set_workers(sysconf(_SC_NPROCESSORS_ONLN) * 2);
    }

    for (i = 0; i < get_workers(); i++) {
        create_worker(i);
    }

    /*
     * Create the fetcher thread that retrieves
     * IP packets from the Netfilter Queue.
     */
    create_fetcher();
    start_dead_session_detection();
    pthread_create(&t_healthagent, NULL, healthagent_function, (void *) NULL);
    start_ipc();
    start_wccp();
    pthread_create(&t_cli, NULL, cli_manager_init, (void *) NULL);
    pthread_create(&t_counters, NULL, counters_function, (void *) NULL);
    pthread_create(&t_memorymanager, NULL, memorymanager_function,
                   (void *) NULL);


    sprintf(message, "[OpenNOP]: Started all threads.\n");
    logger(LOG_INFO, message);

    /*
     * All the threads have been created now commands should be registered.
     */
    register_command(NULL, "show version", cli_show_version, false, false);
    register_command(NULL, "show compression", cli_show_compression, false, false);
    register_command(NULL, "show workers", cli_show_workers, false, false);
    register_command(NULL, "show fetcher", cli_show_fetcher, false, false);
    register_command(NULL, "show sessions", cli_show_sessionss, false, false);
    register_command(NULL, "compression enable", cli_compression_enable, false, false);
    register_command(NULL, "compression disable", cli_compression_disable, false, false);

    /*
     * Rejoin all threads before we exit!
     */
    //pthread_join(t_fetcher, NULL);
    rejoin_fetcher();
    rejoin_dead_session_detection();
    pthread_join(t_healthagent, NULL);
    rejoin_ipc();
    stop_wccp();
    pthread_join(t_cli, NULL);
    pthread_join(t_counters, NULL);
    pthread_join(t_memorymanager, NULL);

    for (i = 0; i < get_workers(); i++) {
        rejoin_worker(i);
    }

    clear_sessiontable();

    sprintf(message, "Exiting: %s daemon exiting", DAEMON_NAME);
    logger(LOG_INFO, message);

    exit(EXIT_SUCCESS);
}
