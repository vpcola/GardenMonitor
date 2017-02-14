#ifndef _MQTTNETWORK_H_
#define _MQTTNETWORK_H_

// Older mbed OS2 NetworkSocketAPI uses NetworkStack instead of 
// NetworkInterface 
#include "SpwfInterface.h"
#include "SocketAddress.h"
 
class MQTTNetwork {
public:
    MQTTNetwork(SpwfSAInterface & aNetwork) : _net(aNetwork),
        _socket(NULL) // don't create the socket until we connect
        
    {
    }
 
    ~MQTTNetwork() 
    {
        disconnect();
    }
 
    int read(unsigned char* buffer, int len, int timeout) 
    {
        if (_socket)
            return _socket->recv(buffer, len);
        return -1;
    }
 
    int write(unsigned char* buffer, int len, int timeout) 
    {
        if (_socket)
            return _socket->send(buffer, len);
        return -1;
    }
 
    int connect(const char* hostname, int port = 443) 
    {
       if (_socket != NULL)
       {
            // Reset the socket
            disconnect();           
       } 
       /**
        * NOTE:  DNS requires UDP socket. DNS is invoked while connecting a 
        * secure socket to resolve URL to IP, so DNS doesnt answer as it 
        * requires unsecure UDP socket connection.
            
        * Workaroud: convert URL to IP quering DNS before secure socket 
        * creation, then connect the secure socket created to the retrieved 
        * IP (Resolve host using DNS first.
        **/
        SocketAddress addr(&_net, hostname, port);
        
        // Must be called before socket creation.
        _net.set_secure_mode();         
        // Create the TCP socket, also calls open 
        // on the socket
        _socket = new TCPSocket(&_net);
        if (_socket == NULL)
        {
            return -1;
        }
        // Called after socket creation
        _net.set_unsecure_mode();
        // Connect to server ...
        int err = _socket->connect(addr);
        if (err != 0 )
        {
            printf ("ERROR opening %d\n\r", err);
            return -1;
        }
        else
            printf ("--->>> Secure socket CONNECTED to: %s\n\r", 
                    addr.get_ip_address());
                    
        return 1;
    }
 
    void disconnect() 
    {
        if (_socket)
        {
            _socket->close();
            delete _socket;
            
            _socket = NULL;
        }
    }
 
private:
    SpwfSAInterface & _net;
    TCPSocket* _socket;
};
 
#endif // _MQTTNETWORK_H_