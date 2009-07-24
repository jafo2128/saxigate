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

/* Most of the ax25.c and ax25.h files is code taken from Digi_ned by Henk De Groot (PE1DNN) 
 * and modified by Robbie De Lise (ON4SAX)
 * 
 * Digi_ned is an excellent APRS DigiPeater for DOS and LINUX.
 * For more info, please visit: http://www.homepages.hetnet.nl/~pe1dnn/
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
#include <netinet/in.h>

#include "ax25.h"

static ports_t lnx_port[MAX_PORTS];
static int     lnx_port_nr = 0;
static int     lnx_s_in;

int add_port(char *port_p)
{
    ports_t         *prt_p;

    if(lnx_port_nr < MAX_PORTS)
    {
        prt_p = &(lnx_port[lnx_port_nr]);

        prt_p->port_p = port_p;

        lnx_port_nr++;

        /* succesfully added */
        return 1;
    }
    else
    {
        /* Can't add more ports */
        return 0;
    }
}

int numports(void) {
	return lnx_port_nr;
}

short mac_init(void)
{
    struct sockaddr  sa;
    ports_t         *prt_p;
    short            i;

	//check for root user.
    if(geteuid() != 0)
    {
        fprintf(stderr,"Must be run as root\r\n");
        exit(-1);
    }

    /* general startup, done once */
    if (ax25_config_load_ports() == 0) {
        fprintf(stderr,"ERROR: problem with axports file\r\n");
        return 0;
    }

    /* convert axport to device and open transmitter sockets */
    for(i = 0; i < lnx_port_nr; i++)
    {
        prt_p = &(lnx_port[i]);

        if ((prt_p->dev_p = ax25_config_get_dev(prt_p->port_p)) == NULL)
        {
            fprintf(stderr,"ERROR: invalid port name - %s\r\n", prt_p->port_p);
            return 0;
        }

        /* do transmitter side */
        prt_p->s_out = socket(PF_INET, SOCK_PACKET, htons(ETH_P_AX25));
        if (prt_p->s_out < 0) 
        {
            perror("tx socket");
            return 0;
        }

        bzero(&sa,sizeof(struct sockaddr));
        strcpy(sa.sa_data, prt_p->dev_p);
        sa.sa_family = AF_INET;
        if (bind(prt_p->s_out, &sa, sizeof(struct sockaddr)) < 0)
        {
            perror("bind");
            close(prt_p->s_out);
            return 0;
        }
    }

    /* open receive socket, one receiver will do for al ports  */
    /* ON4SAX: Modified to use PF_AX25 because for saxIGate we WANT the data we sent ourselves too! */

    lnx_s_in = socket(PF_INET, SOCK_PACKET, htons(PF_AX25));
    if (lnx_s_in < 0)
    {
        perror("rx socket");
        return 0;
    }

    return 1;
}

/* AX25_MAC call to check if RX data is available */
short mac_avl(void)
{
    struct timeval tv;
    fd_set fds;
    int retval;

    /* Watch stdin (fd 0) to see when it has input. */
    FD_ZERO(&fds);
    FD_SET(lnx_s_in, &fds);

    /* wait only a short time (not 0, that gives a very high load!) */
    tv.tv_sec = 0;
    tv.tv_usec = 10;

    retval = select(lnx_s_in + 1, &fds, NULL, NULL, &tv);

    if (retval != 0)
    {
        /* data available */
        return 1;
    }

    /* no data */
    return 0;
}

/* AX25_MAC call to retrieve RX data */
short mac_inp(frame_t *frame_p)
{
    struct sockaddr  from;
    unsigned int     from_len;
    char             data[MAX_FRAME_LENGTH + 1];
    int              data_length;
    int              i;
    ports_t         *prt_p;

    from_len = sizeof(from);
    data_length = recvfrom(lnx_s_in, data, MAX_FRAME_LENGTH + 1, 0,
                                                            &from, &from_len);

    if(data_length < 0) {
        if (errno == EWOULDBLOCK)
            return -1;
        fprintf(stderr,"Error returned from \"recvfrom\" in function mac_inp()\r\n");
        exit(1);
    }

    if(data_length < 2)
        return -1; /* short packet */

    if(data[0] != 0)
        return -1; /* not a data packet */

    data_length--;

    memcpy(frame_p->frame, &(data[1]), data_length);
    frame_p->len  = data_length;

    /* find out from which port this came */
    for(i = 0; i < lnx_port_nr; i++)
    {
        prt_p      = &(lnx_port[i]);

        if(strcmp(prt_p->dev_p, from.sa_data) == 0)
        {
            /* found it! */
            frame_p->port = i;
            return 0;
        }
    }

    /* data on a port we don't use */
    return -1;
}

