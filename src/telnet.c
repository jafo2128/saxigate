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
 
short readDataFromAPRSIS(char *buffer) {
	//This function will read the telnet data in chunks of 256 bytes and place them
	//in a 8kb buffer. It will then search the buffer for the first '\n' and return the
	//line. Next time, even if there is no telnet data waiting, we can return the next line.
	
	int i = 0; //4fors.
	int cc = 0; //to see if we have received anything.
	char localbuffer[256] = {0}; //to save data we rcvd from sockfd
	
	//how long should we wait for data ?
	struct timeval timeout; 
	timeout.tv_sec = 1; //one second.
	timeout.tv_usec = 0;
	
	//add to fd_set socks
	fd_set socks;
	FD_ZERO(&socks);
	FD_SET(sockfd,&socks);
	
	//make sure caller buffer is empty.
	for (i = 0; i <= strlen(buffer); i++) {
		buffer[i] = '\0';
	}
	
	//check if there is data waiting
	int tmp = 0;
	tmp = select(sockfd+1, &socks, (fd_set *) 0, (fd_set *) 0, &timeout);
	//printf("%i\n",tmp);
	if (tmp > 0) {
		//read into localbuffer
		cc = read(sockfd, (char *)localbuffer, 256);
		if (cc > 0) { //if there is any data..
			//append localbuffer to rcvBuffer.
			strcat(rcvBuffer,localbuffer);
		}
	}
	
	if (strlen(rcvBuffer) > 0) { //if there is data in rcvBuffer..
		//scan to first '\n';
		short lookingForLinefeed = 1;
		char newRcvBuffer[8192] = {0};
		for (i = 0; i <= strlen(rcvBuffer); i++) {
			if (rcvBuffer[i] != '\n' && rcvBuffer[i] != '\r' && lookingForLinefeed) {
				//copy everything until \n or \r into the return buffer.
				sprintf(buffer,"%s%c",buffer,rcvBuffer[i]);
			} else if (lookingForLinefeed) {
				//we are no longer looking for \n or \r, we have what we need. Turn it of. 
				lookingForLinefeed = 0;
				//check if next char is \n or \r and skip it to avoid sending back an empty buffer next run.
				if (rcvBuffer[i] == '\n' || rcvBuffer[i] == '\r') i++;
			} else {
				if (rcvBuffer[i] != '\0') //we don't want the nullbyte from the previous rcvBuffer
					//copy the remainder into a newRcvBuffer for next run.
					sprintf(newRcvBuffer,"%s%c",newRcvBuffer,rcvBuffer[i]);
			}
		}
		//copy the new buffer to the live buffer.
		strcpy(rcvBuffer,newRcvBuffer);
		//printf("\n***********\nrcv:%s\n************\n",rcvBuffer);
		//we have data!
		return 1;
	}
	//we have no data..*/
	return 0;
}

short decodeTelnetFrame(char *frame, telnet_uidata_t *uidata) {
	if (frame == NULL)
		return 0; //no data.
	
	if (frame[0] == '#') //OOB data from javaprssrv etc
		return 0;
	
	char *from;
	char *path;
	char *payload;
	//make room for data;
	from = calloc(10,sizeof(char));
	path = calloc(100,sizeof(char));
	payload = calloc(1000,sizeof(char));
	//split frame into pieces	
	from = strtok(frame,">");
	path = strtok(NULL,":");
	payload = strtok(NULL,"\0");
	
	//check if we miss any data. if so, we do not have a msg.
	if (from == NULL || path == NULL || payload == NULL) {
		/*if (from != NULL) free(from);
		if (path != NULL) free(path);
		if (payload != NULL) free(payload);*/
		return 0;
	}
	
	strcpy(uidata->src,from);
	strcpy(uidata->path,path);
	strcpy(uidata->data,payload);
	
	//printf("%s | %s | %s\n",from,path,payload);
	
	return 1;
}

//login to APRS server.
short loginToAPRSIS(char *callsign, short callpass, char *version, short messagegate) {
	char buffer[1000];
	char filter[12] = {0};
	
	if (messagegate == 1) {
		strcpy(filter," filter t/m");
	}
	
	//generate login string for aprs-is (javaprs docs)
	sprintf(buffer, "user %s pass %d vers saxIgate %s%s",callsign,callpass,version,filter);
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
 

