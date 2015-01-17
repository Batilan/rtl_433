/*
 * TODO: Licence header
 * Author: based on work from Jens Jensen 2014 / zerog2k
 */

#include <stdint.h> // for uint8_t
#include <stdio.h>  // fprintf
#include <string.h> // memcmp
#include <stdlib.h> // abs

#include <time.h>

#include "plugin.h"         // for generic plugin_descriptor
#include "rtl_433_plugin.h" // For application plugin descriptor

/**
 *  Acurite 5 in 1 Weather station decoder
 *  See also http://www.acurite.com/5in1
 */

/* from plugin.h: */

//
// typedef struct plugin_descriptor_t {
//     char *application;  //! For what application was this plugin written
//     char *type;         //! What type of plugin (application specific)
//     char *model;        //! The specific model for which the plugin is written
//     int version;        //! The version of the plugin
// } plugin_descriptor;

/* Function declarations */

static int acurite5n1_callback(uint8_t bb[BITBUF_ROWS][BITBUF_COLS]);

r_device acurite5n1 = {
    /* .id             = */ 10,
    /* .name           = */ "Acurite 5n1 Weather Station",
    /* .modulation     = */ OOK_PWM_P,
    /* .short_limit    = */ 70,
    /* .long_limit     = */ 240,
    /* .reset_limit    = */ 21000,
    /* .json_callback  = */ &acurite5n1_callback,
};

rtl_433_plugin_t plugin =
{
    .plugin_desc = {
        .application = "rtl_433",
        .type        = "environment.weather",
        .model       = "com.acurite.5in1",
        .version     = 1
    },
    .r_device_p = &acurite5n1
};

VISIBLE extern void *get_plugin()
{
    static int returned = 0;
    if ( returned == 0)
    {
        returned = 1;
        return &plugin;
    }
    return NULL;

}

// ** Acurite 5n1 functions **

const float acurite_winddirections[] =
    { 337.5, 315.0, 292.5, 270.0, 247.5, 225.0, 202.5, 180,
      157.5, 135.0, 112.5, 90.0, 67.5, 45.0, 22.5, 0.0 };

static int acurite_raincounter = 0;

static int acurite_crc(uint8_t row[BITBUF_COLS], int cols) {
    // sum of first n-1 bytes modulo 256 should equal nth byte
    int i;
    int sum = 0;
    for ( i=0; i < cols; i++)
        sum += row[i];
    if ( sum % 256 == row[cols] )
        return 1;
    else
        return 0;
}

static int acurite_detect(uint8_t *pRow) {
    int i;
    if ( pRow[0] != 0x00 ) {
        // invert bits due to wierd issue
        for (i = 0; i < 8; i++)
            pRow[i] = ~pRow[i] & 0xFF;
        pRow[0] |= pRow[8];  // fix first byte that has mashed leading bit

        if (acurite_crc(pRow, 7))
            return 1;  // passes crc check
    }
    return 0;
}

static float acurite_getTemp (uint8_t highbyte, uint8_t lowbyte) {
    // range -40 to 158 F
    int highbits = (highbyte & 0x0F) << 7 ;
    int lowbits = lowbyte & 0x7F;
    int rawtemp = highbits | lowbits;
    float temp = (rawtemp - 400) / 10.0;
    return temp;
}

static int acurite_getWindSpeed (uint8_t highbyte, uint8_t lowbyte) {
    // range: 0 to 159 kph
    int highbits = ( highbyte & 0x1F) << 3;
    int lowbits = ( lowbyte & 0x70 ) >> 4;
    int speed = highbits | lowbits;
    return speed;
}

static float acurite_getWindDirection (uint8_t byte) {
    // 16 compass points, ccw from (NNW) to 15 (N)
    int direction = byte & 0x0F;
    return acurite_winddirections[direction];
}

static int acurite_getHumidity (uint8_t byte) {
    // range: 1 to 99 %RH
    int humidity = byte & 0x7F;
    return humidity;
}
static int acurite_getRainfallCounter (uint8_t highbyte, uint8_t lowbyte) {
    // range: 0 to 99.99 in, 0.01 in incr., rolling counter?
    int highbits = (highbyte & 0x3F) << 7 ;
    int lowbits = lowbyte & 0x7F;
    int raincounter = highbits | lowbits;
    return raincounter;
}

static int acurite5n1_callback(uint8_t bb[BITBUF_ROWS][BITBUF_COLS]) {
    // acurite 5n1 weather sensor decoding for rtl_433
    // Jens Jensen 2014
    int i;
    uint8_t *buf = NULL;

// TODO remove or replace by debug output call
    fprintf(stderr, "Called callback plugin for App: %s, type: %s, model: %s, version: %d\n",
        plugin.plugin_desc.application,
        plugin.plugin_desc.type,
        plugin.plugin_desc.model,
        plugin.plugin_desc.version );

    // run through rows til we find one with good crc (brute force)
    for (i=0; i < BITBUF_ROWS; i++) {
        if (acurite_detect(bb[i])) {
            buf = bb[i];
            break; // done
        }
    }

    if (buf) {
        // decode packet here
        fprintf(stderr, "Detected Acurite 5n1 sensor\n");
        // TODO: Add debugging hook
        //if (debug_output) {
        //    for (i=0; i < 8; i++)
        //        fprintf(stderr, "%02X ", buf[i]);
        //    fprintf(stderr, "CRC OK\n");
        //}

        if ((buf[2] & 0x0F) == 1) {
            // wind speed, wind direction, rainfall

            float rainfall = 0.00;
            int raincounter = 0;
            if (acurite_raincounter > 0) {
                // track rainfall difference after first run
                raincounter = acurite_getRainfallCounter(buf[5], buf[6]);
                rainfall = ( raincounter - acurite_raincounter ) * 0.01;
            } else {
                // capture starting counter
                acurite_raincounter = raincounter;
            }

            fprintf(stderr, "wind speed: %d kph, ",
                acurite_getWindSpeed(buf[3], buf[4]));
            fprintf(stderr, "wind direction: %0.1f°, ",
                acurite_getWindDirection(buf[4]));
            fprintf(stderr, "rain gauge: %0.2f in.\n", rainfall);

        } else if ((buf[2] & 0x0F) == 8) {
            // wind speed, temp, RH
            fprintf(stderr, "wind speed: %d kph, ",
                acurite_getWindSpeed(buf[3], buf[4]));
            fprintf(stderr, "temp: %2.1f° F, ",
                acurite_getTemp(buf[4], buf[5]));
            fprintf(stderr, "humidity: %d\%% RH\n",
                acurite_getHumidity(buf[6]));
        }
    }
    //if (debug_output)
    //   debug_callback(bb);
    return 1;
}
