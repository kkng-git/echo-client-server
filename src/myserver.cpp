#include <iostream>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <csignal>
#include <time.h>

using namespace std;

const int BUF_SIZE = 32000;

// Gracefully handling interrupt
volatile sig_atomic_t interrupted = 0;

void signal_handler(int signal) {
    interrupted = 1;
}

int main(int argc, char* argv[]){
    // Check argument count
    if(argc != 2){
        cerr << "Invalid number of command line arguments." << endl;
        exit(-1);
    }

    // Verify port number
    int port = -1;
    try{
        port = strtol(argv[1], nullptr, 10);
    }
    catch(const exception& e){
        cerr << "Exception: " << e.what() << endl;
        exit(-1);
    }
    if(port <= 0){
        cerr << "Invalid port argument" << endl;
        exit(-1);
    }

    // Create a UDP socket
    int sockfd;
    struct sockaddr_in receive;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd < 0){
        cerr << "Error creating socket" << endl;
        exit(-1);
    }

    // Set receive timeout
    struct timeval timeout;
    timeout = (struct timeval){0};
    timeout.tv_sec = 18000;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        cerr << "Unable to set a timeout on socket" << endl;
        close(sockfd);
        exit(-1);
    }

    // Set up receiving address
    bzero(&receive, sizeof(receive));
    receive.sin_family = AF_INET;
    receive.sin_port = htons(port);
    receive.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind to socket
    if(::bind(sockfd, (struct sockaddr *)&receive, sizeof(receive)) < 0){
        cerr << "Error binding socket" << endl;
        close(sockfd);
        exit(-1);
    }

    // Store client details
    struct sockaddr_in client;

    // Set up data reception
    int bytesRead;
    socklen_t len;
    char buf[BUF_SIZE];
    
    // Handle interrupt
    signal(SIGINT, signal_handler);
    
    // packetStruct* received = new packetStruct;

    while(!interrupted){
        // Receive
        len = sizeof(client);
        bytesRead = recvfrom(sockfd, buf, BUF_SIZE, 0, (struct sockaddr *)&client, &len);

        if(bytesRead < 0){
            break;
        }

        sendto(sockfd, buf, bytesRead, 0, (struct sockaddr *)&client, len);
    }

    cout << "Closing server.." << endl;

    close(sockfd);

    exit(0);
}