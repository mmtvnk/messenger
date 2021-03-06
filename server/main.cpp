#include <arpa/inet.h>
#include <errno.h>
#include <iostream>
#include <netdb.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <stdint-gcc.h>
#include <unistd.h>

#include "../shared_params.hpp"

using namespace std;

//TODO: uncomment argc, argv to use input parameters
int main(/*int argc, char *argv[]*/)
{
    //TODO: add usage output section

    addrinfo addrinfo_reqs = {
        .ai_flags = 0,
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP,
        .ai_addrlen = 0,
        .ai_addr = nullptr,
        .ai_canonname = nullptr,
        .ai_next = nullptr
    }; //address_info_requirements

    addrinfo *address_info;

    int getaddrinfo_status = getaddrinfo(NULL,
                                         SERVER_PORT,
                                         &addrinfo_reqs,
                                         &address_info);

    if (getaddrinfo_status != 0) {
        cerr << "getaddrinfo() error: " << gai_strerror(getaddrinfo_status) << endl;
        return getaddrinfo_status;
    }

    int listen_socket = socket(address_info->ai_family,
                               address_info->ai_socktype,
                               address_info->ai_protocol);

    if (listen_socket == -1) {
        cerr << "socket() error: " << strerror(errno) << endl;
        return errno;
    }

    //set SO_REUSEADDR and SO_REUSEPORT flags on a listen_socket to "true" value (1)
    int listen_socket_flag = (int)true;

    if (setsockopt(listen_socket,
                   SOL_SOCKET,
                   SO_REUSEADDR | SO_REUSEPORT,
                   &listen_socket_flag,
                   sizeof(listen_socket_flag)) == -1) {
        cerr << "setsockopt() error: " << strerror(errno) << endl;
        return errno;
    }

    if (bind(listen_socket,
             address_info->ai_addr,
             address_info->ai_addrlen) == -1) {
        close(listen_socket);
        freeaddrinfo(address_info);
        cerr << "bind() error: " << strerror(errno) << endl;
        return errno;
    }

    if (listen(listen_socket, MAX_CLIENTS) == -1) {
        cerr << "listen() error: " << strerror(errno) << endl;
        return errno;
    }

    char server_ip_address[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET,
                  &((sockaddr_in *)(address_info->ai_addr))->sin_addr,
                  server_ip_address,
                  INET_ADDRSTRLEN) == NULL) {
        cerr << "inet_ntop() error: " <<strerror(errno) << endl;
        return errno;
    }

    cout << "server " << server_ip_address << " started on port " << SERVER_PORT << "." << endl;

    int pollsd_count = 0; // polling_socket_descriptors_count
    pollfd pollsd_set[MAX_CLIENTS] = {0}; // polling_socket_descriptors_set
    pollsd_set[0].fd = listen_socket;
    pollsd_set[0].events = POLLIN;
    pollsd_count++;

    for(;;) {
        int poll_retval = poll(pollsd_set, pollsd_count, INFTIM);

        if (poll_retval == -1) {
            cerr << "poll() error: " << strerror(errno) << endl;
            continue;
            //TODO: return?
            //return errno;
        }
        else if (poll_retval != 0) {

            for (int i = 0; i < pollsd_count; i++) {

                if (pollsd_set[i].revents & POLLIN) {

                    if (pollsd_set[i].fd == listen_socket) {

                        int client_socket = accept(listen_socket,
                                                   address_info->ai_addr,
                                                   &address_info->ai_addrlen);

                        if (client_socket == -1) {
                            cerr << "accept() error: " << strerror(errno) << endl;
                            continue;
                        }
                        else {
                            pollsd_set[pollsd_count].fd = client_socket;
                            pollsd_set[pollsd_count].events = POLLIN;
                            pollsd_count++;

                            char client_ip_address[INET_ADDRSTRLEN];
                            if (inet_ntop(AF_INET,
                                          &((sockaddr_in *)(address_info->ai_addr))->sin_addr,
                                          client_ip_address,
                                          INET_ADDRSTRLEN) == NULL) {
                                cerr << "inet_ntop() error: " << strerror(errno) << endl;
                                cout << "server accepted unknown (due to inet_ntop() error) client successfully." << endl;
                            }
                            // TODO: output a message informing that client is connected
                            else {
                                cout << "server accepted client from " << client_ip_address << " successfully." << endl;
                            }
                        }
                    }
                    else {
                        char message[MAX_MESSAGE_LENGTH];
                        int bytes_recieved = recv(pollsd_set[i].fd, message, MAX_MESSAGE_LENGTH * sizeof(char), 0);
                        switch(bytes_recieved) {

                        case 0:
                            //Somebody disconnected , get his details and print
                            close(pollsd_set[i].fd);
                            pollsd_set[i] = pollsd_set[(pollsd_count - 1)];
                            pollsd_count--;
                            //TODO: print ip addres of disconnected socket.
                            continue;
                        case -1:
                            cerr << "recv() error: " << strerror(errno) << endl;
                            continue;
                        default:
                            for (int j = 0; j < pollsd_count; j++) {
                                if (pollsd_set[j].fd != listen_socket && pollsd_set[j].fd != pollsd_set[i].fd) {
                                    if (send(pollsd_set[j].fd, message, MAX_MESSAGE_LENGTH * sizeof(char), 0) == -1) {
                                        cerr << "send() error: " << strerror(errno) << endl;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return 0;
}
