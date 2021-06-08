// view1090, a Mode S messages viewer for dump1090 devices.
//
// Copyright (C) 2014 by Malcolm Robb <Support@ATTAvionics.com>
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//  *  Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
//
//  *  Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//#include "coaa.h"
#include "view1090.h"
//
// ============================= Utility functions ==========================
//
void sigintHandler(int dummy) {
    NOTUSED(dummy);
    signal(SIGINT, SIG_DFL);  // reset signal handler - bit extra safety
    Modes.exit = 1;           // Signal to threads that we are done
}

static uint64_t mstime(void) {
    struct timeval tv;
    uint64_t mst;

    gettimeofday(&tv, NULL);
    mst = ((uint64_t)tv.tv_sec)*1000;
    mst += tv.tv_usec/1000;
    return mst;
}
//
// =============================== Terminal handling ========================
//
#ifndef _WIN32
// Get the number of rows after the terminal changes size.
int getTermRows() { 
    struct winsize w; 
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w); 
    return (w.ws_row); 
} 

// Handle resizing terminal
void sigWinchCallback() {
    signal(SIGWINCH, SIG_IGN);
    Modes.interactive_rows = getTermRows();
    interactiveShowData();
    signal(SIGWINCH, sigWinchCallback); 
}
#else 
int getTermRows() { return MODES_INTERACTIVE_ROWS;}
#endif
//
// =============================== Initialization ===========================
//
void view1090InitConfig(void) {
    // Default everything to zero/NULL
    memset(&Modes,    0, sizeof(Modes));
    memset(&View1090, 0, sizeof(View1090));

    // Now initialise things that should not be 0/NULL to their defaults
    Modes.check_crc               = 1;
    strcpy(View1090.net_input_beast_ipaddr,VIEW1090_NET_OUTPUT_IP_ADDRESS); 
    Modes.net_input_beast_port    = MODES_NET_OUTPUT_BEAST_PORT;
    Modes.interactive_rows        = 80; //getTermRows();
    Modes.interactive_delete_ttl  = MODES_INTERACTIVE_DELETE_TTL;
    Modes.interactive_display_ttl = MODES_INTERACTIVE_DISPLAY_TTL;
    Modes.fUserLat                = MODES_USER_LATITUDE_DFLT;
    Modes.fUserLon                = MODES_USER_LONGITUDE_DFLT;

    Modes.mode_ac                 = 0;
    Modes.interactive             = 1;
}
//
//=========================================================================
//
void view1090Init(void) {

    pthread_mutex_init(&Modes.pDF_mutex,NULL);
    pthread_mutex_init(&Modes.data_mutex,NULL);
    pthread_cond_init(&Modes.data_cond,NULL);

#ifdef _WIN32
    if ( (!Modes.wsaData.wVersion) 
      && (!Modes.wsaData.wHighVersion) ) {
      // Try to start the windows socket support
      if (WSAStartup(MAKEWORD(2,1),&Modes.wsaData) != 0) 
        {
        fprintf(stderr, "WSAStartup returned Error\n");
        }
      }
#endif

    // Allocate the various buffers used by Modes
    if ( NULL == (Modes.icao_cache = (uint32_t *) malloc(sizeof(uint32_t) * MODES_ICAO_CACHE_LEN * 2)))
    {
        fprintf(stderr, "Out of memory allocating data buffer.\n");
        exit(1);
    }

    // Clear the buffers that have just been allocated, just in-case
    memset(Modes.icao_cache, 0,   sizeof(uint32_t) * MODES_ICAO_CACHE_LEN * 2);

    // Validate the users Lat/Lon home location inputs
    if ( (Modes.fUserLat >   90.0)  // Latitude must be -90 to +90
      || (Modes.fUserLat <  -90.0)  // and 
      || (Modes.fUserLon >  360.0)  // Longitude must be -180 to +360
      || (Modes.fUserLon < -180.0) ) {
        Modes.fUserLat = Modes.fUserLon = 0.0;
    } else if (Modes.fUserLon > 180.0) { // If Longitude is +180 to +360, make it -180 to 0
        Modes.fUserLon -= 360.0;
    }
    // If both Lat and Lon are 0.0 then the users location is either invalid/not-set, or (s)he's in the 
    // Atlantic ocean off the west coast of Africa. This is unlikely to be correct. 
    // Set the user LatLon valid flag only if either Lat or Lon are non zero. Note the Greenwich meridian 
    // is at 0.0 Lon,so we must check for either fLat or fLon being non zero not both. 
    // Testing the flag at runtime will be much quicker than ((fLon != 0.0) || (fLat != 0.0))
    Modes.bUserFlags &= ~MODES_USER_LATLON_VALID;
    if ((Modes.fUserLat != 0.0) || (Modes.fUserLon != 0.0)) {
        Modes.bUserFlags |= MODES_USER_LATLON_VALID;
    }

    // Prepare error correction tables
    modesInitErrorInfo();
}

