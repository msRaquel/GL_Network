
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <string.h>
#include <netdb.h>
#include <iostream>
#include <vector>
#include <arpa/inet.h>  // htonl/ntohl
#include <cstdio>       // fprintf, perror
#include <cstdlib>      // atoi, exit
#include <netinet/in.h> //sockaddr_in
#include <fstream>
#include <iomanip>


#define MAX_LINE 256
#define MAX_PENDING 5
#define MAX_FILES 10
#define MAX_FILENAME_LEN 100


struct Signal {
    uint8_t ttl;
    float RTT_ms;
};

struct Peer {
    Signal signal;
    uint32_t peerID = 0;
    char ip_string[16] = "";      // INET_ADDRSTRLEN
    char location[32] = "";
};


void AppendToGSPMatrix(Peer* incoming_matrix, size_t peer_count, const std::string& filename) {
    // Check if file exists to determine if we need to write a header
    std::ifstream check_file(filename);
    bool file_exists = check_file.good();
    check_file.close();

    std::ofstream csv_file(filename, std::ios::app);
    
    // Optional: Get current epoch timestamp to label the column
    long int current_time = static_cast<long int>(time(NULL));

    // If it's a completely flat structure where each incoming packet is one time-slice:
    // It's often easier to log: Timestamp, Node_ID, RTT, Hops
    // And let a 3-line Python script pivot it into the final matrix!
    for (size_t i = 0; i < peer_count; ++i) {
        csv_file << current_time << ","
                 << incoming_matrix[i].peerID << ","
                 << incoming_matrix[i].signal.RTT_ms << ","
                 << (64 - static_cast<int>(incoming_matrix[i].signal.ttl)) << ","
                 << incoming_matrix[i].location << "\n";
    }
    csv_file.close();
}

void ProcessIncomingMatrix(int s, char* buffer, int n, const std::string& csv_filename) {
    // Determine how many structural entries are packed into the stream
    size_t struct_size = sizeof(Peer);
    size_t peer_count = n / struct_size;

    if (peer_count == 0 || (n % struct_size) != 0) {
        std::cerr << "Warning: Received partial or misaligned network data stream.\n";
        return;
    }

    // Cast the continuous byte array back into an accessable structural array block
    Peer* incoming_matrix = reinterpret_cast<Peer*>(buffer);

    std::cout << "\n=========================================\n";
    std::cout << "GSP REGISTRY: Received metrics from " << peer_count << " nodes.\n";
    std::cout << "=========================================\n";

    for (size_t i = 0; i < peer_count; ++i) {
        uint8_t standard_starting_ttl = 64;
        int calculated_hops = standard_starting_ttl - static_cast<int>(incoming_matrix[i].signal.ttl);

        std::cout << "ID: " << incoming_matrix[i].peerID
                  << " | Loc: " << incoming_matrix[i].location
                  << " | IP: " << incoming_matrix[i].ip_string
                  << " | RTT: " << incoming_matrix[i].signal.RTT_ms << " ms"
                  << " | Hops: " << calculated_hops << "\n";
    }
    std::cout << "=========================================\n";

    AppendToGSPMatrix(incoming_matrix, peer_count, csv_filename);
    std::cout << "Successfully logged snapshot to: " << csv_filename << "\n";
}



//Credit to Professor Kredo for functions and code skeleton below
/*
 * Create, bind and passive open a socket on a local interface for the provided service.
 * Argument matches the second argument to getaddrinfo(3).
 *
 * Returns a passively opened socket or -1 on error. Caller is responsible for calling
 * accept and closing the socket.
 */
int bind_and_listen( const char *service );

/*
 * Return the maximum socket descriptor set in the argument.
 */
int find_max_fd(const fd_set *fs);

int main(int argc, char *argv[]){


	std::vector<struct peer_entry> peerEntries;	//create a vector that holds all peerInfo
	const char* SERVER_PORT;

	if (argc == 2){
		SERVER_PORT = argv[1];                  //retrieves user port selection, scared to have anything below 5000
	}
	else {
		fprintf( stderr, "usage error: %s host\n", argv[0] );
       	exit( 1 );
	}

	fd_set all_sockets;
	FD_ZERO(&all_sockets);
	fd_set call_set;
	FD_ZERO(&call_set);

	// listen_socket is the fd on which the program can accept() new connections
	int listen_socket = bind_and_listen(SERVER_PORT);
	FD_SET(listen_socket, &all_sockets);
	int max_socket = listen_socket;

	while(1) {

		call_set = all_sockets;
		int num_s = select(max_socket+1, &call_set, NULL, NULL, NULL);
		if( num_s < 0 ){
			perror("ERROR in select() call");
			return -1;
		}
		// Check each potential socket.
		// Skip standard IN/OUT/ERROR -> start at 3.
		for( int s = 3; s <= max_socket; ++s ){

			// Skip sockets that aren't ready
			if( !FD_ISSET(s, &call_set) )
				continue;

			// A new connection is ready
			if( s == listen_socket ){
				int new_s;
				new_s = accept(s, NULL, 0);
				if (new_s == -1){ //Error checking
					perror("Error in accept() call");
					return -1;
				}

                FD_SET(new_s, &all_sockets);
                FD_SET(new_s, &call_set);
				max_socket = find_max_fd(&all_sockets);
				break;
			}
			// A connected socket is ready
            else {
                // Expanded to handle full structural vector payloads in a single reception window
                char buffer[4096]; 
                int n = recv(s, buffer, sizeof(buffer), 0);
                
                if(n == -1){
                    perror("recv error");
                    FD_CLR(s, &all_sockets);
                    close(s);
                    return -1;
                }
                if (n == 0){
                    std::cout << "Peer closed connection on socket: " << s << "\n";
                    FD_CLR(s, &all_sockets);
                    close(s);                   
                    break;
                }   
                
                std::string gsp_log_file = "network_telemetry_raw.csv";
                
                // Call the updated handler with your file path
                ProcessIncomingMatrix(s, buffer, n, gsp_log_file);
                break;
            }
		}
	}
	return 0;
}

int find_max_fd(const fd_set *fs) {
	int ret = 0;
	for(int i = FD_SETSIZE-1; i>=0 && ret==0; --i){
		if( FD_ISSET(i, fs) ){
			ret = i;
		}
	}
	return ret;
}

int bind_and_listen( const char *service ) {
	struct addrinfo hints;
	struct addrinfo *rp, *result;
	int s;

	/* Build address data structure */
	memset( &hints, 0, sizeof( struct addrinfo ) );
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = 0;

	/* Get local address info */
	if ( ( s = getaddrinfo( NULL, service, &hints, &result ) ) != 0 ) {
		fprintf( stderr, "stream-talk-server: getaddrinfo: %s\n", gai_strerror( s ) );
		return -1;
	}

	/* Iterate through the address list and try to perform passive open */
	for ( rp = result; rp != NULL; rp = rp->ai_next ) {
		if ( ( s = socket( rp->ai_family, rp->ai_socktype, rp->ai_protocol ) ) == -1 ) {
			continue;
		}

		if ( !bind( s, rp->ai_addr, rp->ai_addrlen ) ) {
			break;
		}

		close( s );
	}
	if ( rp == NULL ) {
		perror( "stream-talk-server: bind" );
		return -1;
	}
	if ( listen( s, MAX_PENDING ) == -1 ) {
		perror( "stream-talk-server: listen" );
		close( s );
		return -1;
	}
	freeaddrinfo( result );

	return s;
}
