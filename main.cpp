#include "mbed.h"
#include "rtos.h"

#include "SpwfInterface.h"
#include "NTPClient.h"
#include "TCPSocket.h"
#include "XBeeLib.h"
#include "SensorData.h"
#include "MQTTNetwork.h"

using namespace XBeeLib;

//www.mbed.com CA certificate in PEM format
char  CA_cert []="-----BEGIN CERTIFICATE-----\r\n"
"MIIDVDCCAjygAwIBAgIDAjRWMA0GCSqGSIb3DQEBBQUAMEIxCzAJBgNVBAYTAlVT\r\n"
"MRYwFAYDVQQKEw1HZW9UcnVzdCBJbmMuMRswGQYDVQQDExJHZW9UcnVzdCBHbG9i\r\n"
"YWwgQ0EwHhcNMDIwNTIxMDQwMDAwWhcNMjIwNTIxMDQwMDAwWjBCMQswCQYDVQQG\r\n"
"EwJVUzEWMBQGA1UEChMNR2VvVHJ1c3QgSW5jLjEbMBkGA1UEAxMSR2VvVHJ1c3Qg\r\n"
"R2xvYmFsIENBMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA2swYYzD9\r\n"
"9BcjGlZ+W988bDjkcbd4kdS8odhM+KhDtgPpTSEHCIjaWC9mOSm9BXiLnTjoBbdq\r\n"
"fnGk5sRgprDvgOSJKA+eJdbtg/OtppHHmMlCGDUUna2YRpIuT8rxh0PBFpVXLVDv\r\n"
"iS2Aelet8u5fa9IAjbkU+BQVNdnARqN7csiRv8lVK83Qlz6cJmTM386DGXHKTubU\r\n"
"1XupGc1V3sjs0l44U+VcT4wt/lAjNvxm5suOpDkZALeVAjmRCw7+OC7RHQWa9k0+\r\n"
"bw8HHa8sHo9gOeL6NlMTOdReJivbPagUvTLrGAMoUgRx5aszPeE4uwc2hGKceeoW\r\n"
"MPRfwCvocWvk+QIDAQABo1MwUTAPBgNVHRMBAf8EBTADAQH/MB0GA1UdDgQWBBTA\r\n"
"ephojYn7qwVkDBF9qn1luMrMTjAfBgNVHSMEGDAWgBTAephojYn7qwVkDBF9qn1l\r\n"
"uMrMTjANBgkqhkiG9w0BAQUFAAOCAQEANeMpauUvXVSOKVCUn5kaFOSPeCpilKIn\r\n"
"Z57QzxpeR+nBsqTP3UEaBU6bS+5Kb1VSsyShNwrrZHYqLizz/Tt1kL/6cdjHPTfS\r\n"
"tQWVYrmm3ok9Nns4d0iXrKYgjy6myQzCsplFAMfOEVEiIuCl6rYVSAlk6l5PdPcF\r\n"
"PseKUgzbFbS9bZvlxrFUaKnjaZC2mqUPuLk/IH2uSrW4nOQdtqvmlKXBx4Ot2/Un\r\n"
"hw4EbNX/3aBd7YdStysVAq45pmp06drE57xNNB6pXE0zX5IJL4hmXXeXxx12E6nV\r\n"
"5fEWCRE11azbJHFwLJhWC9kXtNHjUStedejV0NxPNO3CBWaAocvmMw==\r\n"
"-----END CERTIFICATE-----\r\n";

SpwfSAInterface spwf(D8, D2, false);  // Wifi interface
DigitalOut myled(D4);
Serial pc(USBTX, USBRX, 115200);
//           TX, RX, Reset, 
XBee802 xbee(A0, A1, A2, NC, NC, 115200);

// Thread handles
Thread xbeeThd(osPriorityNormal, DEFAULT_STACK_SIZE * 2);
Thread blinkerThd;
// The RTOS queue used by the xbee message passing between threads
MemoryPool<sensor_data, 16> mpool;
Queue<sensor_data, 16> queue;