// Set up data connection
int setupConnection(struct client *c) {
    int fd;

    // Try to connect to the selected ip address and port. We only support *ONE* input connection which we initiate.here.
    if ((fd = anetTcpConnect(Modes.aneterr, View1090.net_input_beast_ipaddr, Modes.net_input_beast_port)) != ANET_ERR) {
		anetNonBlock(Modes.aneterr, fd);
		//
		// Setup a service callback client structure for a beast binary input (from dump1090)
		// This is a bit dodgy under Windows. The fd parameter is a handle to the internet
		// socket on which we are receiving data. Under Linux, these seem to start at 0 and 
		// count upwards. However, Windows uses "HANDLES" and these don't nececeriy start at 0.
		// dump1090 limits fd to values less than 1024, and then uses the fd parameter to 
		// index into an array of clients. This is ok-ish if handles are allocated up from 0.
		// However, there is no gaurantee that Windows will behave like this, and if Windows 
		// allocates a handle greater than 1024, then dump1090 won't like it. On my test machine, 
		// the first Windows handle is usually in the 0x54 (84 decimal) region.

		c->next    = NULL;
		c->buflen  = 0;
		c->fd      = 
		c->service =
		Modes.bis  = fd;
		Modes.clients = c;

        if (Modes.mode_ac) {
            send(fd, "\0321J", 3, 0);
        } else {
            send(fd, "\0321j", 3, 0);
        }
    }
    return fd;
}
//
// ================================ Main ====================================
//
// Show the currently captured interactive data on screen.
//
static unsigned uModeZFrames = 0;
static unsigned uModeOFrames = 0;
static unsigned uModeAFrames = 0;
static unsigned uModeCFrames = 0;
static unsigned uModeSSpuriousFrames = 0;
static unsigned uModeASpuriousOneFrames = 0;
static unsigned uModeASpuriousZeroFrames = 0;
static unsigned uModeCSpuriousOneFrames = 0;
static unsigned uModeCSpuriousZeroFrames = 0;
//static unsigned uModeSDuplicateFrames = 0;
//static unsigned uModeSOverlaidFrames = 0;

char strOut[4096];
char * p;

