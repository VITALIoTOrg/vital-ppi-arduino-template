/*
 Arduino based VITAL PPI template
 
 Created by Lorenzo Bracco, Santer Reply
 */

#include <SPI.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <avr/pgmspace.h>
#include <TimeLib.h>
#include <Time.h>

#define CL 15 // "Content-Lenght:" string length
#define CT 13 // "Content-Type:" string length
#define METADATA "/metadata"
#define METALEN 9
#define SENSOR_METADATA "/sensor/metadata"
#define SENSMETALEN 16
#define SERVICE_METADATA "/service/metadata"
#define SERVMETALEN 17
#define OBSERVATION "/sensor/observation"
#define OBSELEN 19
#define MAX_BUF 90
#define NTP_TIMEOUT 14000

int requested(char *ep, char *method, char *type);
void answer(char *path, char *method, char *type, char *data, WiFiClient client);
int getMetadata(char *body, WiFiClient client);
int getObservation(char *body, WiFiClient client);
void sendNTPpacket(IPAddress& address);
void printWifiStatus();

// Table of strings (to keep in flash memory)
const char string0[] PROGMEM = "MyWiFiSSID"; // Wi-fi network SSID
const char string1[] PROGMEM = "MyWiFiPassword"; // Wi-fi password

const char string2[] PROGMEM = "Content-Length:"; // Content-Length header
const char string3[] PROGMEM = "Content-Type:"; // Content-Type header

const char endp1[] PROGMEM = METADATA; // Metadata endpoint path
const char endp2[] PROGMEM = SENSOR_METADATA; // Sensor metadata endpoint path
const char endp3[] PROGMEM = SERVICE_METADATA; // Service metadata endpoint path
const char endp4[] PROGMEM = OBSERVATION; // Observation endpoint path

const char* const string_table[] PROGMEM = { string0, string1, string2, string3 };
const char* const endp_table[] PROGMEM = { endp1, endp2, endp3, endp4 };
// The following should define strings you need to construct your responses (meta is for metadata, you can define them also for observations)
//const char* const meta_table[] PROGMEM = { meta1, meta2, meta3 };

int keyIndex = 0; // your network key Index number (needed only for WEP)

int status = WL_IDLE_STATUS;

WiFiServer server(80);

unsigned int localPort = 2390; // local port to listen for UDP packets (user for NTP)
IPAddress timeServer(129,6,15,28); // time.nist.gov NTP server
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[NTP_PACKET_SIZE]; // buffer to hold incoming and outgoing NTP packets
WiFiUDP Udp; // A UDP instance to send and receive packets over UDP

IPAddress noipserver(8,23,224,120); // No-IP
IPAddress localaddr(192,168,1,50); // Local IP
IPAddress gateaddr(192,168,1,1); // Gateway IP
IPAddress dnsaddr(8,8,8,8); // DNS server IP
IPAddress netmask(255,255,255,0); // Netmask

unsigned long timegone;

void setup() {
  char ssid[30];
  char pass[30];

  timegone = -120000;

  // Initialize serial and wait for port to open
  Serial.begin(9600);
  while (!Serial); // wait for serial port to connect (needed for Leonardo only)
 
  // check for the presence of the shield
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println(F("WiFi shield not present"));
    // don't continue:
    while(true);
  }

  strcpy_P(ssid, (char *) pgm_read_word(&(string_table[0])));
  strcpy_P(pass, (char *) pgm_read_word(&(string_table[1])));
  // attempt to connect to Wifi network
  while (status != WL_CONNECTED) {
    Serial.print(F("Attempting to connect to SSID: "));
    Serial.println(ssid);
    // Connect to WPA/WPA2 network
    //WiFi.config(localaddr, dnsaddr, gateaddr, netmask);
    status = WiFi.begin(ssid, pass);
    // wait 10 seconds for connection:
    delay(10000);
  }
  server.begin();
  // you're connected now, so print out the status
  printWifiStatus();

  Udp.begin(localPort);

  delay(2000); // Just let everything settle
}

