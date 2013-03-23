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
#include "messages.h"
#include "eventmanager.h"
#include "counters.h"

#define DAEMON_NAME "opennopd"
#define PID_FILE "/var/run/opennopd.pid"
#define LOOPBACKIP 16777343UL // Loopback IP address 127.0.0.1.

/* Global Variables. */
int servicestate = RUNNING; // Current state of the service.
__u32 localIP = 0; // Variable to store eth0 IP address used as the device ID.
int isdaemon = true; // Determines how to log the messages and errors.

int main(int argc, char *argv[])
{
    pthread_t t_fetcher; // thread for getting packets out of Netfilter Queue.
    pthread_t t_cleanup; // thread for cleaning up dead sessions.
    pthread_t t_healthagent; // thread for health agent.
    pthread_t t_messages; // thread for messages.
    pthread_t t_eventmanager; // thread for eventmanager.
    pthread_t t_counters; // thread for performance counters.
    struct ifaddrs *ifaddr, *ifa;
    __u32 tempIP;
    int s;
    int i;
    char message [LOGSZ];
    char strIP [20];
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

    int c;
    while ( (c = getopt(argc, argv, "nh|help")) != -1)
    {
        switch(c)
        {
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

    /*
     * Get the numerically highest local IP address.
     * This will be used as the acceleratorID.
     */
    if (getifaddrs(&ifaddr) == -1)
    {
        sprintf(message, "Initialization: Error opening interfaces.\n");
        logger(LOG_INFO, message);
        exit(EXIT_FAILURE);
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {  // loop through all interfaces.

        if (ifa->ifa_addr->sa_family == AF_INET)
        { // get all IPv4 addresses.
            s = getnameinfo(ifa->ifa_addr,
                            sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);

            if (s != 0)
            {
                exit(EXIT_FAILURE);
            }

            inet_pton(AF_INET,(char *)&host, &tempIP);  // convert string to decimal.

            /*
             * Lets fine the largest local IP, and use that as accelleratorID
             * Lets also exclude 127.0.0.1 as a valid ID. 
             */
            if ((tempIP > localIP) && (tempIP != LOOPBACKIP))
            {
                localIP = tempIP;
            }
        } // end get all IPv4 addresses.
    } // end loop through all interfaces.

    if (localIP == 0)
    { // fail if no usable IP found.
        inet_ntop(AF_INET, &tempIP, strIP, INET_ADDRSTRLEN);
        sprintf(message, "Initialization: No usable IP Address. %s\n",strIP);
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
    if (daemonize)
    {
        sprintf(message, "Initialization: Daemonizing the %s process.\n", DAEMON_NAME);
        logger(LOG_INFO, message);

        /* Fork off the parent process */
        pid = fork();
        if (pid < 0)
        {
            exit(EXIT_FAILURE);
        }

        /* If we got a good PID, then
          	   we can exit the parent process. */
        if (pid > 0)
        {
            exit(EXIT_SUCCESS);
        }

        /* Change the file mode mask */
        umask(0);

        /* Create a new SID for the child process */
        sid = setsid();
        if (sid < 0)
        {
            /* Log the failure */
            exit(EXIT_FAILURE);
        }

        /* Change the current working directory */
        if ((chdir("/")) < 0)
        {
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

    for (i = 0; i < SESSIONBUCKETS; i++)
    { // Initialize all the slots in the hashtable to NULL.
        sessiontable[i].next = NULL;
        sessiontable[i].prev = NULL;
    }

    if (numworkers == 0)
    {
        numworkers = sysconf(_SC_NPROCESSORS_ONLN); // Get the number of logical CPUs.
        numworkers = numworkers * 2;
    }

    for (i = 0; i < numworkers; i++)
    {
        pthread_create(&workers[i].optimization.t_processor, NULL, optimization_function, (void *)&workers[i]);
        pthread_create(&workers[i].deoptimization.t_processor, NULL, deoptimization_function, (void *)&workers[i]);
        pthread_cond_init(&workers[i].optimization.queue.signal, NULL); // Initialize the thread signal.
        pthread_cond_init(&workers[i].deoptimization.queue.signal, NULL); // Initialize the thread signal.
        workers[i].optimization.queue.next = NULL; // Initialize the queue.
        workers[i].optimization.queue.prev = NULL;
        workers[i].optimization.lzbuffer = NULL;
        workers[i].optimization.queue.qlen = 0;
        pthread_mutex_init(&workers[i].optimization.queue.lock, NULL); // Initialize the queue lock.
        workers[i].deoptimization.queue.next = NULL; // Initialize the queue.
        workers[i].deoptimization.queue.prev = NULL;
        workers[i].deoptimization.lzbuffer = NULL;
        workers[i].deoptimization.queue.qlen = 0;
        pthread_mutex_init(&workers[i].deoptimization.queue.lock, NULL); // Initialize the queue lock.
        //workers[i].lzfastbuffer = NULL;
        workers[i].sessions = 0;


        workers[i].state = RUNNING;
        pthread_mutex_init(&workers[i].lock, NULL); // Initialize the worker lock.
    }

    /*
     * Create the fetcher thread that retrieves
     * IP packets from the Netfilter Queue.
     */
    pthread_create(&t_fetcher, NULL, fetcher_function, (void *)NULL);
    pthread_create(&t_cleanup, NULL, cleanup_function, (void *)NULL);
    pthread_create(&t_healthagent, NULL, healthagent_function, (void *)NULL);
    pthread_create(&t_messages, NULL, messages_function, (void *)NULL);
    pthread_create(&t_eventmanager, NULL, eventmanager_function, (void *)NULL);
    pthread_create(&t_counters, NULL, counters_function, (void *)NULL);

    /*
     * Rejoin all threads before we exit!
     */
    pthread_join(t_fetcher, NULL);
    pthread_join(t_cleanup, NULL);
    pthread_join(t_healthagent, NULL);
    pthread_join(t_messages, NULL);
    pthread_join(t_eventmanager, NULL);
    pthread_join(t_counters, NULL);

    for (i = 0; i < numworkers; i++)
    {

        workers[i].state = STOPPED;
        pthread_mutex_lock(&workers[i].optimization.queue.lock);
        pthread_cond_signal(&workers[i].optimization.queue.signal);
        pthread_mutex_unlock(&workers[i].optimization.queue.lock);
        pthread_join(workers[i].optimization.t_processor, NULL);
        pthread_mutex_lock(&workers[i].deoptimization.queue.lock);
        pthread_cond_signal(&workers[i].deoptimization.queue.signal);
        pthread_mutex_unlock(&workers[i].deoptimization.queue.lock);
        pthread_join(workers[i].deoptimization.t_processor, NULL);
    }

    for (i = 0; i < SESSIONBUCKETS; i++)
    { // Initialize all the slots in the hashtable to NULL.
        if (sessiontable[i].next != NULL)
        {
            freemem(&sessiontable[i]);
            sprintf(message, "Exiting: Freeing sessiontable %d!\n",i);
            logger(LOG_INFO, message);
        }

    }

    sprintf(message, "Exiting: %s daemon exiting", DAEMON_NAME);
    logger(LOG_INFO, message);

    exit(EXIT_SUCCESS);
}
