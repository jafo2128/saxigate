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


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <net/if.h>
#include <sys/socket.h>
//#include <linux/ax25.h>
#include <linux/if_ether.h>
#include <asm/byteorder.h>
#include <netax25/kernel_ax25.h>
#include <netax25/axlib.h>
#include <netax25/axconfig.h>

#include "ax25.h"
#include "callpass.h"
#include "telnet.h"

#define version "0.1.3"

void strtoupper(char*);
void igateformat(uidata_t*, char*, char*);
void showhelp(char*);

//main routine.
int main(int argc, char *argv[]) {
	int c;				//for getopt
	short verbose = 0;	//boolean for verbose
	char *mycall;		//our callsign
	char *hostname;		//aprs-is server hostname
	char *portstr;		//aprs-is server port as text
	int port;			//aprs-is server port as integer
	short callpass; 	//callpass for out callsign from callpass generator

	//get options from argv 	
	while ((c = getopt(argc, argv, "hvc:p:s:")) != -1) {
		switch (c) {
			case 'V': //verbose mode
			case 'v':
				verbose = 1;
				break;
			
			case 'C': //set our callsign
			case 'c':
				mycall = optarg;
				//make sure string is in uppercase.
				strtoupper(mycall); 

				//check if specified call is valid.
				if (!is_call(mycall)) { 
					fprintf(stderr,"%s is an invalid call.\n\r", mycall);
					showhelp(argv[0]);
					exit(-1);
				}
				
				//generate callpass for our callsign.
				callpass = genCallPass(mycall); 
				break;
			
			case 'S': //set aprs-is server
			case 's':
				//split string on ':' for hostname and port.
				hostname = strtok(optarg,":");
				portstr = strtok(NULL,"\0");
				//if no port specified, use default.
				if (portstr == NULL) {
					port = 14580;
				} else {
					//or use specified, convert to int.
					port = atoi(portstr);
				}
				break;
			
			case 'P': //add a ax25 port.
			case 'p':
				//add port or fail.
				if (!add_port(optarg)) {
					fprintf(stderr,"Failed to use port %s\n\r",optarg);
					exit(-1);
				}
				break;
			case 'H': //print help.
			case 'h': 
				showhelp(argv[0]);
				break;	
			
			case '?': //something wrong with opts ?
				showhelp(argv[0]);
				break;
				
		}
	}

	//check if flags are specified.
	if ((numports() < 1) || (hostname == NULL) || (mycall == NULL)) {
		showhelp(argv[0]);
	}


	//open mac layer to AX25 in kernel or die.
	if (!mac_init()) {
		fprintf(stderr,"Failed to initialize AX25 subsystem");
		exit(-1);	
	}		
	
	//print some data.
	if (verbose) printf("saxIgate v%s (c) 2009 Robbie De Lise (ON4SAX)\n\n",version);
	if (verbose) printf("Mycall: %s\nCallpass: %d\nServer: %s port %i\n\n",mycall, callpass, hostname, port);
	
	//connect to APRS-IS
	if (verbose) printf("Connecting to %s:%i and logging in as %s\n",hostname,port,mycall);
	connectToAPRSIS(hostname, port);
	usleep(25000);
	loginToAPRSIS(mycall, callpass, version);
	//if we get here we should be connected.
	if (verbose) printf("Connected!\n\n");
	
	//run main loop
	while(1) {
		//sleep 25ms to avoid using 100% cpu
		usleep(25000);
		
		frame_t frame; 		//save raw ax25 frame.
		uidata_t uidata_p; 	//save decoded ax25 frame.
		short result; 		//save result from decoding.
		char igatestring[1000] = {0}; //save igate format string
		
		//check for new data
		while (mac_avl()) { 
			//read new data into frame.
			if (mac_inp(&frame) == 0) {
				//decode data.
				result = frame2uidata(&frame, &uidata_p, mycall);
				if (result == 1) {
					//show ax25 data on screen.
					if (verbose) dump_uidata_from(&uidata_p);
					//format for igate transmission.
					igateformat(&uidata_p, mycall, igatestring);
					//show the data in igate format.
					if (verbose) printf("To igate: %s\n\n",igatestring);
					//send to igate.
					sendDataToAPRSIS(igatestring);
				} //endif (result==1)
			}	//endif(mac_inp)
		} //endwhile (mac_avl)
	} //endwhile mainloop. (ctrl+c or external signal)
	
	return 0; 
}

//format data to be transmitted to an Igate
void igateformat(uidata_t *uidata, char *mycall, char *out) {
	char *digis = "";		//save digi's
	char data[1000] = {0};	//save data, clear reserved memory.
	int i;					//counter for for's
	
	//run trough digi's in uidata.
	for (i = 0; i < uidata->ndigi; i++) {
		char *tmp;	//tmp
		sprintf(tmp, "%s,%s", digis, uidata->digipeater[i]);	//DIGICALL,NEWDIGICALL
		digis = tmp; 
	}
	
	//run trough data and copy it byte per byte.
	for (i = 0; i < uidata->size; i++) {
		sprintf(data, "%s%c", data, uidata->data[i]);
	}
	
	//SOURCE>DEST,DIGICALL,DIGICALL,qAR,mycall:data
	//save into 'out'.
	sprintf(out,"%s>%s%s,qAR,%s:%s", uidata->originator, uidata->destination,digis,mycall,data);
}

//convert all chars in string to uppercase.
void strtoupper(char* s) {
	while ((*s = toupper(*s))) ++s;
}

//show usage help
void showhelp(char *filename) {
	printf("\nsaxIgate v%s (c) 2009 Robbie De Lise (ON4SAX)\n",version);
	printf("Usage: %s -p <port> [-p <port>] [-p <port>] [...] -c <igatecall> -s <APRS-IS server> [-v]\n\n",filename);
	printf("\t-p\tAdd an AX25 port to listen on. Multiples can be specified with a maximum of 8\n");
	printf("\t-c\tThe callsign to login to APRS-IS. eg: ON4SAX-10\n");
	printf("\t-s\tThe address to connect to APRS-IS. eg: t2belgium.aprs2.net:14580\n");
	printf("\t\tor t2belgium.aprs2.net. In the latter the default port 14580 will be used.\n");
	printf("\t-v\tVerbose output.\n");
	printf("\t-h\tPrint help (you are looking at it)\n");
	printf("\nCurrent version does not go into deamon mode. Please use '&' to place proc into background\n");
	printf("\nExample of Usage: %s -p 2m -p 70cm -c ON4SAX-10 -s t2belgium.aprs2.net:14580 &\n\n",filename);
	printf("Report bugs to <on4sax@on4sax.be>.\n");
	exit(-1);
}