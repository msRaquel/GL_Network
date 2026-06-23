/***************************************
server.cpp
Author: Raquel Zamudio
Date Completed: 06/16/2026
Description: This peer "server" script should receive the incoming Time-to-Live value of the 
connected socket. 
***************************************/
/* This code is an updated version of the sample code from "Computer Networks: A Systems
 * Approach," 5th Edition by Larry L. Peterson and Bruce S. Davis. Some code comes from
 * man pages, mostly getaddrinfo(3). */
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define SERVER_PORT "5432"
#define MAX_LINE 256
#define MAX_PENDING 5


/*
 * Create, bind and passive open a socket on a local interface for the provided service.
 * Argument matches the second argument to getaddrinfo(3).
 *
 * Returns a passively opened socket or -1 on error. Caller is responsible for calling
 * accept and closing the socket.
 */
int bind_and_listen( const char *service );

int main()
{
    char buf[MAX_LINE];
	int s,len;

    /* Bind socket to local interface and passive open */
	if ( ( s = bind_and_listen( SERVER_PORT ) ) < 0 ) {
		exit( 1 );
	}

    // return something back to connected socket to calculate latency from the receiving side
    

    // calculate 

    return 0;

}