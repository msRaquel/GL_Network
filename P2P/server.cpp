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
#include <netinet/in.h> // sockaddr_in
#include <fstream>
#include <iomanip>
#include <ctime>

#define MAX_LINE 256
#define MAX_PENDING 5
#define MAX_FILES 10
#define MAX_FILENAME_LEN 100

struct Signal {
    uint8_t ttl;
    float RTT_ms;
};

// Peer struct using char array for fixed memory layout
struct Peer {
    Signal signal;
    uint32_t peerID = 0;
    char pair_id[16] = "";
    char ip_string[INET_ADDRSTRLEN] = "";
    char location[32] = "";
};

// Save vector of incoming peers directly into CSV
void AppendToGSPMatrix(uint32_t sender_id, const std::vector<Peer>& peer_vector, const std::string& filename) {
    std::ofstream csv_file(filename, std::ios::app);
    if (!csv_file.is_open()) {
        std::cerr << "Error opening CSV file: " << filename << "\n";
        return;
    }

    long int current_time = static_cast<long int>(time(NULL));

    // Iterate through vector and append to CSV
    for (const auto& peer : peer_vector) {
        int calculated_hops = 64 - static_cast<int>(peer.signal.ttl);

       csv_file << current_time << ","
                 << sender_id << ","             // Row index 
                 << peer.peerID << ","            // Target ID
                 << peer.pair_id << ","           // "1-2" 
                 << peer.ip_string << ","
                 << peer.signal.RTT_ms << ","
                 << calculated_hops << ","
                 << peer.location << "\n";
    }
    csv_file.close();
}

void ProcessIncomingMatrix(int s, char* buffer, int n, const std::string& csv_filename) {
    if (n < static_cast<int>(sizeof(uint32_t))) {
        std::cerr << "Warning: Received incomplete payload header.\n";
        return;
    }

    //  Extract sender peer ID from first 4 bytes
    uint32_t sender_id = *reinterpret_cast<uint32_t*>(buffer);

    // Adjust payload pointer past sender_id
    char* struct_payload = buffer + sizeof(uint32_t);
    int payload_size = n - sizeof(uint32_t);

    size_t struct_size = sizeof(Peer);
    size_t peer_count = payload_size / struct_size;

    if (peer_count == 0 || (payload_size % struct_size) != 0) {
        std::cerr << "Warning: Received partial or misaligned network data stream.\n";
        return;
    }

    // Populate incoming peers into a vector
    Peer* raw_peers = reinterpret_cast<Peer*>(struct_payload);
    std::vector<Peer> incoming_vector(raw_peers, raw_peers + peer_count);

    std::cout << "\n=========================================\n";
    std::cout << "GSP REGISTRY: Received metrics from Sender ID: " << sender_id 
              << " (" << incoming_vector.size() << " targets in vector)\n";
    std::cout << "=========================================\n";

    // Print to terminal
    for (const auto& peer : incoming_vector) {
        uint8_t standard_starting_ttl = 64;
        int calculated_hops = standard_starting_ttl - static_cast<int>(peer.signal.ttl);
        std::cout << "Pair: " << peer.pair_id
                  << " | Target ID: " << peer.peerID
                  << " | IP: " << peer.ip_string
                  << " | Loc: " << peer.location
                  << " | RTT: " << peer.signal.RTT_ms << " ms"
                  << " | Hops: " << calculated_hops << "\n";
    }
    std::cout << "=========================================\n";

    // Append vector data to CSV file
    AppendToGSPMatrix(sender_id, incoming_vector, csv_filename);
    std::cout << "Successfully logged snapshot vector to: " << csv_filename << "\n";
}

/*
 * Create, bind and passive open a socket on a local interface for the provided service.
 * Argument matches the second argument to getaddrinfo(3).
 *
 * Returns a passively opened socket or -1 on error. Caller is responsible for calling
 * accept and closing the socket.
 */
// Courtesy sample code from "Computer Networks: A Systems Approach," 5th Edition by Larry L. Peterson and Bruce S. Davis
// modified by Dr. Kurtis Kredo from California State University, Chico
int bind_and_listen(const char *service);
int find_max_fd(const fd_set *fs);

int main(int argc, char *argv[]) {
    const char* SERVER_PORT;

    if (argc == 2) {
        SERVER_PORT = argv[1];
    } else {
        fprintf(stderr, "usage error: %s host\n", argv[0]);
        exit(1);
    }

    fd_set all_sockets;
    FD_ZERO(&all_sockets);
    fd_set call_set;
    FD_ZERO(&call_set);

    int listen_socket = bind_and_listen(SERVER_PORT);
    FD_SET(listen_socket, &all_sockets);
    int max_socket = listen_socket;

    while (1) {
        call_set = all_sockets;
        int num_s = select(max_socket + 1, &call_set, NULL, NULL, NULL);
        if (num_s < 0) {
            perror("ERROR in select() call");
            return -1;
        }

        for (int s = 3; s <= max_socket; ++s) {
            if (!FD_ISSET(s, &call_set))
                continue;

            if (s == listen_socket) {
                int new_s = accept(s, NULL, 0);
                if (new_s == -1) {
                    perror("Error in accept() call");
                    return -1;
                }

                FD_SET(new_s, &all_sockets);
                FD_SET(new_s, &call_set);
                max_socket = find_max_fd(&all_sockets);
                break;
            } else {
                char buffer[4096]; 
                int n = recv(s, buffer, sizeof(buffer), 0);
                
                if (n == -1) {
                    perror("recv error");
                    FD_CLR(s, &all_sockets);
                    close(s);
                    return -1;
                }
                if (n == 0) {
                    std::cout << "Peer closed connection on socket: " << s << "\n";
                    FD_CLR(s, &all_sockets);
                    close(s);                   
                    break;
                }   
                
                std::string gsp_log_file = "network_telemetry_raw.csv";
                ProcessIncomingMatrix(s, buffer, n, gsp_log_file);
                break;
            }
        }
    }
    return 0;
}

int find_max_fd(const fd_set *fs) {
    int ret = 0;
    for (int i = FD_SETSIZE - 1; i >= 0 && ret == 0; --i) {
        if (FD_ISSET(i, fs)) {
            ret = i;
        }
    }
    return ret;
}

int bind_and_listen(const char *service) {
    struct addrinfo hints;
    struct addrinfo *rp, *result;
    int s;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = 0;

    if ((s = getaddrinfo(NULL, service, &hints, &result)) != 0) {
        fprintf(stderr, "stream-talk-server: getaddrinfo: %s\n", gai_strerror(s));
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        if ((s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1) {
            continue;
        }

        if (!bind(s, rp->ai_addr, rp->ai_addrlen)) {
            break;
        }

        close(s);
    }

    if (rp == NULL) {
        perror("stream-talk-server: bind");
        return -1;
    }

    if (listen(s, MAX_PENDING) == -1) {
        perror("stream-talk-server: listen");
        close(s);
        return -1;
    }

    freeaddrinfo(result);
    return s;
}