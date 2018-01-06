
// http://www.geeksforgeeks.org/socket-programming-in-cc-handling-multiple-clients-on-server-without-multi-threading/

#include "tcpsocket.h"
#include "datastorage.h"
#include "ubus.h"

//Example code: A simple server side code, which echos back the received message.
//Handle multiple socket connections with select and fd_set on Linux
#include <stdio.h>
#include <string.h>   //strlen
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>   //close
#include <arpa/inet.h>    //close
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros

#define TRUE   1
#define FALSE  0

int tcp_array_insert(struct network_con_s entry);

int tcp_array_contains_address_help(struct sockaddr_in entry);

void print_tcp_entry(struct network_con_s entry);

int tcp_entry_last = -1;

void *run_tcp_socket(void *arg)
{
    int opt = TRUE;
    int master_socket, addrlen, new_socket, client_socket[30],
            max_clients = 30, activity, i, valread, sd;
    int max_sd;
    struct sockaddr_in address;

    char buffer[1025];  //data buffer of 1K

    //set of socket descriptors
    fd_set readfds;

    char *message = "ECHO Daemon v1.0 \r\n";

    //initialise all client_socket[] to 0 so not checked
    for (i = 0; i < max_clients; i++) {
        client_socket[i] = 0;
    }

    //create a master socket
    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    //set master socket to allow multiple connections ,
    //this is just a good habit, it will work without this
    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *) &opt,
                   sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    //type of socket created
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(network_config.tcp_port);

    //bind the socket to localhost port 8888
    if (bind(master_socket, (struct sockaddr *) &address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Listener on port %d \n", network_config.tcp_port);

    //try to specify maximum of 3 pending connections for the master socket
    if (listen(master_socket, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    //accept the incoming connection
    addrlen = sizeof(address);
    puts("Waiting for connections ...");

    while (TRUE) {
        //clear the socket set
        FD_ZERO(&readfds);

        //add master socket to set
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;

        //add child sockets to set
        for (i = 0; i < max_clients; i++) {
            //socket descriptor
            sd = client_socket[i];

            //if valid socket descriptor then add to read list
            if (sd > 0)
                FD_SET(sd, &readfds);

            //highest file descriptor number, need it for the select function
            if (sd > max_sd)
                max_sd = sd;
        }

        //wait for an activity on one of the sockets , timeout is NULL ,
        //so wait indefinitely
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR)) {
            printf("select error");
        }

        //If something happened on the master socket ,
        //then its an incoming connection
        if (FD_ISSET(master_socket, &readfds)) {
            if ((new_socket = accept(master_socket,
                                     (struct sockaddr *) &address, (socklen_t *) &addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            //inform user of socket number - used in send and receive commands
            printf("New connection , socket fd is %d , ip is : %s , port : %d\n", new_socket,
                   inet_ntoa(address.sin_addr), ntohs
                   (address.sin_port));

            //send new connection greeting message
            if (send(new_socket, message, strlen(message), 0) != strlen(message)) {
                perror("send");
            }

            puts("Welcome message sent successfully");

            //add new socket to array of sockets
            for (i = 0; i < max_clients; i++) {
                //if position is empty
                if (client_socket[i] == 0) {
                    client_socket[i] = new_socket;
                    printf("Adding to list of sockets as %d\n", i);

                    break;
                }
            }
        }

        //else its some IO operation on some other socket
        for (i = 0; i < max_clients; i++) {
            sd = client_socket[i];

            if (FD_ISSET(sd, &readfds)) {
                //Check if it was for closing , and also read the
                //incoming message
                if ((valread = read(sd, buffer, 1024)) == 0) {
                    //Somebody disconnected , get his details and print
                    getpeername(sd, (struct sockaddr *) &address, \
                        (socklen_t *) &addrlen);
                    printf("Host disconnected , ip %s , port %d \n",
                           inet_ntoa(address.sin_addr), ntohs(address.sin_port));

                    //Close the socket and mark as 0 in list for reuse
                    close(sd);
                    client_socket[i] = 0;
                }

                    //Echo back the message that came in
                else {
                    //set the string terminating NULL byte on the end
                    //of the data read
                    buffer[valread] = '\0';
                    //send(sd, buffer, strlen(buffer), 0);
                    printf("RECEIVED: %s\n", buffer);
                    handle_network_msg(buffer);
                }
            }
        }
    }
}

int add_tcp_conncection(char* ipv4, int port){
    int sockfd;
    struct sockaddr_in serv_addr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        fprintf(stderr,"ERROR opening socket");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ipv4);
    serv_addr.sin_port = htons(port);

    print_tcp_array();

    if(tcp_array_contains_address(serv_addr)) {
        close(sockfd);
        return 0;
    }


    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    {
        close(sockfd);
        fprintf(stderr,"ERROR connecting\n");
        return 0;
    }

    struct network_con_s tmp =
            {
                    .sock_addr = serv_addr,
                    .sockfd = sockfd
            };

    insert_to_tcp_array(tmp);

    printf("NEW TCP CONNECTION!!! to %s:%d\n", ipv4, port);

    return 0;
}

int insert_to_tcp_array(struct network_con_s entry) {
    pthread_mutex_lock(&tcp_array_mutex);

    int ret = tcp_array_insert(entry);
    pthread_mutex_unlock(&tcp_array_mutex);

    return ret;
}

void print_tcp_entry(struct network_con_s entry)
{
    printf("Conenctin to Port: %d\n", entry.sock_addr.sin_port);
}

void send_tcp(char* msg)
{
    printf("SENDING TCP!\n");
    pthread_mutex_lock(&tcp_array_mutex);
    for (int i = 0; i <= tcp_entry_last; i++) {
        if(send(network_array[i].sockfd, msg, strlen(msg), 0) < 0)
        {
            close(network_array->sockfd);
            printf("Removing bad TCP connection!\n");
            for (int j = i; j < tcp_entry_last; j++) {
                network_array[j] = network_array[j + 1];
            }

            if (tcp_entry_last > -1) {
                tcp_entry_last--;
            }
        }
    }
    pthread_mutex_unlock(&tcp_array_mutex);
}


void print_tcp_array()
{
    printf("--------Connections------\n");
    for (int i = 0; i <= tcp_entry_last; i++) {
        print_tcp_entry(network_array[i]);
    }
    printf("------------------\n");
}

int tcp_array_insert(struct network_con_s entry) {
    if (tcp_entry_last == -1) {
        network_array[0] = entry;
        tcp_entry_last++;
        return 1;
    }

    int i;
    for (i = 0; i <= tcp_entry_last; i++) {
        if (entry.sock_addr.sin_addr.s_addr < network_array[i].sock_addr.sin_addr.s_addr) {
            break;
        }
        if (entry.sock_addr.sin_addr.s_addr == network_array[i].sock_addr.sin_addr.s_addr) {
            return 0;
        }
    }
    for (int j = tcp_entry_last; j >= i; j--) {
        if (j + 1 <= ARRAY_NETWORK_LEN) {
            network_array[j + 1] = network_array[j];
        }
    }
    network_array[i] = entry;

    if (tcp_entry_last < ARRAY_NETWORK_LEN) {
        tcp_entry_last++;
    }
    return 1;
}

int tcp_array_contains_address(struct sockaddr_in entry) {
    pthread_mutex_lock(&tcp_array_mutex);

    int ret = tcp_array_contains_address_help(entry);
    pthread_mutex_unlock(&tcp_array_mutex);

    return ret;
}

int tcp_array_contains_address_help(struct sockaddr_in entry) {
    if (tcp_entry_last == -1) {
        return 0;
    }

    int i;
    for (i = 0; i <= tcp_entry_last; i++) {
        if (entry.sin_addr.s_addr == network_array[i].sock_addr.sin_addr.s_addr) {
            return 1;
        }
    }
    return 0;
}