void loop() {
  WiFiClient clientDNS;
  unsigned long busytime;
  int newline, body;
  int pathstatus; // 0 before, 1 reading, 2 read
  char path[50]; // contains the requested endpoint
  char data[1000]; // request body
  char httpMethod[20];
  char line[100];
  char c;
  int cl; // content lenght
  char clstr[CL + 1];
  char ctstr[CL + 1];
  char ct[50];
  int i, j;
  int bi; // index for the body
  int pi; // index for the path
  int mi; // index for the http method
  int li; // index for the line

  if (millis() - timegone >  120000) {
    Serial.println(F("Updating dynamic DNS..."));
    if (clientDNS.connect(noipserver, 80)) {
      clientDNS.println(F("GET /nic/update?hostname=myhostname HTTP/1.0"));
      clientDNS.println(F("Host: dynupdate.no-ip.com"));
      clientDNS.println(F("Authorization: Basic encodedauthstring"));
      clientDNS.println(F("User-Agent: VITAL Arduino/1.0 myname@domain.com"));
      clientDNS.println();
      busytime = millis();
      while (clientDNS.connected() && (millis() - busytime) < 10000) {
        while (clientDNS.available()) {
          c = clientDNS.read();
          Serial.write(c);
        }
      }
      clientDNS.stop();
    }
    Serial.println();
    Serial.println(F("Done."));
    timegone = millis();
  }

  // listen for incoming clients
  WiFiClient client = server.available();
  if (client) {
    Serial.println(F("new client"));
    // Initialize stuff
    strcpy_P(clstr, (char *) pgm_read_word(&(string_table[2])));
    strcpy_P(ctstr, (char *) pgm_read_word(&(string_table[3])));
    newline = body = 0; // boolean
    cl = 0; // integer
    ct[0] = '\0';
    pathstatus = 0; // custom
    li = pi = mi = 0; // indeces
    busytime = millis();
    while (client.connected() && (millis() - busytime) < 10000) {
      if (client.available()) {
        c = client.read(); // read next byte
        Serial.write(c);
        line[li] = c;
        li++;
        switch (pathstatus) {
          case 0:
            if(c == ' ') {
              httpMethod[mi] = '\0';
              pathstatus++;
            } else {
              httpMethod[mi] = c;
              mi++;
            }
            break;
          case 1:
            if(c == ' ') {
              path[pi] = '\0';
              pathstatus++;
            } else {
              path[pi] = c;
              pi++;
            }
        }
        if (body) {
          data[bi] = c;
          bi++;
          if (bi >= cl) { // body and whole request ended
            data[bi] = '\0';
            answer(path, httpMethod, ct, data, client);
            break;
          }
        }
        if (c == '\n') {
          if (newline) { // headers and body are separated by an empty line
            body = 1;
            //Serial.println(F("Body size:"));
            //Serial.println(cl);
            bi = 0;
            if (cl == 0) { // If content length is zero, then request is ended
              answer(path, httpMethod, ct, data, client);
              break;
            }
          }
          line[li] = '\0';
          if (!body && String(line).startsWith(String(clstr))) { // Is the line the Content-Length header?
            i = CL;
            while (i < li && line[i] == ' ') // Ignore spaces
              i++;
            while (i < li && (line[i] >= '0' && line[i] <= '9')) { // Get the number
              cl =  10 * cl + line[i] - '0'; // from characters to number
              i++;
            }
          } else if (!body && String(line).startsWith(String(ctstr))) { // Is the line the Content-Length header?
            i = CT;
            while (i < li && line[i] == ' ') // Ignore spaces
              i++;
            j = 0;
            while (i < li && !(line[i] == '\n' || line[i] == '\r' || line[i] == ' ')) { // Get the number
              ct[j] =  line[i]; // from characters to number
              i++;
              j++;
            }
            ct[j] = '\0';
          }
          li = 0;
          newline = 1;
        } else if (c != '\r') { // Specs say to ignore CR and consider LF as line separator
          newline = 0;
        }
      }
    }

    // give the client time to receive the data
    delay(100);

    // close the connection:
    client.stop();

    // just a sec or it could redetect the connection
    delay(100);

    Serial.println();
    Serial.println(F("client disconnected"));
    Serial.println();
  }
}

/*
 * Returns requested endpoint:
 * -3 wrong content type
 * -2 wrong method
 * -1 wrong endpoint
 * 0 metadata
 * 1 sensor metadata
 * 2 service metadata
 * 3 observation
 */
