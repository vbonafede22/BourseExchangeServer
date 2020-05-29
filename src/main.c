#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>

#include "client_registry.h"
#include "exchange.h"
#include "trader.h"
#include "debug.h"
#include "my_header.h"
#include "server.h"

extern EXCHANGE *exchange;
extern CLIENT_REGISTRY *client_registry;

static void terminate(int status);

/*
 * "Bourse" exchange server.
 *
 * Usage: bourse <port>
 */
int main(int argc, char* argv[]){
    // Option processing should be performed here.
    // Option '-p <port>' is required in order to specify the port number
    // on which the server should listen.

    // Perform required initializations of the client_registry,
    // maze, and player modules.
    client_registry = creg_init();
    exchange = exchange_init();
    trader_init();

    // TODO: Set up the server socket and enter a loop to accept connections
    // on this socket.  For each connection, a thread should be started to
    // run function brs_client_service().  In addition, you should install
    // a SIGHUP handler, so that receipt of SIGHUP will perform a clean
    // shutdown of the server.

    if(argc != 3){
        debug("Arguments -p <port number> is required");
        terminate(EXIT_FAILURE);
    }

    if(strcmp(argv[1], "-p") != 0){
        debug("Invalid Arguments given");
        terminate(EXIT_FAILURE);
    }

    //port from command line 
    int port = atoi(argv[2]);
    if(port < 1 || port > 65535){
        debug("Invalid port number");
        terminate(EXIT_FAILURE);
    } else {
        //set up signal handling 
        struct sigaction action;

        memset(&action, 0, sizeof(action));

        //sigaction structure 
        action.sa_handler = sig_handler;
        action.sa_flags = 0;

        if(sigaction(SIGHUP, &action, NULL) == -1){
            debug("Error in signal handler");
            terminate(EXIT_FAILURE);
        }

        //create a socket
        int server_fd;
        struct sockaddr_in socket_addr;
        if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
            debug("Error in socket");
            terminate(EXIT_FAILURE);
        }

        //internet style socket address structure from textbook
        socket_addr.sin_family = AF_INET;
        socket_addr.sin_addr.s_addr = INADDR_ANY;
        socket_addr.sin_port = htons(port);

        //socket is bound to a local address 
        if(bind(server_fd, (struct sockaddr *)&socket_addr, sizeof(socket_addr)) == -1){
            debug("Server bind error");
            terminate(EXIT_FAILURE);
        }

        //accept incoming connections, listen for connections on a given socket
        if(listen(server_fd, FD_SETSIZE - 4) == -1){     //FD_SETSIZE is number of connections 
            debug("Listen error");
            terminate(EXIT_FAILURE);
        }
        debug("Listening on port %d", port);

        //start a thread for each connection
        int client_fd;
        struct sockaddr_in client_addr;
        while(1){
            int struct_size = sizeof(struct sockaddr_in);
            client_fd = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&struct_size);
            pthread_t thread_id;
            int* client_fd_ptr = malloc(sizeof(int));           //needs to be freed at some point
            *client_fd_ptr = client_fd;
            pthread_create(&thread_id, NULL, brs_client_service, client_fd_ptr);   
        }
    }

    terminate(EXIT_FAILURE);
}

//sighup handler, same as terminate???
void sig_handler(int sig){
    creg_shutdown_all(client_registry);
    
    debug("Waiting for service threads to terminate...");
    creg_wait_for_empty(client_registry);
    debug("All service threads terminated.");

    // Finalize modules.
    creg_fini(client_registry);
    exchange_fini(exchange);
    trader_fini();

    debug("Bourse server terminating");
    exit(EXIT_SUCCESS);
}

/*
 * Function called to cleanly shut down the server.
 */
static void terminate(int status) {
    // Shutdown all client connections.
    // This will trigger the eventual termination of service threads.
    creg_shutdown_all(client_registry);
    
    debug("Waiting for service threads to terminate...");
    creg_wait_for_empty(client_registry);
    debug("All service threads terminated.");

    // Finalize modules.
    creg_fini(client_registry);
    exchange_fini(exchange);
    trader_fini();

    debug("Bourse server terminating");
    exit(status);
}
