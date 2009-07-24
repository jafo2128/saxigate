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
 
#define version "0.1.7-beta"


short checkMessageAgainstMHandPrepareForTX(telnet_uidata_t *uidata, char *mycall, char *rftx);
void sendToRf(char *payload, char *mycall, short verbose);

void strtoupper(char*);
void igateformat(uidata_t*, char*, char*);
void connectAndLogin(char *hostname,int port,char *mycall,short callpass,short,short verbose);
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
	frame_t frame; 		//save raw ax25 frame.
	uidata_t uidata_p; 	//save decoded ax25 frame.
	short result; 		//save result from decoding.
	char igatestring[1000] = {0}; //save igate format string
	char telnetrxdata[2048] = {0}; //telnet rx buffer.
	telnet_uidata_t telnet_uidata; //save decoded telnet frame.
	short messagegate = 0; //for the messagegate function.
	
	//alloc space for hostname & portstr
	hostname = calloc(100, sizeof(char));
	portstr = calloc(10, sizeof(char));
	
	//plot program info
	printf("saxIgate v%s (c) 2009 Robbie De Lise (ON4SAX)\n",version);
	
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
			case 'M': //route messages from aprs-is to local ax25 stations in mheard
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
					
				break;
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
	if (verbose) printf("\nMycall: %s\nCallpass: %d\nServer: %s port %i\nMessage Gate:",mycall, callpass, hostname, port);
	if (verbose && messagegate == 1) printf(" Active");
	else if (verbose) printf(" Inactive");
	if (verbose) printf("\n\n");
	
	//connect to APRS-IS
	connectAndLogin(hostname,port,mycall,callpass,messagegate,verbose);

	//run main loop
	while(1) {
		//sleep 25ms to avoid using 100% cpu
		usleep(25000);
		
		//printf("MainWhile\n");
		
		//check for new data
		while (mac_avl()) { 
			//read new data into frame.
			if (mac_inp(&frame) == 0) {
				//decode data.
				result = frame2uidata(&frame, &uidata_p, mycall);
				if (result == 1) {
					//show ax25 data on screen.
					dump_uidata_from(&uidata_p, verbose);
					//check cache for double data.
					if (checkCache(&uidata_p)) { 
						//format for igate transmission.
						igateformat(&uidata_p, mycall, igatestring);
						//show the data in igate format.
						if (verbose) printf("To igate: %s\n\n",igatestring);
						//send to igate.
						if (!sendDataToAPRSIS(igatestring)) {
							//we probably lost connection to the igate. Lets try to connect again.
							disconnectFromAPRSIS();
							if (verbose) printf("Connection lost to %s:%i. Reconnecting in 5 seconds..\n",hostname,port);
							sleep(5);
							connectAndLogin(hostname,port,mycall,callpass,messagegate,verbose);
						}
					} else {
						if (verbose) printf("Double data. Already sent to APRS-IS.\n");
					} //endif (checkCache);
				} //endif (result==1)
			}	//endif(mac_inp)
		} //endwhile (mac_avl)
	
	
		//check for data on telnet
		if (messagegate == 1) {
			while(readDataFromAPRSIS(telnetrxdata)) {
				char txrf[256] = {0};
				//printf("Telnet: %s\n",telnetrxdata);
				decodeTelnetFrame(telnetrxdata,&telnet_uidata);
				if (checkMessageAgainstMHandPrepareForTX(&telnet_uidata, mycall, txrf)) {
					//printf("txrf: %s\n",txrf);
					sendToRf(txrf, mycall, verbose);
				}
			}
		}
			

	} //endwhile mainloop. (ctrl+c or external signal)
	
	return 0; 
}