int requested(char *ep, char *method, char *type)
{
  int i, m, ssm, srm, o;
  char metadata[METALEN + 1];
  char sensor_metadata[SENSMETALEN + 1];
  char service_metadata[SERVMETALEN + 1];
  char observation[OBSELEN + 1];
  int tocall;

  strcpy_P(metadata, (char *) pgm_read_word(&(endp_table[0])));
  strcpy_P(sensor_metadata, (char *) pgm_read_word(&(endp_table[1])));
  strcpy_P(service_metadata, (char *) pgm_read_word(&(endp_table[2])));
  strcpy_P(observation, (char *) pgm_read_word(&(endp_table[3])));

  tocall = -1;
  i = m = ssm = srm = o = 0;
  while (ep[i] != '\0' && ep[i] != '?') {
    if (m < METALEN)
      if (ep[i] == metadata[m])
        m++;
    if (ssm < SENSMETALEN)
      if (ep[i] == sensor_metadata[ssm])
        ssm++;
    if (srm < SERVMETALEN)
      if (ep[i] == service_metadata[srm])
        srm++;
    if (o < OBSELEN)
      if (ep[i] == observation[o])
        o++;
    i++;
  }

  if (m == METALEN && m == i)
    tocall = 0;
  else if (ssm == SENSMETALEN && ssm == i)
    tocall = 1;
  else if (srm == SERVMETALEN && srm == i)
    tocall = 2;
  else if (o == OBSELEN && o == i)
    tocall = 3;

  if (tocall >= 0 && !String(method).equalsIgnoreCase(F("POST"))) {
    tocall = -2;
  }

  if (tocall >= 0 && !String(type).equalsIgnoreCase(F("application/json"))) {
    tocall = -3;
  }

  return tocall;
}

void answer(char *path, char *method, char *type, char *data, WiFiClient client) {
  int re;

  re = requested(path, method, type);
  switch (re) {
    case -3:
      // send error UNSUPPORTED MEDIA TYPE
      client.println(F("HTTP/1.1 415 Unsupported Media Type"));
      client.println(F("Connection: close")); // the connection will be closed after completion of the response
      client.println();
      break;
    case -2:
      // send error METHOD NOT ALLOWED
      client.println(F("HTTP/1.1 405 Method Not Allowed"));
      client.println(F("Connection: close")); // the connection will be closed after completion of the response
      client.println();
      break;
    case -1:
      // send error NOT FOUND
      client.println(F("HTTP/1.1 404 Not Found"));
      client.println(F("Connection: close")); // the connection will be closed after completion of the response
      client.println();
      break;
    case 0:
      if (getMetadata(data, client) != 0) {
        Serial.println();
        Serial.println(F("Error getting system metadata"));
      }
      break;
    case 1:
      if (getSensorMetadata(data, client) != 0) {
        Serial.println();
        Serial.println(F("Error getting sensor metadata"));
      }
      break;
    case 2:
      if (getServiceMetadata(data, client) != 0) {
        Serial.println();
        Serial.println(F("Error getting service metadata"));
      }
      break;
    case 3:
      if (getObservation(data, client) != 0) {
        Serial.println();
        Serial.println(F("Error getting observation"));
      }
      break;
  }
}

int getMetadata(char *body, WiFiClient client)
{
  int i, j;
  char tosend[1000];
  char buf[MAX_BUF + 1]; // On each client.print a max of 90 bytes is allowed

  //strcpy_P(tosend, (char *) pgm_read_word(&(meta_table[0])));

  // send a standard http response header
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: application/json"));
  client.print(F("Content-Length: "));
  client.println(String(tosend).length());
  client.println(F("Connection: close"));
  client.println();

  j = 0;
  for (i = 0; tosend[i] != '\0'; i++) {
    buf[j] = tosend[i];
    j++;
    if (j == MAX_BUF) {
      buf[j] = '\0';
      client.print(buf);
      j = 0;
    }
  }
  if (j > 0) {
    buf[j] = '\0';
    client.print(buf);
  }

  return 0;
}

int getSensorMetadata(char *body, WiFiClient client)
{
  int i, j;
  char tosend[1000];
  char buf[MAX_BUF + 1]; // On each client.print a max of 90 bytes is allowed

  //strcpy_P(tosend, (char *) pgm_read_word(&(meta_table[1])));

  // send a standard http response header
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: application/json"));
  client.print(F("Content-Length: "));
  client.println(String(tosend).length());
  client.println(F("Connection: close"));
  client.println();

  j = 0;
  for (i = 0; tosend[i] != '\0'; i++) {
    buf[j] = tosend[i];
    j++;
    if (j == MAX_BUF) {
      buf[j] = '\0';
      client.print(buf);
      j = 0;
    }
  }
  if (j > 0) {
    buf[j] = '\0';
    client.print(buf);
  }

  return 0;
}

int getServiceMetadata(char *body, WiFiClient client)
{
  int i, j;
  char tosend[1000];
  char buf[MAX_BUF + 1]; // On each client.print a max of 90 bytes is allowed

  //strcpy_P(tosend, (char *) pgm_read_word(&(meta_table[2])));

  // send a standard http response header
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: application/json"));
  client.print(F("Content-Length: "));
  client.println(String(tosend).length());
  client.println(F("Connection: close"));
  client.println();

  j = 0;
  for (i = 0; tosend[i] != '\0'; i++) {
    buf[j] = tosend[i];
    j++;
    if (j == MAX_BUF) {
      buf[j] = '\0';
      client.print(buf);
      j = 0;
    }
  }
  if (j > 0) {
    buf[j] = '\0';
    client.print(buf);
  }

  return 0;
}

