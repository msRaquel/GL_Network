/***************************************
peer.cpp
Author: Raquel Zamudio
Date Completed: 06/15/2026
Description: Peer script that will compute latency for a small network built on the Chico State
campus. The destination should be able to read the incoming TTL. 
***************************************/
#include <cstdio>   // For std::popen, std::pclose, std::fgets
#include <iostream> // For std::cout 
#include <cstring>  // For std::strstr
#include <cstdlib>  // For std::atoi
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <dirent.h>
#include <arpa/inet.h> // htonl/ntohl
#include <fcntl.h> // For open() flags
#include <sys/stat.h>
#include <netinet/in.h> 
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

#define MAX_BUFFER_SIZE 1200
#define STARTING_TTL 64


/* DATA STRUCTURES FOR DATA COLLECTION */

// signal attached to each peer (node)
// ipv4 values range from 0 - 255 for header fields, so we use unsigned 8 bit variable to replicate the header field
struct Signal{
    uint8_t ttl = 0;
    float RTT_ms = -1.0f;
};

// keep track of peer information
struct Peer{
    Signal signal;
    uint32_t peerID = 0;
    char pair_id[16] = ""; 
    char ip_string[INET_ADDRSTRLEN] = "";
    char location[32] = "";
};

/* NETWORK SOCKET CONNECTION SECTION */

// Courtesy sample code from "Computer Networks: A Systems Approach," 5th Edition by Larry L. Peterson and Bruce S. Davis
// modified by Dr. Kredo
int lookup_and_connect( const char *host, const char *service );

// send data to connected socket (registry in this case)
int sendall(int s, char *buf, int *len)
{
	int total = 0;
	int bytesleft = *len;
	int n;
	
	while(total < *len)
	{
		n = send(s, buf + total, bytesleft, 0);
		if ( n == -1) { break; }
		total += n;
		bytesleft-= n;
	}
	*len = total;
	return n==-1?-1:0;
}

/* PING (SIGNAL) EXTRACTION SECTION */



// goal of this function is to extract values returned by the ping command 
// buffer will be provided as an argument with the statistics provided by ping
// once values have been extracted initialize a peer struct with the values found
int extractPingStats(struct Peer &peer, char *buff){
    // search for location in the buffer for the ttl and min statistics
    const char *hopLoc = strstr(buff, "ttl=");
    const char *rttLoc = strstr(buff, "min/avg/max/mdev");

    // scan the each location and copy the ttl and minimum values to the peer struct
    if (hopLoc != nullptr){
        sscanf(hopLoc, "ttl=%hhd", &peer.signal.ttl);
    }

    if (rttLoc != nullptr){
        const char *minRttLoc = strchr(rttLoc, '=');

        if (minRttLoc != nullptr){
            sscanf(minRttLoc + 1, "%f", &peer.signal.RTT_ms);
        }
      
    }

        std::cout << "\n>>> [Parsed Data] <<<" << std::endl;
        std::cout << "Latency measured at " << peer.signal.RTT_ms << " ms" << std::endl;
        std::cout << "TTL measured at " << (int)peer.signal.ttl << std::endl;
        std::cout << "---------------------\n" << std::endl;


    return 1;
}

/* check if ping execution was succesful */
bool executePing(const char *PING_BASE, char *buff, struct Peer &peer){

    char pingCall[64];
    int status;
    
    // initialize 
    size_t baseLen = std::strlen(PING_BASE);
    size_t ipLen = std::strlen(peer.ip_string);
    size_t totalLen = baseLen + ipLen;

    // Copy the base ping command to the start of the buffer
    std::memcpy(pingCall, PING_BASE, baseLen);

    // Copy the host address directly
    std::memcpy(pingCall + baseLen, peer.ip_string, ipLen);

    pingCall[totalLen] = '\0';

    /* send ping command and receive response to calculate latency and time-to-live (TTL) */
    std::cout << "Executing: " << pingCall << "\n";

    FILE* pipe = popen(pingCall, "r");

    // error handling 
    if (pipe == NULL){
        perror("error opening pipe");
        exit( 1 );
    }
    while (fgets(buff, MAX_BUFFER_SIZE, pipe) != NULL){
        printf("%s", buff);
        if ((extractPingStats(peer, buff) == -1))
        {
            return false;
        }
    } 
    status = pclose(pipe);
    if (status == -1) {
        /* Error reported by pclose() */
        perror("error when closing pipe");
    }

    std::cout << "Latency measured for this peer " << peer.signal.RTT_ms << " ms" << std::endl;
    std::cout << "Hops measured for this peer " << (STARTING_TTL - (int)peer.signal.ttl) << " hops" << std::endl; 


    return true;
};

