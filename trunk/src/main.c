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

#include "ax25.h"
#include "callpass.h"
#include "telnet.h"
#include "cache.h"
#include "igate.h"
//#include "msggate.h"
 
#define version "0.1.7"


void strtoupper(char*);
void showhelp(char*);

//main routine.
int main(int argc, char *argv[]) {
	int c = 0;				//for getopt
	short verbose = 0;	//boolean for verbose
	char mycall[10] = {0};		//our callsign
	char *hostname;		//aprs-is server hostname
	char *portstr;		//aprs-is server port as text
	int port = 0;			//aprs-is server port as integer
	short callpass; 	//callpass for out callsign from callpass generator
	short messagegate = 0; //for the messagegate function.
	
	//alloc space for hostname & portstr
	hostname = calloc(100, sizeof(char));
	portstr = calloc(10, sizeof(char));
	
	//plot program info
	printf("saxIgate v%s (c) 2009-2010 Robbie De Lise (ON4SAX)\n",version);
	
	//get options from argv 	
	while ((c = getopt(argc, argv, "hvmc:p:s:")) != -1) {
		switch (c) {
			case 'V': //verbose mode
			case 'v':
				verbose = 1;
				break;
			
			case 'C': //set our callsign
			case 'c':
				strcpy(mycall,optarg);
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
				strcpy(hostname,strtok(optarg,":"));
				strcpy(portstr,strtok(NULL,"\0"));
				//if no port specified, use default.
				if (portstr == NULL) {
					port = 14580;
				} else {
					//or use specified, convert to int.
					port = atoi(portstr);
				}
				//no need for portstr anymore..
				free(portstr);
				break;
			
			case 'P': //add a ax25 port.
			case 'p':
				//add port or fail.
				if (!add_port(optarg)) {
					fprintf(stderr,"Failed to use port %s\n\r",optarg);
					exit(-1);
				}
				break;
			/*case 'M': //route messages from aprs-is to local ax25 stations in mheard
			case 'm':
					messagegate = 1;
					printf("\n*********************** !!! WARNING !!! ***********************");
					printf("\n* The MessageGate function is *NOT* completed yet and in beta *");
					printf("\n* It will forward __ALL__ messages from the APRS-IS WorldWide *");
					printf("\n* backbone to your first RF port. This is considered RF-SPAM  *");
					printf("\n* and frowned upon. Please do not use this in production !!   *");
					printf("\n***************************************************************\n");
					printf("\n*********************** !!! WARNING !!! ***********************");
					printf("\n* Sending data TO rf has not been tested with a live TNC yet. *");
					printf("\n* Use at OWN RISK and *NOT* on your local APRS frequency!     *");
					printf("\n***************************************************************\n");
					
				break;*/
			case 'H': //print help.
			case 'h': 
			case '?':
				showhelp(argv[0]);
				break;	
				
		}
	}

	//check if flags are specified.
	if ((numports() < 1) || (hostname == NULL) || (mycall == NULL)) {
		showhelp(argv[0]);
	}

	//deamon mode.
	if (verbose == 0) {
		int fx;
		fx = fork();
		if (fx == -1) {
			fprintf(stderr,"Failed to start as deamon\n\r");
			exit(-1);
		}
		if (fx != 0) {
			printf("Running as deamon with pid %i\n\r",fx);
			exit(-1); //exit parent.
		}
	}

	//open mac layer to AX25 in kernel or die.
	if (!mac_init()) {
		fprintf(stderr,"Failed to initialize AX25 subsystem");
		exit(-1);	
	}		
	
	//print some data.
	/*if (verbose) printf("\nMycall: %s\nCallpass: %d\nServer: %s port %i\nMessage Gate:",mycall, callpass, hostname, port);
	if (verbose && messagegate == 1) printf(" Active");
	else if (verbose) printf(" Inactive");*/
	if (verbose) printf("\nMycall: %s\nCallpass: %d\nServer: %s port %i",mycall, callpass, hostname, port);
	if (verbose) printf("\n\n");
	
	//set telnet_info
	setTelnetInfo(hostname,port,mycall,callpass,messagegate,version);
	
	//connect to APRS-IS
	connectAndLogin(verbose);

	//run main loop
	while(1) {
		//sleep 25ms to avoid using 100% cpu
		usleep(25000);
		
		//check ax25 mac for data and send to igate.
		readAx25DataForIgate(mycall, verbose);
			
		//check for data on telnet
		/*if (messagegate == 1) {
			doMessageGate(mycall, verbose);
		}*/
			

	} //endwhile mainloop. (ctrl+c or external signal)
	
	return 0; 
}

//convert all chars in string to uppercase.
void strtoupper(char* s) {
	while ((*s = toupper(*s))) ++s;
}



//show usage help
void showhelp(char *filename) {
	printf("Usage: %s -p <port> [-p <port>] [-p <port>] [...] -c <igatecall> -s <APRS-IS server> [-m] [-v]\n\n",filename);
	printf("\t-p\tAdd an AX25 port to listen on. Multiples can be specified with a maximum of 8\n");
	printf("\t-c\tThe callsign to login to APRS-IS. eg: ON4SAX-10\n");
	printf("\t-s\tThe address to connect to APRS-IS. eg: belgium.aprs2.net:14580\n");
	printf("\t\tor belgium.aprs2.net. In the latter the default port 14580 will be used.\n");
	//printf("\t-m\tTurns on message gate. This will forward messages from aprs-is to\n");
	printf("\t\tthe air for stations locally heard in the last 10 minutes.\n"); 
	printf("\t-v\tVerbose output. Program will stay in foreground.\n");
	printf("\t-h\tPrint help (you are looking at it)\n");
	printf("\nExample of Usage: %s -p 2m -p 70cm -c ON4SAX-10 -s belgium.aprs2.net:14580 -m\n\n",filename);
	printf("Report bugs to <on4sax@on4sax.be>.\n");
	exit(-1);
}