/* AX25_MAC call to put TX data in the MAC layer */
short mac_out(frame_t *frame_p) {
    struct sockaddr  to;
    char             data[MAX_FRAME_LENGTH + 1];
    int              res;
    ports_t         *prt_p;

    prt_p = &(lnx_port[(short) frame_p->port]);

    bzero(&to,sizeof(struct sockaddr));
    to.sa_family = AF_INET;
    strcpy(to.sa_data, prt_p->dev_p);

    data[0] = 0; /* this is data */
    memcpy(&data[1], frame_p->frame, frame_p->len);

    res = sendto(prt_p->s_out, data, frame_p->len + 1, 0, &to, sizeof(to));
    if (res >= 0)
    {
            return 0;
    }
    if (errno == EMSGSIZE) {
            //vsay("Sendto: packet (size %d) too long\r\n", frame_p->len + 1);
            return -1;
    }
    if (errno == EWOULDBLOCK) {
            //vsay("Sendto: busy\r\n");
            return -1;
    }
    perror("sendto");
    return -1;
}


void dump_raw(frame_t *frame_p) {
    short i, c;

    printf("Received from port %d\r\n", (short) frame_p->port + 1);

    for(i = 0; i < frame_p->len; i++)
    {
        c = ((short) frame_p->frame[i]) & 0x00ff;
        if(iscntrl((short) c))
        {
            if(c == '\r')
            {
                printf("\r\n");
            }
            else
            {
                printf(".");
            }
        }
        else
        {
            printf("%c", c);
        }
    }
    printf("\r\n");

    /* one more time in HEX */
    for(i = 0; i < frame_p->len; i++)
    {
        c = ((short) frame_p->frame[i]) & 0x00ff;
        printf("%02x ", c);
    }
    printf("\r\n");
}

/*
 * convert a raw AX.25 frame to a disassembled frame structure
 *
 * returns: 0 if frame could not be decoded
 *          1 if frame could be decoded and has a supported PID
 *         -1 if frame could be decoded but has an unsupported PID
 *         -2 if frame could be decoded but has an illegal call
 */
