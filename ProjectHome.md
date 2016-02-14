# This project is no longer maintained! Please use the more mature and better APRX software! http://wiki.ham.fi/Aprx.en #

saxIgate is a small APRS I-Gate for Linux. It reads APRS (AX25-UI) data from the kernel drivers (RF) and pushes it over a TCP/IP connection to an APRS-IS server of your choice.

Since the kernel handles the hardware, you can use a TNC, Baycom, Soundcard, or anything the linux kernel supports to receive the data from the air.

**CHANGELOG**:

_v0.1.7:
  * Fixed a bug that would crash the app when a port was specified. Thx to OK1TEB for notifying me.
  * Cleaned up code in main.c and distributed it over igate.c and msggate.c, for a cleaner approach.
  * DNS error will no longer kill the application._

_v0.1.6:_
  * saxIgate now runs as a deamon when not in verbose mode.
  * Cleaned up #includes to get rid of 'Implicit Declaration' warnings when compiling.
  * saxIgate sends out an ID beacon on connect over TCP/IP only (not on RF).

_v0.1.5:_
  * Fixed some memory leaks that caused crash after a few days.
  * Fixed bug that would truncate digipath on some systems.

_v0.1.4:_
  * Program now reconnects on disconnect instead of exiting.
  * Caching system added to prevent double data to be send after a digipeater re-airs it.

_v0.1.3:_
  * Initial release to public (google code) under the GNU License v3.
  * Code cleanup and added comments for my own sake :p I tend to forget what something is for when a project is in the fridge to long.
  * Using STD\_ERR for error messages.
  * Actually using the Verbose flag (-v). No output to STD\_OUT if omitted.
  * Checking for -p, -c and -s flags! These are required.
  * Fixed bug that would give a Segmentation Fault when NOT running in verbose mode.

_v0.1.2:_
  * Added code to connect, login and send data to APRS-IS. Tested with my own APRS-IS server T2BELGIUM running javAPRSSrvr 3.14b05 with 2 live APRS digi's (ON0DAS-4 & ON0BAF-4)
  * Modified ax25 code to also read packages that are locally transmitted (for digi beacons etc)

_v0.1.1:_
  * Added callpass generator (code from Xastir) and formatting of ax25 data for I-gate transmission.

_v0.1.0:_
  * Initial code to read ax25 messages from the kernel. Code copied from the digi\_ned project and modified to suit saxIgate needs.
