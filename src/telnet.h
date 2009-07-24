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


int sockfd;
char rcvBuffer[8192];

typedef struct telnet_uidata_s {
	char src[9];
	char path[100];
	char data[1000];	
} telnet_uidata_t;

/*typedef struct rcvBuffer {
	char line[1024];
	struct rcvBuffer *next;
} rcvBuffer;

rcvBuffer *firstRcvBuffer;
*/

short connectToAPRSIS(char*, int);
short sendDataToAPRSIS(char[]);
short loginToAPRSIS(char*, short, char*, short);
void disconnectFromAPRSIS();
short readDataFromAPRSIS(char *buffer);
short decodeTelnetFrame(char *frame, telnet_uidata_t *uidata);