short frame2uidata(frame_t *frame_p, uidata_t *uidata_p, char *digi_digi_call)
{
    unsigned char  i,j;
    unsigned short k;
    unsigned short ndigi;
    unsigned short ssid;
    unsigned short len;
    unsigned char *bp;
    unsigned short pid;
    unsigned short call_ok;
    unsigned short hops_total;
    unsigned short hops_to_go;
    char          *p;
    
    //tmp added by on4sax to get the function running.
    short digi_kenwood_mode = 0;
    short digi_opentrac_enable = 0;
    
    /* assume all calls are okay */
    call_ok = 1;
    
    uidata_p->port = frame_p->port;
    len = frame_p->len;
    bp  = &(frame_p->frame[0]);

    if (!bp || !len) 
    {
        //printf("Short packet (no data)\r\n");
        return 0;
    }

    if (len < 15)
    {
        //printf("Short packet (< 15 bytes)\r\n");
        return 0;
    }

    if (bp[1] & 1) /* Compressed FlexNet Header */
    {
        //printf("Compressed FlexNet header in packet, not supported\r\n");
        return 0;
    }

    /* cleanup data size before beginning to decode */
    uidata_p->size = 0;

    /* Destination of frame */
    j = 0;
    for(i = 0; i < 6; i++)
    {
        if ((bp[i] &0xfe) != 0x40)
            uidata_p->destination[j++] = bp[i] >> 1;
    }
    ssid = (bp[6] & SSID_MASK) >> 1;
    if(ssid != 0)
    {
        uidata_p->destination[j++] = '-';
        if((ssid / 10) != 0)
        {
            uidata_p->destination[j++] = '1';
        }
        ssid = (ssid % 10);
        uidata_p->destination[j++] = ssid + '0';
    }
    uidata_p->dest_flags = bp[6] & FLAG_MASK;
    uidata_p->destination[j] = '\0';
    if(is_call(uidata_p->destination) == 0)
    {
        //printf("Invalid Destination call in received packet\r\n");
        call_ok = 0;
    }

    bp += 7;
    len -= 7;

    /* Source of frame */
    j = 0;
    for(i = 0; i < 6; i++) 
    {
        if ((bp[i] &0xfe) != 0x40)
            uidata_p->originator[j++] = bp[i] >> 1;
    }
    ssid = (bp[6] & SSID_MASK) >> 1;
    if(ssid != 0)
    {
            uidata_p->originator[j++] = '-';
            if((ssid / 10) != 0)
            {
                uidata_p->originator[j++] = '1';
            }
            ssid = (ssid % 10);
            uidata_p->originator[j++] = ssid + '0';
    }
    uidata_p->orig_flags = bp[6] & FLAG_MASK;
    uidata_p->originator[j] = '\0';
    if(is_call(uidata_p->originator) == 0)
    {
        //printf("Invalid Originator call in received packet\r\n");
        call_ok = 0;
    }

    bp += 7;
    len -= 7;

    /* Digipeaters */
    ndigi=0;
    while ((!(bp[-1] & END_FLAG)) && (len >= 7))
    {
        /* Digi of frame */
        if(ndigi != 8)
        {
            j = 0;
            for(i = 0; i < 6; i++)
            {
                if ((bp[i] &0xfe) != 0x40) 
                    uidata_p->digipeater[ndigi][j++] = bp[i] >> 1;
            }
            if(j == 0)
            {
                //printf("Short Digipeater call found (0 bytes length)\r\n");
                call_ok = 0;
            }
            uidata_p->digi_flags[ndigi] = (bp[6] & FLAG_MASK);
            ssid = (bp[6] & SSID_MASK) >> 1;
            if(ssid != 0)
            {
                    uidata_p->digipeater[ndigi][j++] = '-';
                    if((ssid / 10) != 0)
                    {
                        uidata_p->digipeater[ndigi][j++] = '1';
                    }
                    ssid = (ssid % 10);
                    uidata_p->digipeater[ndigi][j++] = ssid + '0';
            }
            uidata_p->digipeater[ndigi][j] = '\0';
            if(is_call(uidata_p->digipeater[ndigi]) == 0)
            {
                //printf("Invalid Digipeater call in received packet\r\n");
                call_ok = 0;
            }
            ndigi++;
        }
        bp += 7;
        len -= 7;

    }
    uidata_p->ndigi = ndigi;

    /* if at the end, short packet */
    if(!len) 
    {
        //printf("Short packet (no primitive found)\r\n");
        return 0;
    }

    /* We are now at the primitive bit */
    uidata_p->primitive = *bp;
    bp++;
    len--;
    /* Flag with 0xffff that we don't have a PID */
    uidata_p->pid = 0xffff; 

    /* if there were call decoding problems return now */
    if(call_ok == 0)
    {
        return -2;
    }

    /*
     * Determine if a packet is received LOCAL (i.e. direct) or
     * REMOTE (i.e. via a digipeater)
     */
    if(uidata_p->ndigi == 0)
    {
        /* no digipeaters at all, must be LOCAL then */
        uidata_p->distance = LOCAL;
    }
    else
    {
        /* there are digi's, look if it was repeated at least once */
        if((uidata_p->digi_flags[0] & H_FLAG) != 0)
        {
            /* this packet was digipeated and thus not received localy */
            uidata_p->distance = REMOTE;
        }
        else
        {
            /*
             * Here are the exceptions! With these exceptions no H_FLAG
             * is set on the first digipeater although the packet was
             * already digipeated one or more times.
             *
             * These are two types of digipeating use this:
             *
             * 1) Digipeating on destination-SSID. After the first hop
             * the digipeater-call of the first digipeater is added
             * without a H_FLAG. Subsequent digipeaters do not change
             * this until the SSID reaches 0. When the SSID reaches 0
             * the H_FLAG on the existing first digipeater call is set.
             *
             * Example flow, starting with destination SSID=3
             * PE1DNN>APRS-3:>Digipeating...               Direct packet
             * PE1DNN>APRS-2,PE1DNN-2:>Digipeating...      First hop
             * PE1DNN>APRS-1,PE1DNN-2:>Digipeating...      Second hop
             * PE1DNN>APRS,PE1DNN-2*:>Digipeating...       Third hop
             *
             * 2) Intelligent digipeater calls counting down or even
             * finished counting down... The H_FLAG may be set when
             * the call is completely "used", but this is not always
             * the case.
             *
             * Example flow, starting with destination WIDE3-3
             * PE1DNN>APRS,WIDE3-3:>Digipeating...         Direct packet
             * PE1DNN>APRS,WIDE3-2:>Digipeating...         First hop
             * PE1DNN>APRS,WIDE3-1:>Digipeating...         Second hop
             * PE1DNN>APRS,WIDE3*:>Digipeating...          Third hop
             * (if the packet in the third hop passed a Kantronics TNC
             *  with NOID set, then the TNC will not mark WIDE3 with a
             *  H_FLAG,i.e PE1DNN>APRS,WIDE3:>Digipeating...)
             */

            /*
             * Check if the packet is digipeating on SSID. This is the
             * case when the destination SSID is not '0'. We only need
             * to find the '-' on the destination call. With SSID = '0'
             * the '-' is not present.
             */
            p = strchr(uidata_p->destination, '-');
            if(p != NULL)
            {
                /*
                 * Destination SSID is not '0', assume digipeating on
                 * destination SSID. I.e the packet was already repeated
                 * once because there is an unused digipeater call in the
                 * digipeater list. With digipeating on destination-SSID
                 * the starting packet should not have any digipeaters at
                 * all.
                 */
                uidata_p->distance = REMOTE;
            }
            else if(strcmp(uidata_p->digipeater[0], digi_digi_call) == 0)
            {
                    /*
                     * Direct to me, must be LOCAL;
                     */
                    uidata_p->distance = LOCAL;
            }
            else
            {
                /*
                 * retrieve hops_total and hops_to_go from the first
                 * digipeater call
                 */

                /* Initialize to no hops to go anymore */
                hops_to_go = 0;

                p = strchr(uidata_p->digipeater[0], '-');
                if(p == NULL)
                {
                    /*
                     * "hops_to_go" was already initialized to zero
                     *
                     * put 'p' at the last digit of the call
                     * We checked already above that the call is at least 1
                     * character in size.
                     */
                    p = uidata_p->digipeater[0];
                    p = &(p[strlen(p) - 1]);
                }
                else
                {
                    /*
                     * Retrieve hops_to_go from the SSID
                     *
                     * p[0] points to the '-'
                     * p[1] points to the first SSID character
                     * p[2] points to '\0' or the second SSID character
                     *      if there is a second character the first
                     *      SSID character is always '1'.
                     *      (SSID can be 1..15; when 0 it is no shown)
                     */
                    if((p[1] >= '0') && (p[1] <= '9'))
                    {
                        /* Keep this value */
                        hops_to_go = (short)(p[1] - '0');

                        /*
                         * Since the value was not '\0' it is save to
                         * check the next character. This is only needed
                         * if the first character is '1'
                         */
                        if(p[1] == '1')
                        {
                            if((p[2] >= '0') && (p[2] <= '9'))
                            {
                                /*
                                 * The first SSID character meant 10, the
                                 * number of hops_to_go is 10 plus the
                                 * second SSID character
                                 */
                                hops_to_go = 10 + (short)(p[2] - '0');
                            }
                        }
                    }

                    /*
                     * put 'p' at the last digit of the call
                     * We checked already  above that the call is at least 1
                     * character in size.
                     */
                    p--;
                }

                /*
                 * Retrieve hops_total, if any. Initialize to 15 before
                 * start so intelligent calls without a total hopcount
                 * are regarded as not being the first unless the SSID=15.
                 *
                 * If the digipeater does not have a SSID then initialize to
                 * zero.
                 */
                if (hops_to_go == 0)
                {
                    hops_total = 0;
                }
                else
                {
                    hops_total = 15;
                }

                if((*p >= '0') && (*p <= '9'))
                {
                    /* Keep this value */
                    hops_total = (short)(*p - '0');

                    /*
                     * Check last but one character if the last digit
                     * was smaller than 6. Check if the pointer doesn't
                     * move before the start of the call.
                     */
                    p--;
                    if( (uidata_p->digipeater[0] <= p)
                        &&
                        (hops_total < 6)
                      )
                    {
                        if(*p == '1')
                        {
                            /*
                             * Take this '1' into account, the total hop
                             * count is 10 more.
                             */
                            hops_total = hops_total + 10;
                        }
                    }
                }

                /*
                 * We collected hops_total and hops_to_go.
                 *
                 * Check if it is an "Intelligent" digi-call which is in
                 * progress counting down or is finished.
                 */
                if(hops_total > hops_to_go)
                {
                    /*
                     * The hops_total is higher than hops_to_go, this is
                     * assumed to be an "Intelligent" call busy counting
                     * down.
                     */
                    uidata_p->distance = REMOTE;
                }
                else
                {
                    /*
                     * The hops_to_go count has a higher or equal number 
                     * than hops_total. This is the case when it is an
                     * "Intelligent" call that did not start count-down
                     * yet or when the SSID is higher than the last digit(s)
                     * of the call (no "Intelligent" digipeating used).
                     *
                     * In these cases the packet must be direct.
                     */
                    uidata_p->distance = LOCAL;
                }
            }
        }
    }

    /* No data left, ready */
    if (!len)
    {
        /* this is not an UI frame, in kenwood_mode it should be ignored */
        if(digi_kenwood_mode == 0)
        {
            return 1;
        }
        else
        {
            return -1;
        }
    }

    /* keep the PID */
    pid = ((short) *bp) & 0x00ff; 
    uidata_p->pid = pid;
    bp++;
    len--;

    k = 0;
    while (len)
    {
        i = *bp++;

        if(k < 256) uidata_p->data[k++] = i;

        len--;
    }
    uidata_p->size = k;

    if(call_ok == 0)
    {
        return -2;
    }

    if(digi_kenwood_mode == 0)
    {
        /* check PID */
        if( (pid != 0xf0) /* normal AX.25 */
            &&
            (pid != 0xcc) /* IP datagram */
            &&
            (pid != 0xcd) /* ARP */
            &&
            (pid != 0xcf) /* NETROM */
            &&
            ((digi_opentrac_enable == 0) || (pid != 0x77)) /* Opentrac */
          )
        {
            /*
             * only support for AX.25, IP, ARP, NETROM and Opentrac
             * packets, bail out on others
             */
            return -1;
        }
    }
    else
    {
        /* only support for AX.25 UI frames */
        if((uidata_p->primitive & ~0x10) != 0x03)
        {
            /* not a normal AX.25 UI frame, bail out */
            return -1;
        }
        else
        {
            /*
             * AX.25 UI frame, check for PID=F0 and
             * PID=77 if Opentrac is enabled
             */

            /* check PID */
            if( (pid != 0xf0) /* normal AX.25 */
                &&
                ((digi_opentrac_enable == 0) || (pid != 0x77)) /* Opentrac */
              )
            {
                /*
                 * only support for AX.25, IP, ARP, NETROM and Opentrac
                 * packets, bail out
                 */
                return -1;
            }
        }
    }

    return 1;
}

