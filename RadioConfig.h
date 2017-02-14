#ifndef __RADIO_CONFIG_H__
#define __RADIO_CONFIG_H__

#include "XBeeLib.h"
#include <string>

using namespace XBeeLib;

#define XBRADIO_TRANSMIT_FREQ 5000
class RadioConfig
{
    public:
       
        RadioConfig(std::string name,
                uint16_t address,
                XBee802::IoLine humid_pin = XBee802::DIO2_AD2,
                XBee802::IoLine temp_pin = XBee802::DIO1_AD1,
                XBee802::IoLine lumin_pin = XBee802::DIO0_AD0,
                int transmit_freq = XBRADIO_TRANSMIT_FREQ)
           :_name(name),
           _address(address),
           _humidity_pin(humid_pin),
           _temperature_pin(temp_pin),
           _luminance_pin(lumin_pin),
           _transmit_freq(transmit_freq)
        {
        }

        RadioStatus setRemoteConfig(XBee802 & local, RemoteXBee802 & remote)
        {
            RadioStatus radioStatus;
            // Configure the digital input pin, for all remote garden probes, 
            // this would be DIO3
            radioStatus = local.set_pin_config(remote, XBee802::DIO3_AD3,
                                   DigitalInput); // Set remote device's DIO3 as input pin
            if (radioStatus != Success)
            {
                printf("Failed to configure Digital IO pin DIO3_AD3 on %lX\r\n", 
                    remote.get_addr16());
                return radioStatus;
            }
            // Enable the pull-up resistor on the remote device
            radioStatus == local.set_pin_pull_up(remote, XBee802::DIO3_AD3, true);
            if (radioStatus != Success)
            {
                printf("Failed to setup pull-up resistor on remote Device %lX\r\n",
                    remote.get_addr16());
                return radioStatus;
            }
            
            // Configure the ADC pin for Humidity
            radioStatus = local.set_pin_config(remote, _humidity_pin, Adc);
            if (radioStatus != Success)
            {
                printf("Failed to setup ADC pin %d on remote device %lX\r\n", 
                    (int) _humidity_pin, remote.get_addr16());   
                return radioStatus;
            }
            
            // Configure the ADC pin for Temperature
            radioStatus = local.set_pin_config(remote, _temperature_pin, Adc);
            if (radioStatus != Success)
            {
                printf("Failed to setup ADC pin %d on remote device %lX\r\n", 
                    (int) _temperature_pin, remote.get_addr16());   
                return radioStatus;
            }
            
            // Configure the ADC pin for Luminance
            radioStatus = local.set_pin_config(remote, _luminance_pin, Adc);
            if (radioStatus != Success)
            {
                printf("Failed to setup ADC pin %d on remote device %lX\r\n",
                    (int) _luminance_pin, remote.get_addr16());   
                return radioStatus;
            }                        
            
            // Configure the sampling rate interval
            radioStatus = local.set_io_sample_rate(remote, _transmit_freq);
            
            return radioStatus;
        }
        
        std::string _name;
        uint16_t    _address;
        XBee802::IoLine _humidity_pin;
        XBee802::IoLine _temperature_pin;
        XBee802::IoLine _luminance_pin;
        int _transmit_freq;
        
};

#endif