void sendToRf(char *payload, char *mycall, short verbose) {
	uidata_t uidata;
	int n = 0;
	char *tmp;
	frame_t frame;
	
	/* prepare transmission */
    uidata.port = 0;

    /* transmit remotely */
    uidata.distance = REMOTE;

    /* primitive UI with no poll bit */
    uidata.primitive = 0x03;

    /* PID = 0xF0, normal AX25*/
    uidata.pid = 0xF0;

    /* fill in originator */
    strcpy(uidata.originator, mycall);
    uidata.orig_flags = (char) (RR_FLAG | C_FLAG);

    /* fill in destination */
    strcpy(uidata.destination, "APNS01");
    uidata.dest_flags = RR_FLAG;
	
	//set digipath to WIDE1-1
	strcpy(uidata.digipeater[0], "WIDE1-1");
	uidata.digi_flags[0] = RR_FLAG;
	uidata.ndigi = 1;
	
	/* convert newline to cariage-return characters */
	tmp = payload;
	while(*tmp != '\0')
	{
	    if(*tmp == '\n')
	    {
	        *tmp = '\r';
	    }
	    tmp++;
	}
	
	
	/* check if there is a '\r' at the end, if not then add it */
    n = strlen(payload);
    if((n == 0) || (payload[n - 1] != '\r'))
      {
      /* add a <CR> at the end */
      strcat(payload,"\r");
    }

	//check for empty line.	
    if(strlen(payload) == 1)
    {
        return;
    }
	
	//add payload to uidata.
    strcpy(uidata.data, payload);
    uidata.size = strlen(payload);
    
    dump_uidata_to(&uidata, verbose);
    
    //convert to ax25 mac frame.
    uidata2frame(&uidata, &frame);
    //transmit.
    mac_out(&frame);
    
    
}

short checkMessageAgainstMHandPrepareForTX(telnet_uidata_t *uidata, char *mycall, char *rftx) {
	//if the payload starts with ':' we have a message.
	if (uidata->data[0] == ':') {
		//we have a msg!
		char to[10] = {0};
		char msg[256] = {0};
		strcpy(to,strtok(uidata->data,":"));
		strcpy(msg,strtok(NULL,"\0"));
		//check mh
		
		//prepare to tx to rf
		char dst[10];
		strcpy(dst,strtok(uidata->path,","));
		sprintf(rftx,"}%s>%s,TCPIP,%s*:%s:%s\n",uidata->src,dst,mycall,to,msg);
		return 1;
	}
	
	return 0;
}

//format data to be transmitted to an Igate
void igateformat(uidata_t *uidata, char *mycall, char *out) {
	char digis[100] = {0};		//save digi's
	char data[1000] = {0};	//save data, clear reserved memory.
	int i;					//counter for for's
	
	//run trough digi's in uidata.
	//printf("Total Digis: %i\n",uidata->ndigi);
	for (i = 0; i < uidata->ndigi; i++) {
		char tmp[100];	//tmp
		sprintf(tmp, "%s,%s", digis, uidata->digipeater[i]);	//DIGICALL,NEWDIGICALL
		strcpy(digis,tmp); 
		//printf("Digis: %i->%s = %s\n",i,uidata->digipeater[i],digis);		
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

void connectAndLogin(char *hostname,int port,char *mycall,short callpass,short messagegate, short verbose) {
	if (verbose) printf("Connecting to %s:%i as %s\n",hostname,port,mycall);
	if (connectToAPRSIS(hostname, port)) {
		usleep(25000);
		if (!loginToAPRSIS(mycall, callpass, version, messagegate)) {
			if (verbose) printf("Failed to send login data. Retrying in 5 seconds..\n");
			disconnectFromAPRSIS();
			sleep(5);
			connectAndLogin(hostname,port,mycall,callpass,messagegate,verbose);
		} else {
			if (verbose) printf("Connected!\n");
		}
	} else {
		if (verbose) printf("Failed to connect. Retrying in 5 seconds..\n");
		disconnectFromAPRSIS();
		sleep(5);
		connectAndLogin(hostname,port,mycall,callpass,messagegate,verbose);
	}
}

//show usage help
void showhelp(char *filename) {
	printf("Usage: %s -p <port> [-p <port>] [-p <port>] [...] -c <igatecall> -s <APRS-IS server> [-m] [-v]\n\n",filename);
	printf("\t-p\tAdd an AX25 port to listen on. Multiples can be specified with a maximum of 8\n");
	printf("\t-c\tThe callsign to login to APRS-IS. eg: ON4SAX-10\n");
	printf("\t-s\tThe address to connect to APRS-IS. eg: belgium.aprs2.net:14580\n");
	printf("\t\tor belgium.aprs2.net. In the latter the default port 14580 will be used.\n");
	printf("\t-m\tTurns on message gate. This will forward messages from aprs-is to\n");
	printf("\t\tthe air for stations locally heard in the last 10 minutes.\n"); 
	printf("\t-v\tVerbose output. Program will stay in foreground.\n");
	printf("\t-h\tPrint help (you are looking at it)\n");
	printf("\nExample of Usage: %s -p 2m -p 70cm -c ON4SAX-10 -s belgium.aprs2.net:14580 -m\n\n",filename);
	printf("Report bugs to <on4sax@on4sax.be>.\n");
	exit(-1);
}