void mainShowData(void) {
    struct aircraft *a = Modes.aircrafts;
    time_t now = time(NULL);
    int count = 0;
    char progress;
    char spinner[4] = "|/-\\";

    unsigned uAllFrames, uAltitudeFrames, uSquawkFrames;
    unsigned uModeACFrames;

    // Refresh screen every (MODES_INTERACTIVE_REFRESH_TIME) miliseconde
    if ((mstime() - Modes.interactive_last_update) < MODES_INTERACTIVE_REFRESH_TIME)
       {return;}

    Modes.interactive_last_update = mstime();

    // Attempt to reconsile any ModeA/C with known Mode-S
    // We can't condition on Modes.modeac because ModeA/C could be comming
    // in from a raw input port which we can't turn off.
    interactiveUpdateAircraftModeS();

    progress = spinner[time(NULL)%4];
	p = strOut;

#ifndef _WIN32
    p+=sprintf(p,"\x1b[H\x1b[2J");    // Clear the screen
#endif

    p += sprintf(p, "Hex     Mode  Sqwk  Flight   Alt    Spd  Hdg    Lat      Long   Sig  Msgs   Ti%c\n", progress);
    p += sprintf(p, "-------------------------------------------------------------------------------\n");

    uAllFrames = Modes.DFCount[ 0] + Modes.DFCount[ 1] + Modes.DFCount[ 2] + Modes.DFCount[ 3]
               + Modes.DFCount[ 4] + Modes.DFCount[ 5] + Modes.DFCount[ 6] + Modes.DFCount[ 7]
               + Modes.DFCount[ 8] + Modes.DFCount[ 9] + Modes.DFCount[10] + Modes.DFCount[11]
               + Modes.DFCount[12] + Modes.DFCount[13] + Modes.DFCount[14] + Modes.DFCount[15]
               + Modes.DFCount[16] + Modes.DFCount[17] + Modes.DFCount[18] + Modes.DFCount[19]
               + Modes.DFCount[20] + Modes.DFCount[21] + Modes.DFCount[22] + Modes.DFCount[23]
               + Modes.DFCount[24] + Modes.DFCount[25] + Modes.DFCount[26] + Modes.DFCount[27]
               + Modes.DFCount[28] + Modes.DFCount[29] + Modes.DFCount[30] + Modes.DFCount[31];
    uAltitudeFrames = Modes.DFCount[0] + Modes.DFCount[4] + Modes.DFCount[16] + Modes.DFCount[20];
    uSquawkFrames = Modes.DFCount[5] + Modes.DFCount[21];
    uModeACFrames = Modes.DFCount[5] + Modes.DFCount[32];

    while(a) {

        int flags = a->modeACflags;
        if (flags & MODEAC_MSG_FLAG) {

            if (flags & MODEAC_MSG_MODES_HIT) {

                if (flags & MODEAC_MSG_MODEC_HIT) {
                    uModeCFrames += a->messages;
                }
                if (flags & MODEAC_MSG_MODEA_HIT) {
                    uModeAFrames += a->messages;
                }

            } else if (a->modeA == 0) {
                uModeZFrames += a->messages;

            } else if ((a->modeA == 0x0001) || (a->modeA == 0x0002) || (a->modeA == 0x0004) || (a->modeA == 0x0008)
                    || (a->modeA == 0x0010) || (a->modeA == 0x0020) || (a->modeA == 0x0040) || (a->modeA == 0x0080)
                    || (a->modeA == 0x0100) || (a->modeA == 0x0200) || (a->modeA == 0x0400) || (a->modeA == 0x0800)
                    || (a->modeA == 0x1000) || (a->modeA == 0x2000) || (a->modeA == 0x4000) || (a->modeA == 0x8000) ){

                uModeOFrames += a->messages;

            } else {
                unsigned uOneBitError;
                
                uOneBitError = interactiveUpdateAircraftModeAOneBitError(a);
                if (uOneBitError == 0) {

                  uOneBitError = interactiveUpdateAircraftModeCOneBitError(a);
                  if      (uOneBitError == 0)        {uModeSSpuriousFrames     += a->messages;}
                  else if (uOneBitError & a->modeC)  {uModeCSpuriousOneFrames  += a->messages;}
                  else                               {uModeCSpuriousZeroFrames += a->messages;}
                }
                else if (uOneBitError & a->modeA) {uModeASpuriousOneFrames  += a->messages;}
                else                              {uModeASpuriousZeroFrames += a->messages;}

            } 
            a->messages = 0;
        }
        a = a->next;
    }

    p+=sprintf(p,"%8d ModeS  Frames (All)    (DF0 =%d, DF1 =%d, DF2 =%d, DF3 =%d)\n", uAllFrames, Modes.DFCount[0],Modes.DFCount[1],Modes.DFCount[2],Modes.DFCount[3]);
    p+=sprintf(p,"                                (DF4 =%d, DF5 =%d, DF6 =%d, DF7 =%d)\n",             Modes.DFCount[4],Modes.DFCount[5],Modes.DFCount[6],Modes.DFCount[7]);
    p+=sprintf(p,"                                (DF8 =%d, DF9 =%d, DF10=%d, DF11=%d)\n",           Modes.DFCount[8],Modes.DFCount[9],Modes.DFCount[10],Modes.DFCount[11]);
    p+=sprintf(p,"                                (DF12=%d, DF13=%d, DF14=%d, DF15=%d)\n",         Modes.DFCount[12],Modes.DFCount[13],Modes.DFCount[14],Modes.DFCount[15]);
    p+=sprintf(p,"                                (DF16=%d, DF17=%d, DF18=%d, DF19=%d)\n",         Modes.DFCount[16],Modes.DFCount[17],Modes.DFCount[18],Modes.DFCount[19]);
    p+=sprintf(p,"                                (DF20=%d, DF21=%d, DF22=%d, DF23=%d)\n",         Modes.DFCount[20],Modes.DFCount[21],Modes.DFCount[22],Modes.DFCount[23]);
    p+=sprintf(p,"                                (DF24=%d, DF25=%d, DF26=%d, DF27=%d)\n",         Modes.DFCount[24],Modes.DFCount[25],Modes.DFCount[26],Modes.DFCount[27]);
    p+=sprintf(p,"                                (DF28=%d, DF29=%d, DF30=%d, DF31=%d)\n",         Modes.DFCount[28],Modes.DFCount[29],Modes.DFCount[30],Modes.DFCount[31]);
    p+=sprintf(p,"\n");
    p+=sprintf(p,"%8d ModeS  Altitude Frames (DF0=%d, DF4 =%d, DF16=%d, DF20=%d)\n", uAltitudeFrames, Modes.DFCount[0],Modes.DFCount[4],Modes.DFCount[16],Modes.DFCount[20]);
    p+=sprintf(p,"%8d ModeS  Squawk Frames   (DF5=%d, DF21=%d)\n", uSquawkFrames, Modes.DFCount[5],Modes.DFCount[21]);
    p+=sprintf(p,"%8d ModeS  Short Duplicates\n", Modes.uModeSShortDuplicateFrames);
    p+=sprintf(p,"%8d ModeS  Short Overlapping\n", Modes.uModeSShortOverlaidFrames);
    p+=sprintf(p,"%8d ModeS  Long Duplicates\n", Modes.uModeSLongDuplicateFrames);
    p+=sprintf(p,"%8d ModeS  Long Overlapping\n", Modes.uModeSLongOverlaidFrames);
    p+=sprintf(p,"\n");
    p+=sprintf(p,"%8d ModeAC Frames (All)\n", uModeACFrames);
    p+=sprintf(p,"%8d ModeA  Frames (Matching ModeS Squawk)\n", uModeAFrames);
    p+=sprintf(p,"%8d ModeC  Frames (Matching ModeS Altitude)\n", uModeCFrames);
    p+=sprintf(p,"%8d ModeAC Frames (Duplicates)\n", Modes.uModeACDuplicateFrames);
    p+=sprintf(p,"%8d ModeAC Frames (Overlapping)\n", Modes.uModeACOverlaidFrames);
    p+=sprintf(p,"%8d ModeAC Frames (Interleaved)\n", Modes.uModeACInterleavedFrames);
    p+=sprintf(p,"%8d ModeAC Frames (XX0000)\n", uModeZFrames);
    p+=sprintf(p,"%8d ModeAC Frames (One Bit Set)\n", uModeOFrames);
    p+=sprintf(p,"%8d ModeA  Frames (One Bit Zero Errors)\n", uModeASpuriousZeroFrames);
    p+=sprintf(p,"%8d ModeA  Frames (One Bit One Errors)\n", uModeASpuriousOneFrames);
    p+=sprintf(p,"%8d ModeC  Frames (One Bit Zero Errors)\n", uModeCSpuriousZeroFrames);
    p+=sprintf(p,"%8d ModeC  Frames (One Bit One Errors)\n", uModeCSpuriousOneFrames);
    p+=sprintf(p,"%8d ModeAC Frames (Matching Nothing)\n", uModeSSpuriousFrames);
    p+=sprintf(p,"\n");

#ifdef _WIN32
    cls();
#endif
	printf("%s", strOut);
}
//
//=========================================================================
//
void showHelp(void) {
    printf(
"-----------------------------------------------------------------------------\n"
"|                        view1090 dump1090 Viewer        Ver : "MODES_DUMP1090_VERSION " |\n"
"-----------------------------------------------------------------------------\n"
  "--interactive            Interactive mode refreshing data on screen\n"
  "--interactive-rows <num> Max number of rows in interactive mode (default: 15)\n"
  "--interactive-ttl <sec>  Remove from list if idle for <sec> (default: 60)\n"
  "--interactive-rtl1090    Display flight table in RTL1090 format\n"
  "--modeac                 Enable decoding of SSR modes 3/A & 3/C\n"
  "--net-bo-ipaddr <IPv4>   TCP Beast output listen IPv4 (default: 127.0.0.1)\n"
  "--net-bo-port <port>     TCP Beast output listen port (default: 30005)\n"
  "--lat <latitude>         Reference/receiver latitide for surface posn (opt)\n"
  "--lon <longitude>        Reference/receiver longitude for surface posn (opt)\n"
  "--no-crc-check           Disable messages with broken CRC (discouraged)\n"
  "--help                   Show this help\n"
    );
}