int main(int argc, char *argv[])
{
    const char *hosts_file;
    const char *host;
    const char *port;
    int peerID = 0;
    int s;
    const char *PING_BASE = "ping -c 4 ";
    char buff[MAX_BUFFER_SIZE];
    // Ensure a hosts file was provided via command line argument
   
	if (argc == 5) //takes in requirements to run this file
	{
        //Gather and save input data for use
        hosts_file = argv[1];
       	host = argv[2];
        port = argv[3];
        peerID = atoi(argv[4]);

    
	}
    else
	{
       	fprintf( stderr, "usage error: %s host\n", argv[0] );
       	exit( 1 );
   	}

    // vector of peers
    std::vector<Peer> network_peers;

    // begin extracting data from hosts file to create peers to later collect latency measures from
    std::ifstream file(hosts_file);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open host file: " << hosts_file << "\n";
        return 1;
    }

    uint32_t current_id = 0;

   std::string line;
    // Parse Comma-Separated Hosts File
    while (std::getline(file, line)) {
        // Skip empty lines or header comments
        if (line.empty() || line[0] == '#') continue;

        std::stringstream ss(line);
        std::string id_str, ip_str, loc_str;

        // Extract comma-separated tokens
        if (std::getline(ss, id_str, ',') &&
            std::getline(ss, ip_str, ',') &&
            std::getline(ss, loc_str, ',')) {
            
            // Helper function logic to clean up leading/trailing whitespaces from parsing
            auto trim = [](std::string& s) {
                s.erase(0, s.find_first_not_of(" \t\r\n"));
                s.erase(s.find_last_not_of(" \t\r\n") + 1);
            };
            trim(id_str);
            trim(ip_str);
            trim(loc_str);

            Peer target_peer;
            target_peer.peerID = static_cast<uint32_t>(std::atoi(id_str.c_str()));
            
            // Safely copy IP and Location into char 
            std::strncpy(target_peer.ip_string, ip_str.c_str(), INET_ADDRSTRLEN - 1);
            target_peer.ip_string[INET_ADDRSTRLEN - 1] = '\0';

            std::strncpy(target_peer.location, loc_str.c_str(), sizeof(target_peer.location) - 1);
            target_peer.location[sizeof(target_peer.location) - 1] = '\0';

            // Initialize signals
            target_peer.signal.RTT_ms = -1.0f;
            target_peer.signal.ttl = 0;

            network_peers.push_back(target_peer);
        }
    }
    file.close();

    std::cout << "Successfully initialized " << network_peers.size() << " target peers.\n";
    std::cout << "=========================================\n";

    // loop through peer and collect peer 1-2, 1-3, 1-4, 1-5,1-6
    // if this computer is #1 then ping 2-26 while also providing the latency of all other nodes
    for(size_t i = 0; i < network_peers.size(); ++i){

        snprintf(network_peers.pair_id, sizeof(network_peers.pair_id), "%u-%u", peerID, network_peers[i].);


        // starting with peer 1, ping 1-2,3,4,5
        std::cout << "[Peer: "<< peerID << " Pinging " << network_peers[i].peerID << "]\n";
        

        bool success = executePing(PING_BASE, buff, network_peers[i]);
        
        if (!success) {
            std::cerr << "Warning: Failed execution to " << network_peers[i].ip_string << "\n";
        }

        std::cout << "=========================================\n";

        // through each ping, ping all the peers providing the latency for all from the initial ping
        // this is meant for the graph learning algorithm to have sufficient signals 
        for (size_t j = 0; j < network_peers.size(); ++j) {
            std::cout << "[Target " << j + 1 << "/" << network_peers.size() 
                << "] Probing Location: " << network_peers[j].location 
                << " (ID: " << network_peers[j].peerID << ")\n";
        
            bool success = executePing(PING_BASE, buff, network_peers[j]);
        
            if (!success) {
                std::cerr << "Warning: Failed execution to " << network_peers[j].ip_string << "\n";
            }
            std::cout << "=========================================\n";
        }

        if ( ( s = lookup_and_connect( host, port ) ) < 0 ) {
		    exit( 1 );
	    } 

        // we want to send measurements once at a time

    
        // Calculate exactly how many total bytes your vector is holding

        int id_bytes = sizeof(peerID);
        int array_size = network_peers.size() * sizeof(Peer);
        int total_bytes = array_size + id_bytes;

        std::vector<char> tempBuff(total_bytes);

        // memcpy the peerId and the data processed
        std::memcpy(tempBuff.data(), &peerID, id_bytes);
        std::memcpy(tempBuff.data() + id_bytes, network_peers.data(), array_size);

        // send it all to registry
        if((sendall(s, tempBuff.data(), &total_bytes)) == -1){
            perror("Error in sending data");
        };

        close(s);

    }


    return 0;
}

int lookup_and_connect( const char *host, const char *service ) {
	struct addrinfo hints;
	struct addrinfo *rp, *result;
	int s;
	/* Translate host name into peer's IP address */
	memset( &hints, 0, sizeof( hints ) );
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	if ( ( s = getaddrinfo( host, service, &hints, &result ) ) != 0 ) {
		fprintf( stderr, "stream-talk-client: getaddrinfo: %s\n", gai_strerror( s ) );
		return -1;
	}

	/* Iterate through the address list and try to connect */
	for ( rp = result; rp != NULL; rp = rp->ai_next ) {
		if ( ( s = socket( rp->ai_family, rp->ai_socktype, rp->ai_protocol ) ) == -1 ) {
			continue;
		}

		if ( connect( s, rp->ai_addr, rp->ai_addrlen ) != -1 ) {
			break;
		}

		close( s );
	}
	if ( rp == NULL ) {
		perror( "stream-talk-client: connect" );
		return -1;
	}
	freeaddrinfo( result );

	return s;
}