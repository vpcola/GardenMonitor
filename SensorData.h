#ifndef __SENSOR_DATA_H__
#define __SENSOR_DATA_H__

#include "mbed.h"

// message shared between threads, all items must be POD
typedef struct 
{
    uint32_t channelid;
    float humidity;
    float temperature;
    float luminance;
    
    void debug(Stream & strm)
    {
        strm.printf("Channel id : [%d]\r\n", channelid);
        strm.printf("Humidity : [%.2f]\r\n", humidity);
        strm.printf("Temperature : [%.2f]\r\n", temperature);
        strm.printf("Luminance : [%.2f]\r\n", luminance);
    }
}sensor_data;

#endif