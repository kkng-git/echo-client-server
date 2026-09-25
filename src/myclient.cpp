#include <iostream>
#include <sys/stat.h>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>
#include <cstring>
#include <unistd.h>

using namespace std;

const int BUF_SIZE = 32000;

int main(int argc, char* argv[]){
    // Check argument count
    if(argc != 6){
        cerr << "Invalid number of command line arguments." << endl;
        exit(-1);
    }

    // Verify MSS value
    int mss = strtol(argv[3], nullptr, 10);
    int mtu = -1;
    if(mss < 5){
        cerr << "Required minimum MSS is 4 + 1. Exiting..." << endl;
        exit(-1);
    }
    else{
        mtu = mss - sizeof(uint32_t);
    }

    // Verify file integrity
    char* infile = argv[4];
    char* outfile = argv[5];

    // Check infile integrity
    struct stat sb;
    if(::stat(infile, &sb) == 0){
        // Check if input is directory
        if (S_ISDIR(sb.st_mode)){
            cerr << "Infile input is directory. Please provide a File Path. Exiting..." << endl;
            exit(-1);
        }
    }
    else{
        cerr << "File does not exist: " << infile << endl;
        exit(-1);
    }

    // Check outfile integrity
    if(::stat(outfile, &sb) == 0){
        // Check if output is directory
        if (S_ISDIR(sb.st_mode)){
            cerr << "Outfile input is directory. Please provide a File Path. Exiting..." << endl;
            exit(-1);
        }
    }
    else{
        // File does not exist, creating
        ofstream file(outfile);
        file.close();
    }

    // Verify port
    int port = -1;
    try{
        port = strtol(argv[2], nullptr, 10);
        // cout << port << endl;
    }
    catch(const exception& e){
        cerr << "Exception: " << e.what() << endl;
        exit(-1);
    }
    if(port == 0){
        cerr << "Invalid port argument" << endl;
        exit(-1);
    }

    // Verify IP Address
    struct sockaddr_in server;

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    if(inet_pton(AF_INET, argv[1], &server.sin_addr) <= 0) {
        cerr << "Invalid IP address / Address not supported" << endl;
        exit(-1);
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0){
        cerr << "Error in creating socket" << endl;
        exit(-1);
    }

    // Set receive timeout
    struct timeval timeout;
    timeout = (struct timeval){0};
    timeout.tv_sec = 60;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        cerr << "Unable to set a receiving timeout on socket" << endl;
        close(sock);
        exit(-1);
    }

    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        cerr << "Unable to set a sending timeout on socket" << endl;
        close(sock);
        exit(-1);
    }

    // int bytesReceived

    // Open infile
    FILE *in = fopen(infile, "r");
    if(in == NULL){
        cerr << "Unable to open infile: " << infile << endl;
        close(sock);
        exit(-1);
    }
    // Open outfile
    FILE *out = fopen(outfile, "w");
    if(out == NULL){
        cerr << "Unable to open outfile: " << outfile << endl;
        fclose(in);
        close(sock);
        exit(-1);
    }

    // Loop
    errno = 0;

    // Storing MSS number of bytes
    char sendline[mss];
    int bytesRead = -1;
    uint32_t* counter = new uint32_t(0);
    int fileRead = -1;

    // Out of Order packet handling
    char* storage[10];
    // initialize
    for(int i=0; i<10; i++){
        storage[i] = nullptr;
    }
    int max = 9;
    uint32_t mostRecent = 0;

    // Read MTU number of bytes from file
    while((fileRead = fread(sendline + sizeof(uint32_t), 1, mtu, in)) > 0){
        // Sending
        // cout << "File Read bytes: " << fileRead << endl;
        
        // Delim
        if(fileRead < mtu){
            sendline[fileRead+sizeof(uint32_t)] = '\0';
        }

        //Add header
        memcpy(sendline, counter, sizeof(uint32_t));

        // Sending
        sendto(sock, sendline, fileRead+sizeof(uint32_t), 0, (struct sockaddr *)&server, sizeof(server));

        // Reset sendline
        memset(sendline, 0, mss);

        // Receive
        char recvline[mss+1];
        uint32_t header;
        bytesRead = recvfrom(sock, recvline, mss+1, 0, NULL, NULL);
        // cout << "Received bytes: " << bytesRead << endl;
        if(bytesRead == -1){
            cerr << "Cannot detect server." << endl;
            delete counter;
            fclose(in);
            fclose(out);
            close(sock);
            exit(-1);
        }
        // Delim
        recvline[bytesRead] = '\0';

        // Check header
        memcpy(&header, &recvline, sizeof(uint32_t));
        // cout << header << endl;
        // If header value does not match most recent sent packet, then store
        if(header != *counter){
            uint32_t headercheck;
            // Check if storage contains in order packet
            for(int i=0; i<10; i++){
                if(storage[i] != nullptr){
                    memcpy(&headercheck, storage[i], sizeof(uint32_t));
                    if(headercheck == mostRecent+1){
                        // Write packet
                        fputs(storage[i]+sizeof(uint32_t), out);
                        mostRecent = mostRecent + 1;
                        free(storage[i]);
                        storage[i] = nullptr;
                        break;
                    }
                }
            }
            // Store out of order packet for now
            // If Full, drop
            bool stored = false;
            for(int i=0; i<10; i++){
                if(storage[i] == nullptr){
                    char* temp = (char*) malloc(sizeof(char) * (bytesRead));
                    memcpy(temp, &recvline, bytesRead);
                    storage[i] = temp;
                    stored=true;
                    break;
                }
            }
            // If packet dropped, then its lost
            if(!stored){
                cerr << "Packet loss detected." << endl;
                for(int i=0; i<10; i++){
                    if(storage[i] != nullptr){
                        free(storage[i]);
                    }
                }
                delete counter;
                fclose(in);
                fclose(out);
                close(sock);
                exit(2);
            }
        }
        else{
            fputs(recvline+sizeof(uint32_t), out);

            mostRecent = mostRecent + 1;
        }
        *counter = *counter + 1;
    }
    // cout << "Sending complete" << endl;
    
    // Close
    bool loss = false;
    for(int i=0; i<10; i++){
        if(storage[i] != nullptr){
            //if theres anything in storage, then we missed a packet
            free(storage[i]);
            loss = true;
        }
    }
    delete counter;
    fclose(in);
    fclose(out);
    close(sock);
    if(loss){
        cerr << "Packet loss detected." << endl;
        exit(2);
    }
    exit(0);
}