/*
 * convert a disassembled frame structure to a raw AX.25 frame 
 */
void uidata2frame(uidata_t *uidata_p, frame_t *frame_p) {
    unsigned char  i,j;
    unsigned short k;
    unsigned short ssid;
    unsigned short len;
    unsigned char *bp;

    frame_p->port = uidata_p->port;

    bp = &(frame_p->frame[0]);

    len = 0; /* begin of frame */

    /* Destination of frame */
    j = 0;
    for(i = 0; i < 6; i++)
    {
        /* if not end of string or at SSID mark */
        if((uidata_p->destination[j] != '\0')
           &&
           (uidata_p->destination[j] != '-'))
        {
            bp[i] = (uidata_p->destination[j++] << 1);
        }
        else
        {
            bp[i] = 0x40;
        }
    }
    if(uidata_p->destination[j] == '-')
    {
        /* "j" points to the SSID mark */
        j++;
        
        ssid = uidata_p->destination[j++] - '0';
        if(uidata_p->destination[j] != '\0')
        {
            ssid = (10 * ssid) + (uidata_p->destination[j++] - '0');
        }
    }
    else
    {
        ssid = 0;
    }
    bp[6] = (ssid << 1) | uidata_p->dest_flags;
    bp += 7;
    len += 7;

    /* Source of frame */
    j = 0;
    for(i = 0; i < 6; i++)
    {
        /* if not end of string or at SSID mark */
        if((uidata_p->originator[j] != '\0')
           &&
           (uidata_p->originator[j] != '-'))
        {
            bp[i] = (uidata_p->originator[j++] << 1);
        }
        else
        {
            bp[i] = 0x40;
        }
    }
    if(uidata_p->originator[j] == '-')
    {
        /* "j" points to the SSID mark */
        j++;

        ssid = uidata_p->originator[j++] - '0';
        if(uidata_p->originator[j] != '\0')
        {
            ssid = (10 * ssid) + (uidata_p->originator[j++] - '0');
        }
    }
    else
    {
        ssid = 0;
    }
    bp[6] = (ssid << 1) | uidata_p->orig_flags;
    bp += 7;
    len += 7;

    /* Digipeaters */
    for(k = 0; k < uidata_p->ndigi; k++)
    {
        /* check if the distribution distance should be LOCAL */
        if( uidata_p->distance == LOCAL )
        {
            /*
             * to keep the frame local avoid that it is digipeated after
             * we transmitted it, there shall not be any unused digipeater
             * in the frame. break out of the loop if an unused digipeater
             * is encountered
             *
             * an unused digipeater doesn't have its H_BIT set in the SSID
             */
            if((uidata_p->digi_flags[k] & H_FLAG) == 0)
            {
                /* don't put "unused" digipeaters in the frame */
                break; /* break out of the for loop */
            }
        }

        j = 0;
        for(i = 0; i < 6; i++)
        {
            /* if not end of string or at SSID mark */
            if((uidata_p->digipeater[k][j] != '\0')
               &&
               (uidata_p->digipeater[k][j] != '-'))
            {
                bp[i] = (uidata_p->digipeater[k][j++] << 1);
            }
            else
            {
                bp[i] = 0x40;
            }
        }
        if(uidata_p->digipeater[k][j] == '-')
        {
            /* "j" points to the SSID mark */
            j++;

            ssid = uidata_p->digipeater[k][j++] - '0';
            if(uidata_p->digipeater[k][j] != '\0')
            {
                ssid = (10 * ssid) + (uidata_p->digipeater[k][j++] - '0');
            }
        }
        else
        {
            ssid = 0;
        }
        bp[6] = (ssid << 1) | uidata_p->digi_flags[k];
        bp += 7;
        len += 7;
    }

    /* patch bit 1 on the last address */
    bp[-1] = bp[-1] | END_FLAG;

    /* We are now at the primitive bit */
    *bp = uidata_p->primitive;
    bp++;
    len++;

    /* if PID == 0xffff we don't have a PID */
    if(uidata_p->pid == 0xffff)
    {
        frame_p->len = len;
        return;
    }

    /* set the PID */
    *bp = (char) uidata_p->pid;
    bp++;
    len++;

    /* when size == 0 don't copy */
    if(uidata_p->size == 0)
    {
        frame_p->len = len;
        return;
    }

    memcpy(bp, &(uidata_p->data[0]), uidata_p->size);
    len += uidata_p->size;

    frame_p->len = len;

    return;
}

