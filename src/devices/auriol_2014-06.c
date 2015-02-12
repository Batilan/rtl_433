#include "rtl_433.h"


#include <stdint.h> // for uint8_t
#include <stdio.h>  // fprintf
#include <string.h> // memcmp
#include <stdlib.h> // abs

#include <time.h>


#define MAX_OUTPUT_BUFFER 256
/**
 *  TODO: Provide information about the protocol and compatible transmitter devices here
 *  Add references (to hopefully permanent URL's) where more detailed info can be found
 *  Note that this info should be picked up by Doxygen to provide documentation for this module
 */

/* from plugin.h: */

/* Function declarations */

int auriol_2014_06_callback(uint8_t bb[BITBUF_ROWS][BITBUF_COLS]);


r_device auriol_2014_06 = {
    /* .id             = */ 101,
    /* .name           = */ "Auriol",
    /* .modulation     = */ OOK_PWM_D,
    /* .short_limit    = */ 600,
    /* .long_limit     = */ 1450,
    /* .reset_limit    = */ 8000,
    /* .json_callback  = */ &auriol_2014_06_callback,
};

int auriol_2014_06_callback(uint8_t bb[BITBUF_ROWS][BITBUF_COLS]) {
    int row, col;
    static char out_buffer[MAX_OUTPUT_BUFFER];

    for( row = 0; row < 0; row++) {

        for(col = 0; col < 6; col+=2 ) {
            fprintf( stderr, "%x%x ", bb[row][col] & 0xff, bb[row][col+1] & 0xff );
        }

        fprintf( stderr, " - ");

        for(col = 0; col < 8; col++ ) {
            fprintf( stderr, "%4d", bb[row][col] & 0xff );
        }
        fprintf( stderr, " - ");
        for(col = 0; col < 8; col++ ) {
            // Print MSB and LSB nibbles
            fprintf( stderr, "%3d%3d", (bb[row][col] & 0xf0)>>4, (bb[row][col] & 0x0f) );
        }
        fprintf( stderr, "\n");
    }
// NOTE: Working on this decoder
    if ( *(long*)bb[0] == 0L && *(long*)bb[2] == 0L && *(long*)&(bb[1][4]) != 0L && bb[1][5] == 0 &&  bb[1][6] == 0 &&  bb[1][7] == 0 &&
         memcmp( bb[1], bb[3], 8) == 0 &&  memcmp( bb[1], bb[5], 8) == 0 &&  memcmp( bb[1], bb[11], 8) == 0 )
    {
        uint8_t nibble[10];
        int hygro;
        int tempfx10;
        float temp;
        int channel;
        time_t timer;
        char buffer[25];
        struct tm* tm_info;

        // Split data in Nibbles for easy access
        for (col =0; col < 5; col++)
        {
            nibble[col*2] = (bb[1][col] & 0xf0)>>4;
            nibble[(col*2)+1] = (bb[1][col] & 0x0f);
        }

        tempfx10 = bb[1][2]*16 + nibble[6] - 900;
        temp = ((tempfx10 / 10.0)-32.0) / 1.8;
        hygro = nibble[7]*10 + nibble[8];
        channel = nibble[9];

        /* Determine current time and put in a formatted string buffer */
        time(&timer);
        tm_info = localtime(&timer);
        strftime(buffer, 25, "%Y-%m-%d %H:%M:%S", tm_info);

        fprintf(stderr, "temp: %2.1f, hygro: %d, channel: %d, time = %s\n", temp, hygro, channel, buffer);
        snprintf(out_buffer, MAX_OUTPUT_BUFFER,
            " {"
            " \"sensor-event\": { \"brand\": \"auriol\", \"model\": \"Z1234\", \"channel\": %d, \"epoch\": %ld },"
            " \"readings\": {\"temp\": %2.1f, \"hygro\": %d }"
            " }\n",
            channel, timer, temp, hygro );

        printf(out_buffer);

        return 1;
    }
#ifdef DEBUG
    else
    {
        // TODO: Remove, only for debugging
        if ( *(long*)bb[0] != 0L  || *(long*)bb[2] != 0L )
        {
            printf("Invalid pre-amble %ld %ld\n", *(long*)bb[0],  *(long*)bb[2]);
        }
        if ( *(long*)&(bb[1][4]) != 0L )
        {
            printf("No data found where expected (5 th byte)\n");
        }
        if (  bb[1][5] != 0 ||  bb[1][6] != 0 ||  bb[1][7] != 0 )
        {
            printf("Frame too long (%d %d %d)\n", bb[1][5], bb[1][6], bb[1][7]);
        }
        if (  memcmp( bb[1], bb[3], 8) != 0 ||  memcmp( bb[1], bb[5], 8) != 0 ||  memcmp( bb[1], bb[11], 8) != 0 )
        {
            printf("Frames not identical\n");
        }
    }
#endif
    return 0;
}

