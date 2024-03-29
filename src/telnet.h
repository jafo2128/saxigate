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


typedef struct telnet_uidata_s {
	char src[9];
	char path[100];
	char data[1000];	
} telnet_uidata_t;

typedef struct telnet_info_s {
	char hostname[100];
	int port;
	short callpass;
	short messagegate;
	char version[15];
	char mycall[10];
} telnet_info_t;

short connectToAPRSIS();
short sendDataToAPRSIS(char[]);
short loginToAPRSIS();
void disconnectFromAPRSIS();
short readDataFromAPRSIS(char *buffer);
short decodeTelnetFrame(char *frame, telnet_uidata_t *uidata);
void connectAndLogin(short);
void setTelnetInfo(char *hostname, int port, char *mycall, short callpass,short messagegate, char *version);

int sockfd;
char rcvBuffer[8192];
struct telnet_info_s telnet_info;

