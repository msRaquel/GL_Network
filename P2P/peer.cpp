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
#include <netdb.h>

#define MAX_BUFFER_SIZE 256 
#define STARTING_TTL 64
// signal attached to each peer (node)
// ipv4 values range from 0 - 255 for header fields, so we use unsigned 8 bit variable to replicate the header field
struct Signal{
    uint8_t ttl;
    float RTT_ms;

};

// keep track of peer information
struct Peer{
    Signal signal;
    uint32_t peerID = 0;
	char ip_string[INET_ADDRSTRLEN] = "";
};

/* check if ping execution was succesful */

bool executePing(){
    return true;
};

// search for a specific set of characters from a buffer
// update size argument and then 
int findStringLoc(char *buff, const char *goalString, size_t &index){

    // look for goal string, in this case search for  
   char *foundString = strstr(buff, goalString);

   if (foundString != NULL){
        index = foundString - buff;
   }
   else {

        return -1;
   }

   return 1;

}

// goal of this function is to extract values returned by the ping command 
// buffer will be provided as an argument with the statistics provided by ping
// once values have been extracted initialize a peer struct with the values found
void extractPingStats(struct Peer &peer, char *buff){
    // use findString function to find the location of the time and round trip values
    size_t RTT_pos,TTL_pos;
     
    // find the location of where the time value exists
   // 1. Search for "time=" and "ttl=" (Note: Linux ping uses lowercase "ttl=")
    int foundTime = findStringLoc(buff, "time=", RTT_pos);
    int foundTTL  = findStringLoc(buff, "ttl=", TTL_pos);
// 2. ONLY parse if BOTH fields exist on this specific line of the buffer
    if (foundTime == 1 && foundTTL == 1) {
        // Safe extraction
        sscanf(buff + RTT_pos, "time=%f", &peer.signal.RTT_ms);
        sscanf(buff + TTL_pos, "ttl=%hhd", &peer.signal.ttl);

        std::cout << "\n>>> [Parsed Data] <<<" << std::endl;
        std::cout << "Latency measured at " << peer.signal.RTT_ms << " ms" << std::endl;
        std::cout << "TTL measured at " << (int)peer.signal.ttl << std::endl;
        std::cout << "---------------------\n" << std::endl;
    }



}

int main(int argc, char *argv[])
{
    // keep track of peer
    int peerID, status;
    // host to be used for DNS lookup through lookup and connect 
    const char *host = "google.com"; // ip address to be translate
    const char *PING_BASE = "ping -c 4 ";

    Peer peer;
    char pingCall[64];
    char buff[MAX_BUFFER_SIZE];

    size_t baseLen = std::strlen(PING_BASE);
    size_t ipLen = std::strlen(host);
    size_t totalLen = baseLen + ipLen;

    // Copy the base ping command to the start of the buffer
    std::memcpy(pingCall, PING_BASE, baseLen);

    // Copy the host address directly
    std::memcpy(pingCall + baseLen, host, ipLen);

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
        extractPingStats(peer, buff);
    } 
    status = pclose(pipe);
    if (status == -1) {
    /* Error reported by pclose() */
    perror("error when closing pipe");
    }

    std::cout << "Latency measured for this peer " << peer.signal.RTT_ms << " ms" << std::endl;
    std::cout << "Hops measured for this peer " << (STARTING_TTL - (int)peer.signal.ttl) << " hops" << std::endl; 

    return 0;

}