int blinkUpdateFreq = 500;

/*----------------------------------------------------------------------------*/
/* Callback functions                                                         */
/*----------------------------------------------------------------------------*/
static void io_data_cb(const RemoteXBee802& remote, 
            const IOSample802& sample_data)
{
    // RadioStatus radioStatus;
    pc.printf("================ Radio Callback ==================\r\n");
    if (remote.is_valid_addr16b()) {
        pc.printf("\r\nGot a 16-bit IO Sample Data [%04X]\r\n", 
                    remote.get_addr16());
    } else {
        const uint64_t remote_addr64 = remote.get_addr64();
        pc.printf("\r\nGot a 64-bit IO Sample Data %lld[%08x:%08x]\r\n", 
            remote.get_addr64(), 
            UINT64_HI32(remote_addr64), 
            UINT64_LO32(remote_addr64));        
    }
    
    // TODO: Implement handling of data
    // Create a message to be passed to the thingspeak consumer thread ...
    sensor_data *message = mpool.alloc();
    // Dummy values for now
    message->temperature = 1.0; 
    message->humidity = 1.0;
    message->luminance = 1.0; 
    message->channelid = 1;
    
    // Push the data to the consumer thread
    pc.printf("Pushing data to the queue ...\r\n");
    queue.put(message);      
}

/*----------------------------------------------------------------------------*/
/* Threads                                                                    */
/*----------------------------------------------------------------------------*/
void XbeeListenerThd()
{
    /* This thread polls data from the xbee device
     * and calls any callback functions associated with it.
     */
    while(true)
    {
        xbee.process_rx_frames(); 
        Thread::wait(100);
    }
}

void BlinkerThd()
{
    while(true)
    {
        myled = !myled;
        Thread::wait(blinkUpdateFreq);
    }    
}
/*----------------------------------------------------------------------------*/
/* Helper/Initialization functions                                            */
/*----------------------------------------------------------------------------*/
int StartXbeeRadios()
{
    /* Initialize the local xbee and create a 
     * thread to poll data from remote xbees
     */
    pc.printf("Initializing xbee radios!\r\n");        
    /* Register the callback function (when a data from
     * remote xbees are received
     */
    xbee.register_io_sample_cb(&io_data_cb);
    
    /* Initialize the local xbee device and read 
     * the relevant parameters.
     */
    pc.printf("Initializing XBee radio ...\r\n");
    RadioStatus radioStatus = xbee.init();
    if (radioStatus != Success) {
        pc.printf("Initialization failed!!\r\n");
        return -1;
    }
    
    /* Now start the Xbee listener thread to start
     * polling data from remote xbee radios
     */
    pc.printf("Running the xbee listener thread ...\r\n");
    xbeeThd.start(XbeeListenerThd);     
    
    pc.printf("XBee radio initialized ...\r\n");
    
    return 0;
}



/*----------------------------------------------------------------------------*/
/* Main/Startup                                                               */
/*----------------------------------------------------------------------------*/

