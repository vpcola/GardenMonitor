/* NTPClient.cpp */
/* Copyright (C) 2012 mbed.org, MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

//Debug is disabled by default
#if 0
//Enable debug
#define __DEBUG__
#include <cstdio>
#define DBG(x, ...) std::printf("[NTPClient : DBG]"x"\r\n", ##__VA_ARGS__); 
#define WARN(x, ...) std::printf("[NTPClient : WARN]"x"\r\n", ##__VA_ARGS__); 
#define ERR(x, ...) std::printf("[NTPClient : ERR]"x"\r\n", ##__VA_ARGS__); 

#else
//Disable debug
#define DBG(x, ...) 
#define WARN(x, ...)
#define ERR(x, ...) 

#endif
#include "NetworkStack.h"
#include "NTPClient.h"

#include "UDPSocket.h"

#include "mbed.h" //time() and set_time()

#define NTP_PORT 123
#define NTP_CLIENT_PORT 0 //Random port
#define NTP_TIMESTAMP_DELTA 2208988800ull //Diff btw a UNIX timestamp (Starting Jan, 1st 1970) and a NTP timestamp (Starting Jan, 1st 1900)

/*NTPClient::NTPClient() : m_sock()
{

}*/

NTPClient::NTPClient(NetworkStack & _m_intf) : m_intf(_m_intf)
{
}

#ifdef htons
#undef htons
#endif /* htons */
#ifdef htonl
#undef htonl
#endif /* htonl */
#ifdef ntohs
#undef ntohs
#endif /* ntohs */
#ifdef ntohl
#undef ntohl
#endif /* ntohl */


#if ((__BYTE_ORDER__) == (__ORDER_LITTLE_ENDIAN__))

#define htons(x) ((((x) & 0xff) << 8) | (((x) & 0xff00) >> 8))
#define ntohs(x) htons(x)
#define htonl(x) ((((x) & 0xff) << 24) | \
                     (((x) & 0xff00) << 8) | \
                     (((x) & 0xff0000UL) >> 8) | \
                     (((x) & 0xff000000UL) >> 24))
#define ntohl(x) htonl(x)

#else

#define htons(x)  (x)
#define htonl(x)  (x)
#define ntohl(x)  (x)
#define ntohs(x)  (x)

#endif 


NTPResult NTPClient::setTime(const char* host, uint16_t port, uint32_t timeout)
{
#ifdef __DEBUG__
  time_t ctTime;
  ctTime = time(NULL);
  set_time(ctTime);
  DBG("Time is set to (UTC): %s", ctime(&ctTime));
#endif

  SocketAddress address(0, port);
  int r = m_intf.gethostbyname(&address, host);  
  if (r) {
        printf("error: 'gethostbyname(\"%s\")' failed with code %d\r\n", host, r);
  } else if (!address) {
        printf("error: 'gethostbyname(\"%s\")' returned null IP address\r\n", host);
  }  
  //printf ("address: %s\n\r",address.get_ip_address());
   
  //Create & bind socket
  if (m_sock.open(&m_intf) < 0) printf ("ERROR sock open \n\r");  
  m_sock.set_timeout(timeout);  

  struct NTPPacket pkt;  
  memset (&pkt, 0, sizeof(NTPPacket));   

  //Now ping the server and wait for response
  DBG("Ping");
  //Prepare NTP Packet:
  pkt.li = 0; //Leap Indicator : No warning
  pkt.vn = 4; //Version Number : 4
  pkt.mode = 3; //Client mode
  pkt.stratum = 0; //Not relevant here
  pkt.poll = 0; //Not significant as well
  pkt.precision = 0; //Neither this one is

  int ret = m_sock.sendto(address, (char*)&pkt, sizeof(NTPPacket) ); 
  if (ret < 0 )
  {
    ERR("Could not send packet %d", ret);
    m_sock.close();
    return NTP_CONN;
  }

  //Read response
  DBG("Pong");

  ret = m_sock.recvfrom(&address, (char*)&pkt, sizeof(NTPPacket) );  // LICIO
  if(ret < 0)
  {
    ERR("Could not receive packet %d", ret);
    m_sock.close();
    return NTP_CONN;
  }

  if(ret < sizeof(NTPPacket)) //TODO: Accept chunks
  {
    ERR("Receive packet size does not match");
    m_sock.close();
    return NTP_PRTCL;
  }

  if( pkt.stratum == 0)  //Kiss of death message : Not good !
  {
    ERR("Kissed to death!");
    m_sock.close();
    return NTP_PRTCL;
  }

  //Correct Endianness
  pkt.refTm_s = ntohl( pkt.refTm_s ); 
  pkt.refTm_f = ntohl( pkt.refTm_f );
  pkt.origTm_s = ntohl( pkt.origTm_s );
  pkt.origTm_f = ntohl( pkt.origTm_f );
  pkt.rxTm_s = ntohl( pkt.rxTm_s );
  pkt.rxTm_f = ntohl( pkt.rxTm_f );
  pkt.txTm_s = ntohl( pkt.txTm_s );
  pkt.txTm_f = ntohl( pkt.txTm_f );

  // see RFC 4330 p.13
  time_t txTm = (time_t)(pkt.txTm_s - NTP_TIMESTAMP_DELTA);  
  set_time(txTm); 

#ifdef __DEBUG__
  ctTime = time(NULL);
  DBG("Time is now (UTC): %s", ctime(&ctTime));
#endif
  m_sock.close();

  return NTP_OK;
}

