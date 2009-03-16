/*
 *  Copyright (C) 2009 - Robbie De Lise (ON4SAX)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *  Please note that the GPL allows you to use the driver, NOT the radio.
 *  In order to use the radio, you need a license from the communications
 *  authority of your country.
 */ 
 
 #include <stdio.h>
 #include <ctype.h>
 #include <string.h>
 #include "callpass.h"
 

 
 /* This code was taken from the Xastir project */
 short genCallPass(char *theCall) {
	char rootCall[10]; // need to copy call to remove ssid from parse
	char *p1 = rootCall;
	short hash;
	short i, len;
	char *ptr = rootCall;
	
	while ((*theCall != '-') && (*theCall != '\0')) *p1++ = toupper((int)(*theCall++));
		*p1 = '\0';
		
	hash = kKey; //init with the key value
	i = 0;
	len = (short)strlen(rootCall);
	
	while (i<len) { //loop through the string two bytes at a time;
		hash ^= (unsigned char)(*ptr++)<<8; //xor high byte with accumulated hash
		hash ^= (*ptr++); //xor low byte with accumulated hash.
		i+=2;
	}
	return (short)(hash & 0x7fff); //mask off the high bit so number is always positive
}