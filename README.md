# UDP Echo

This is the documentation file describing the usage and internal design of the UDP Echo software tool. 

The tool is meant to mimic the functionality of "echo" between a client and a server, with slightly different command line options. 

## Usage

To generate an executable version of the tool, run "make" at the top level of the lab directory. 
This will place two executable files in the bin folder: myserver and myclient. 
You can run the tool from the top directory or in the bin directory. 
In the case that you are in the bin directory, the tool's usage would be as followed:

    1. Run your server with a desired port:
        ./myserver <serverPort>
    2. Identify server IP address (serverIp).
    3. Run your client against your server with a file of your choice:
        ./myclient <serverIp> <serverPort> <mss> <inputPath> <outputPath>


## Implementation details

Internally, both execeutables are programmed in C++.
The server is programmed to check the command line options, verifying their values.
The server then creates a socket and binds to the configured port.
At that point, the server begins to listen on the port for a client connection.
When it receives bytes, the server simply sends those bytes back to the client it received data from.
When the user inputs CTRL-C, the server will close the socket and exit gracefully.

The client is programmed to check the command line options, also verifying the input values.
The client then creates a socket and opens the input and output files.
It also creates a storage for out of order packets.

The client then uses the MSS input in order to determine how many bytes to pull from the input file.
The client then attaches a header to the file data and sends it to the socket.
The total size of the packet will be equal or less than MSS.
Afterwards, the client reads from the socket, waiting for the echo response.
Upon receiving the response, the client checks multiple things:
If the packet is out of order, then it will check the storage for the correctly ordered packet.
If it finds the correctly ordered packet, it will write that to the output file.
However, no matter what the client will store the out of order packet in the storage, which is a shortcoming.
Regardless, it will eventually pull the packet back out of storage.
If the packet is in order, it will simply write to output file.
This sequence continues until the entire input file is read.
In a perfect case, the client will reconstruct the input file perfectly in the output file.

## Test cases:

As for test cases, I tested using 5 cases:

### 1. General functionality:
    I ran the server and the client on the same host, testing the echo functionality with a random test file.

        ./client 127.0.0.1 9090 100 test.dat output.dat
        ./server 9090

### 2. Advanced functionality:
    I ran the client with a small MSS and large MSS for varying test file sizes.
        
        ./client 127.0.0.1 9090 10000 test.dat output.dat
        ./client 127.0.0.1 9090 10 test.dat output.dat

### 3. Advanced functionality 2:
    I ran the client when the server was down to test if it would time out.
    It would successfully time out after 60 seconds.

### 4. Improper command line inputs:
    - I did multiple tests for this:
        - Invalid port: ./server test
            - Response: Invalid port argument
        - File does not exist: ./client 127.0.0.1 8080 500 random.dat output.dat
            - Response: File does not exist: random.dat
        - Missing options: ./client 127.0.0.1 8080 500 test.dat
            - Response: Invalid number of command line arguments
        - Invalid options: ./client 127.0.0.1 8080 500 random.dat output.dat -random
            - Response: Invalid command line option: -random

### 5. Additional functionality tests:
    - I did tests with multiple files of varying sizes, including 0 up to 1GB.

Everything was also tested with valgrind to ensure that there was no memory loss.