/*
 * Check if the input string is a valid call
 *
 * Returns: 0 if not okay
 *          1 if okay
 */
short is_call(const char *c)
{
    short n;

    if(*c == '\0')
    {
        /* Missing call */
        return 0;
    }

    /* call-size counter to 0 */
    n = 0;

    do
    {
        /* count call size */
        n++;

        if(n == 7)
        {
            /* Illegal call length */
            return 0;
        }

        /*
         * Before the dash can be either a digit
         * or an uppercase alpha character
         */
        if(!isdigit(*c) && !isupper(*c))
        {
            /* Illegal character in call */
            return 0;
        }

        /* advance to next character */
        c++;

        /* if end of string, checking completed successfully */
        if(*c == '\0')
        {
            /* okay, call without SSID */
            return 1;
        }
    } while(*c != '-');

    /* if we end up here then a '-' was found */

    /* advance to first character of SSID */
    c++;

    if(*c == '\0')
    {
        /* Missing SSID */
        return 0;
    }

    if(*c == '1')
    {
        /* can be a 2 digit SSID */

        /* next character can be '\0' */
        c++;

        if(*c == '\0')
        {
            /* SSID is '1', okay */
            return 1;
        }

        /* remaining character shall be 0..5 */
        if((*c < '0') || (*c > '5'))
        {
            /* Impossible digit value */
            return 0;
        }
    }
    else
    {
        /* single digit SSID */
        if(!isdigit(*c))
        {
            /* Illegal character in SSID */
            return 0;
        }
    }

    /* next character shall be '\0' */
    c++;

    if(*c != '\0')
    {
        /* characters following the SSID */
        return 0;
    }
    else
    {
        /* checking completed successfully */
        return 1;
    }
}