int getObservation(char *body, WiFiClient client)
{
  int i, j, len, fail;
  char tosend[1000];
  String request, finresp;
  char buf[MAX_BUF + 1]; // On each client.print a max of 90 bytes is allowed
  unsigned long highWord, lowWord, secsSince1900, epoch;
  const unsigned long seventyYears = 2208988800UL; // Unix time starts on Jan 1 1970. In seconds, that's 2208988800
  unsigned long timegone;
  int property;

  fail = 0;

  // Determine whether temperature or humidity have been requested
  property = -1;
  request = String(body);
  // Look for a valid property in the request body and set the "property" variable accordingly
  if (property == -1) {
    client.print(buf);
    // send a standard http response header
    client.println(F("HTTP/1.1 400 Bad Request"));
    client.println(F("Content-Type: application/json"));
    client.println(F("Connection: close"));
    client.println();
    client.print(F("{ \"code\": 400, \"message\": \"Could not correctly detect requested property\" }"));
    fail = 1;
  }

  if (!fail) {
    sendNTPpacket(timeServer); // send an NTP packet to a time server
    delay(200);
    timegone = millis();
    while (Udp.parsePacket() == 0) {
      if (millis() - timegone > NTP_TIMEOUT) {
        fail = 1;
        break;
      } else if (millis() - timegone > NTP_TIMEOUT/2) {
        sendNTPpacket(timeServer); // Try and resend
      }
      delay(200); // Do not check continuously
    }
  }

  if (!fail) {
    // We've received a packet, read the data from it
    Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    // The timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, extract the two words
    highWord = word(packetBuffer[40], packetBuffer[41]);
    lowWord = word(packetBuffer[42], packetBuffer[43]);

    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900)
    secsSince1900 = highWord << 16 | lowWord;

    // subtract seventy years:
    epoch = secsSince1900 - seventyYears;
    setTime(epoch);

    // Fill "finresp" with the reponse data

    // The following is to construct a timestamp
    /*finresp.concat(year());
    finresp.concat(F("-"));
    if (month() < 10)
      finresp.concat(F("0"));
    finresp.concat(month());
    finresp.concat(F("-"));
    if (day() < 10)
      finresp.concat(F("0"));
    finresp.concat(day());
    finresp.concat(F("T"));
    if (hour() < 10)
      finresp.concat(F("0"));
    finresp.concat(hour());
    finresp.concat(F(":"));
    if (minute() < 10)
      finresp.concat(F("0"));
    finresp.concat(minute());
    finresp.concat(F(":"));
    if (second() < 10)
      finresp.concat(F("0"));
    finresp.concat(second());
    finresp.concat(F("+00:00"));*/

    finresp.concat(F(" ]"));

    len = finresp.length();

    // send a standard http response header
    client.println(F("HTTP/1.1 200 OK"));
    client.println(F("Content-Type: application/json"));
    client.print(F("Content-Length: "));
    client.println(len);
    client.println(F("Connection: close"));
    client.println();

    j = 0;
    for (i = 0; i < len; i++) {
      buf[j] = finresp.charAt(i);
      j++;
      if (j == MAX_BUF) {
        buf[j] = '\0';
        client.print(buf);
        j = 0;
      }
    }
    if (j > 0) {
      buf[j] = '\0';
      client.print(buf);
    }
  } else if (property != -1) {
    // send a standard http response header
    client.println(F("HTTP/1.1 500 Internal Server Error"));
    client.println(F("Content-Type: application/json"));
    client.println(F("Connection: close"));
    client.println();
    client.print(F("{ \"code\": 500, \"message\": \"Could not get the time\" }"));
  }

  return fail;
}

// send NTP request to the time server at the given address
void sendNTPpacket(IPAddress& address) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  packetBuffer[0] = 0b11100011; // LI, Version, Mode
  packetBuffer[1] = 0; // Stratum, or type of clock
  packetBuffer[2] = 6; // Polling Interval
  packetBuffer[3] = 0xEC; // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp
  Udp.beginPacket(address, 123); // NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

void printWifiStatus() {
  // print the SSID of the network you're attached to
  Serial.print(F("SSID: "));
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address
  IPAddress ip = WiFi.localIP();
  Serial.print(F("IP Address: "));
  Serial.println(ip);

  // print the received signal strength
  long rssi = WiFi.RSSI();
  Serial.print(F("signal strength (RSSI):"));
  Serial.print(rssi);
  Serial.println(F(" dBm"));
}