int main() 
{
    time_t ctTime;
    const char * wifiSSID = "VPCOLA";
    const char * wifiPassword = "AB12CD34";
    ctTime = time(NULL);
    
    pc.printf("MBED Garden Monitor 1.0\r\n");
    blinkerThd.start(BlinkerThd);
    
    pc.printf ("\n\rConnecting to WiFi ...\n\r");  
    spwf.connect(wifiSSID,wifiPassword, NSAPI_SECURITY_WPA2);
    
    pc.printf("IP Address : %s\r\n", spwf.get_ip_address());
    pc.printf("MAC Address : %s\r\n", spwf.get_mac_address());
    
    /**
     * NOTE:  DNS requires UDP socket. DNS is invoked while connecting a 
     * secure socket to resolve URL to IP, so DNS doesnt answer as it 
     * requires unsecure UDP socket connection.
            
     * Workaroud: convert URL to IP quering DNS before secure socket 
     * creation, then connect the secure socket created to the retrieved 
     * IP (Resolve host using DNS first.
     **/
     SocketAddress addr(&spwf, "www.mbed.com", 443);// must be called BEFORE 
                                                     // set_secure_mode() to
                                                     // allow DNS udp connection 
     pc.printf("www.mbed.com resolves to : %s\r\n", addr.get_ip_address());
            
    NTPClient ntp(spwf);        

    pc.printf("Initial System Time is: %s\r\n", ctime(&ctTime));   
    pc.printf("Trying to update time...\r\n");
    if (ntp.setTime("0.pool.ntp.org") == 0)
    {
      pc.printf("Set time successfully\r\n");
      ctTime = time(NULL);
      pc.printf("Time is set to (UTC): %s\r\n", ctime(&ctTime));
      
#if 0      
      /* Set the wifi time/date in order to generate
       * TLS certificate authentication properly
       */
      pc.printf("Setting wifi date/time ..\r\n");
      if (!spwf.set_time(ctTime)) 
        pc.printf ("ERROR set_time\n\r");     
    
      pc.printf("Cleaning wifi TLS certificates...\r\n");
      if (!spwf.clean_TLS_certificate(ALL)) 
        pc.printf ("ERROR clean_TLS_certificate\n\r");
      
      pc.printf("Setting wifi TLS certificate ...\r\n");
      if (!spwf.set_TLS_certificate(CA_cert, 
                    sizeof(CA_cert), FLASH_CA_ROOT_CERT)) 
           pc.printf ("ERROR set_TLS_certificate\n\r");
           
      pc.printf("Setting server domain ...\r\n");     
      if (!spwf.set_TLS_SRV_domain("*.mbed.com",FLASH_DOMAIN)) 
           pc.printf ("ERROR set_TLS_CA_domain\n\r");
 #endif      
      /**
       * After the Xbee Radios are initialized, we now start a 
       * to forward the data via secure MQTT in our main loop.
       **/
      if ( StartXbeeRadios() == 0 )
      {
            // Time to start our main loop  
            pc.printf("Starting main loop ...\r\n");   
            while(true)
            {
               pc.printf("Waiting for events...\r\n");
               // Wait until data is available in the queue
               osEvent evt = queue.get();
               if (evt.status == osEventMessage)
               {
                    pc.printf("Received event message!\r\n");
                    // Unpack the message
                    sensor_data *message = (sensor_data *)evt.value.p;
                    message->debug(pc);
                    
                    // Must be called before socket creation.
                    //spwf.set_secure_mode();         
                    // Create the TCP socket
                    TCPSocket  * socket = new TCPSocket(&spwf);   
                    // Called after socket creation
                    //spwf.set_unsecure_mode();
                    // Connect to server ...
                    int err = socket->connect(addr);
                    if (err != 0 )
                        pc.printf ("ERROR opening %d\n\r", err);
                    else
                        pc.printf ("--->>> Secure socket CONNECTED to: %s\n\r", 
                                addr.get_ip_address());
               
                     // TODO: Use Secure MQTT to update data on server
               
               
                    // Finally close this socket
                    socket->close();
                    delete socket;

               } // event
               // Allow other threads to run
               Thread::wait(100);
             } // Main loop
        }
        else
        {
            // XBee Radios failed to start ...
            pc.printf("XBee radios failed to start ...\r\n");
        }
    }  
    else
    {
      // NTP Time failed to start
      pc.printf("Error: NTP could not contact server\r\n");
    }
    
    spwf.disconnect();
    pc.printf ("WIFI disconnected, exiting ...\n\r");

    // Blink LED fast to indicate an error
    blinkUpdateFreq = 200;    
    while(true) { 
      Thread::wait(100);
    }    
}