#ifdef _WIN32
void showCopyright(void) {
    uint64_t llTime = time(NULL) + 1;

    printf(
"-----------------------------------------------------------------------------\n"
"|                    spurious1090 ModeS Viewer           Ver : " MODES_DUMP1090_VERSION " |\n"
"-----------------------------------------------------------------------------\n"
"\n"
" Copyright (C) 2012 by Salvatore Sanfilippo <antirez@gmail.com>\n"
" Copyright (C) 2014 by Malcolm Robb <support@attavionics.com>\n"
" Copyright (C) 2021 by Malcolm Robb <support@attavionics.com>\n"
"\n"
" All rights reserved.\n"
"\n"
" THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS\n"
" ""AS IS"" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT\n"
" LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR\n"
" A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT\n"
" HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,\n"
" SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT\n"
" LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,\n"
" DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY\n"
" THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n"
" (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n"
" OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n"
"\n"
" For further details refer to <https://github.com/MalcolmRobb/dump1090>\n" 
"\n"
    );

  // delay for 1 second to give the user a chance to read the copyright
  while (llTime >= time(NULL)) {}
}
#endif
//
//=========================================================================
//
int main(int argc, char **argv) {
    int j, fd;
    struct client *c;
    char pk_buf[8];

    // Set sane defaults

    view1090InitConfig();
    signal(SIGINT, sigintHandler); // Define Ctrl/C handler (exit program)

    // Parse the command line options
    for (j = 1; j < argc; j++) {
        int more = ((j + 1) < argc); // There are more arguments

        if        (!strcmp(argv[j],"--net-bo-port") && more) {
            Modes.net_input_beast_port = atoi(argv[++j]);
        } else if (!strcmp(argv[j],"--net-bo-ipaddr") && more) {
            strcpy(View1090.net_input_beast_ipaddr, argv[++j]);
        } else if (!strcmp(argv[j],"--onlyaddr")) {
            Modes.onlyaddr = 1;
        } else if (!strcmp(argv[j],"--modeac")) {
            Modes.mode_ac = 1;
        } else if (!strcmp(argv[j],"--interactive-rows") && more) {
            Modes.interactive_rows = atoi(argv[++j]);
        } else if (!strcmp(argv[j],"--raw")) {
            Modes.raw = 1;
        } else if (!strcmp(argv[j],"--interactive")) {
            Modes.interactive = 1;
        } else if (!strcmp(argv[j],"--list")) {
            Modes.interactive = 0;
        } else if (!strcmp(argv[j],"--interactive-ttl") && more) {
            Modes.interactive_display_ttl = atoi(argv[++j]);
        } else if (!strcmp(argv[j],"--interactive-rtl1090")) {
            Modes.interactive = 1;
            Modes.interactive_rtl1090 = 1;
        } else if (!strcmp(argv[j],"--lat") && more) {
            Modes.fUserLat = atof(argv[++j]);
        } else if (!strcmp(argv[j],"--lon") && more) {
            Modes.fUserLon = atof(argv[++j]);
        } else if (!strcmp(argv[j],"--metric")) {
            Modes.metric = 1;
        } else if (!strcmp(argv[j],"--no-crc-check")) {
            Modes.check_crc = 0;
        } else if (!strcmp(argv[j],"--fix")) {
            Modes.nfix_crc = 1;
        } else if (!strcmp(argv[j],"--no-fix")) {
            Modes.nfix_crc = 0;
        } else if (!strcmp(argv[j],"--mlat")) {
            Modes.mlat = 1;
        } else if (!strcmp(argv[j],"--aggressive")) {
            Modes.nfix_crc = MODES_MAX_BITERRORS;
        } else if (!strcmp(argv[j],"--help")) {
            showHelp();
            exit(0);
        } else {
            fprintf(stderr, "Unknown or not enough arguments for option '%s'.\n\n", argv[j]);
            showHelp();
            exit(1);
        }
    }

#ifdef _WIN32
    // Try to comply with the Copyright license conditions for binary distribution
    if (!Modes.quiet) {showCopyright();}
#define MSG_DONTWAIT 0
#endif

#ifndef _WIN32
    // Setup for SIGWINCH for handling lines
    if (Modes.interactive) {signal(SIGWINCH, sigWinchCallback);}
#endif

    // Initialization
    view1090Init();

    // Try to connect to the selected ip address and port. We only support *ONE* input connection which we initiate.here.
    c = (struct client *) malloc(sizeof(*c));
    if ((fd = setupConnection(c)) == ANET_ERR) {
        fprintf(stderr, "Failed to connect to %s:%d\n", View1090.net_input_beast_ipaddr, Modes.net_input_beast_port);
        exit(1);
    }

    // Keep going till the user does something that stops us
    while (!Modes.exit) {
        // If Modes.aircrafts is not NULL, remove any stale aircraft
        if (Modes.aircrafts) {
            interactiveRemoveStaleAircrafts();
    }

        // Refresh screen when in interactive mode
        if (Modes.interactive) {
            mainShowData();
        }

        if ((fd == ANET_ERR) || (recv(c->fd, pk_buf, sizeof(pk_buf), MSG_PEEK | MSG_DONTWAIT) == 0)) {
			free(c);
			usleep(1000000);
			c = (struct client *) malloc(sizeof(*c));
			fd = setupConnection(c);
			continue;
        }
        modesReadFromClient(c,"",decodeBinMessage);
		usleep(100000);
    }

    // The user has stopped us, so close any socket we opened
    if (fd != ANET_ERR) 
      {close(fd);}

    return (0);
}
//
//=========================================================================
//