void dump_uidata_from(uidata_t *uidata_p, short verbose)
{
    char          stamp[MAX_MESSAGE_LENGTH + 1];
    struct tm    *t;
    time_t        now;

    /* show timestamp */
    now = time(NULL);
    tzset();
    t = localtime(&now);
    strftime(stamp, MAX_MESSAGE_LENGTH- 1, "[%b %d %Y - %H:%M:%S]", t);
    if (verbose) printf("%s",stamp);

    if (verbose) printf("\r\nfrom:%d ", (short) uidata_p->port + 1);

    dump_uidata_common(uidata_p, REMOTE, verbose);

    if(uidata_p->distance == LOCAL && verbose)
    {
        printf("This is a local station\r\n");
    }
} 

void dump_uidata_to(char *to, uidata_t *uidata_p)
{
    printf("to:%s ", to);
    dump_uidata_common(uidata_p, REMOTE, 1);
}

void dump_uidata_common(uidata_t *uidata_p, distance_t distance, short verbose)
{
    short         i;
    short         did_digi;
    unsigned char c;

    if (verbose) printf("%s > %s", uidata_p->originator, uidata_p->destination);

    /* remember if we already displayed a digi, used for " via" text */
    did_digi = 0;

    for(i = 0; i < uidata_p->ndigi; i++)
    {
        /* dump digi:
         *
         * When distance is REMOTE all digis are dumped, otherwise
         * only digis with the H_FLAG set are dumped.
         */
        if( (distance == REMOTE)
            ||
            ((uidata_p->digi_flags[i] & H_FLAG) != 0)
          )
        {
            if(did_digi == 0)
            {
                /* first digi in output, add " via" text */
                if (verbose) printf(" via");
                did_digi = 1;
            }
            if (verbose) printf(" %s", uidata_p->digipeater[i]);
            if((uidata_p->digi_flags[i] & H_FLAG) != 0)
            {
                if (verbose) printf("*");
            }
            if((uidata_p->digi_flags[i] & RR_FLAG) != RR_FLAG)
            {
                if (verbose) printf("!");
            }
        }
    }

    /* get primitive in temp variable, strip poll bit */
    i = uidata_p->primitive & ~0x10;

    if((i & 0x01) == 0)
    {
        /* I frame */
        if (verbose) printf(" I%d,%d", (i >> 5) & 0x07, (i >> 1) & 0x07);
    }
    else if((i & 0x02) == 0)
    {
        /* S frame */
        switch((i >> 2) & 0x03) {
        case 0:
                if (verbose) printf(" RR");
                break;
        case 1:
                if (verbose) printf(" RNR");
                break;
        case 2:
                if (verbose) printf(" REJ");
                break;
        default:
                if (verbose) printf(" ???");
                break;
        }
        if (verbose) printf("%d", (i >> 5) & 0x07);
    }
    else
    {
        /* U frame */
        switch((i >> 2) & 0x3f) {
        case 0x00:
                if (verbose) printf(" UI");
                break;
        case 0x03:
                if (verbose) printf(" DM");
                break;
        case 0x0B:
                if (verbose) printf(" SABM");
                break;
        case 0x10:
                if (verbose) printf(" DISC");
                break;
        case 0x18:
                if (verbose) printf(" UA");
                break;
        case 0x21:
                if (verbose) printf(" FRMR");
                break;
        default:
                if (verbose) printf(" ???");
                break;
        }
    }

    if((uidata_p->primitive & 0x10) == 0)
    {
        if((uidata_p->dest_flags & C_FLAG) != 0) {
            if (verbose) printf("^");
        } else {
            if (verbose) printf("v");
        }
    }
    else
    {
        if((uidata_p->dest_flags & C_FLAG) != 0) {
            if (verbose) printf("+");
        } else {
            if (verbose) printf("-");
        }
    }

    /* if we have a PID */
    if(uidata_p->pid != 0xffff)
        if (verbose) printf(" PID=%X", ((short) uidata_p->pid) & 0x00ff);

    if(uidata_p->size > 0)
    {
        if (verbose) printf(" %d bytes\r\n", uidata_p->size);

        for(i = 0; i < uidata_p->size; i++)
        {
            c = uidata_p->data[i];
            if(iscntrl((short) c))
            {
                if(c == '\r')
                {
                    if (verbose) printf("\r\n");
                }
                else
                    if (verbose) printf(".");
            }
            else
            {
                if (verbose) printf("%c", c);
            }
        }
    }
    if (verbose) printf("\r\n");
}


