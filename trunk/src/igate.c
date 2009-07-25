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
#include "igate.h"
#include "cache.h"
#include "telnet.h"

void readAx25DataForIgate(char *mycall, short verbose) {
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
						if (verbose) printf("Connection lost. Reconnecting in 5 seconds..\n");
						sleep(5);
						connectAndLogin(verbose);
					}
				} else {
					if (verbose) printf("Double data. Already sent to APRS-IS.\n");
				} //endif (checkCache);
			} //endif (result==1)
		}	//endif(mac_inp)
	} //endwhile (mac_avl)
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
