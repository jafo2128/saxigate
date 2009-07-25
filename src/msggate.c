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
#include "telnet.h"
#include "msggate.h"



void doMessageGate(char *mycall, short verbose) {
	char telnetrxdata[2048] = {0}; //telnet rx buffer.
	telnet_uidata_t telnet_uidata; //save decoded telnet frame.
	
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

