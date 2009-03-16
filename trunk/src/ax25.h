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


/* The ax25.c and ax25.h files contain code mostly taken from Digi_ned by Henk De Groot (PE1DNN) 
 * and modified by Robbie De Lise (ON4SAX)
 * 
 * Digi_ned is an excellent APRS DigiPeater for DOS and LINUX.
 * For more info, please visit: http://www.homepages.hetnet.nl/~pe1dnn/
 */
 


#define MAX_PORTS            8     /* Maximum number of PORTS in AX25_MAC  */ 
#define MAX_FRAME_LENGTH   400     /* Maximum length of a raw frame        */
#define MAX_DATA_LENGTH    256     /* Maximum data lenght in a frame       */
#define MAX_DIGI             8     /* Maximum number of DIGI's in a frame  */ 
#define CALL_LENGTH         10     /* Max. length of a call: "PE1DNN-12\0" */

#define MAX_MESSAGE_LENGTH 128     /* Max length of a message              */

#define END_FLAG  (0x01)
#define C_FLAG    (0x80)
#define H_FLAG    (0x80)
#define R1_FLAG   (0x20)
#define R2_FLAG   (0x40)
#define RR_FLAG   (R1_FLAG | R2_FLAG)
#define SSID_MASK (0x1E)
#define FLAG_MASK (0xE0)


typedef enum { LOCAL, REMOTE } distance_t;

typedef struct ports_s {
    int    s_out;
    char  *dev_p;
    char  *port_p;
} ports_t;

/* struct for storing raw assembled RX and TX packets */
typedef struct frame_s {
    short              len;                     /* length of the frame       */
    char               port;                    /* port number               */
    unsigned char      frame[MAX_FRAME_LENGTH]; /* L1 frame without CRC      */
} frame_t;

/* struct for storing RX and TX disassembled packets */
typedef struct uidata_s {
    short              port;                    /* port of this frame        */
    distance_t         distance;                /* LOCAL or REMOTE RX and TX */
    char               primitive;               /* primitive + P/F bit       */
    unsigned short     pid;                     /* PID or 0xffff if no PID   */
    char               originator[CALL_LENGTH]; /* source call of the frame  */
    char               orig_flags;              /* flags surrounding SSID    */
    char               destination[CALL_LENGTH];/* destination call of frame */
    char               dest_flags;              /* flags surrounding SSID    */
    short              ndigi;                   /* number of digipeaters     */
    char               digipeater[MAX_DIGI][CALL_LENGTH]; /* digi calls      */
    char               digi_flags[MAX_DIGI];    /* flags surrounding SSID    */
    short              size;                    /* size of data in frame     */
    char               data[MAX_DATA_LENGTH];   /* data of the frame         */
} uidata_t;




int add_port(char *port_p);
short mac_init(void);
short mac_avl(void);
short mac_inp(frame_t *frame_p);
void dump_raw(frame_t *frame_p);
short frame2uidata(frame_t *frame_p, uidata_t *uidata_p,char*);
short is_call(const char *c);
void dump_uidata_from(uidata_t *uidata_p);
void dump_uidata_common(uidata_t *uidata_p, distance_t distance);
int numports(void);

