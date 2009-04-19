/*
 *  saxIgate, a small APRS I-Gate for Linux
 *  Copyright (C) 2009 Robbie De Lise (ON4SAX)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */ 

 
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

#include "telnet.h"

void disconnectFromAPRSIS() {
	close(sockfd);
}

//connect To APRSIS server (standard telnet socket though)
short connectToAPRSIS(char *hostname, int port) {
	struct hostent *server;
	struct sockaddr_in serv_addr;

	//create socket.
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	
	//check if we are alive.
	if (sockfd < 0) {
		fprintf(stderr,"ERROR opening socket");
		exit(-1);
	}
	
	//get the ip 
	server = gethostbyname(hostname);
	//check if we have an ip.
	if (server == NULL) {
		fprintf(stderr,"ERROR, no such host\n");
		exit(-1);
	}
	
	//clear serv_addr struct before we fill it..
	bzero((char*) &serv_addr, sizeof(serv_addr));
	//set the type to AF_INET.
	serv_addr.sin_family = AF_INET;
	//copy the ip into serv_addr
	bcopy((char*)server->h_addr, (char*)&serv_addr.sin_addr.s_addr, server->h_length);
	//set the port into serv_addr
	serv_addr.sin_port = htons(port);
	
	//connect to server.
	if(connect(sockfd,&serv_addr,sizeof(serv_addr)) < 0) {
		//fprintf(stderr,"ERROR connection");
		return 0;
	} else {
		return 1;
	}
}

//send data to APRSIS server over sockfd
short sendDataToAPRSIS(char buffer[1000]) {
	int n;
	//make sure a \n\r is at the end of the line to send.
	sprintf(buffer, "%s\n\r", buffer);
	//send line.
	n = write(sockfd,buffer,strlen(buffer));
	//fail ?
	if (n < 0) {
		//fprintf(stderr,"ERROR writing to socket");
		return 0;
	} else {
		return 1;
	}
}

//login to APRS server.
short loginToAPRSIS(char *callsign, short callpass, char *version) {
	char buffer[1000];
	//generate login string for aprs-is (javaprs docs)
	sprintf(buffer, "user %s pass %d vers saxIgate %s",callsign,callpass,version);
	//send it over the socket.
	if (sendDataToAPRSIS(buffer)) {
		usleep(10000); //wait 10ms.
		//sendout an ID beacon over APRS-IS.
		sprintf(buffer, "%s>ID,qAR,%s:>%s IGATE running saxIgate %s",callsign,callsign,callsign,version);
		if (sendDataToAPRSIS(buffer))
			return 1;
		else
			return 0;
	} else {
		return 0; 
	};
}
 

