#include "mbed.h"
#include "MQTTClientMbedOs.h"
#include "EthernetInterface.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>

// Maximum number of element the application buffer can contain
#define MAXIMUM_BUFFER_SIZE                                                  128

// threads

Thread publishThread;

//network, socket and mqtt
EthernetInterface eth;
TCPSocket socket;
MQTTClient client(&socket);
MQTTPacket_connectData mqttData;

// ip addresses
SocketAddress MQTTBrokerAddress;
SocketAddress DeviceAddress;
SocketAddress BuffAddr;


DigitalOut dataReadyLED(A0);
DigitalIn gpsPosReady(D7, PullDown);    // PPS pin on GPS module. Can be used to create data synchronization, interrupts etc.
DigitalOut errorLED(D8);


// Create a BufferedSerial object with a default baud rate.
static BufferedSerial GpsSerial(PTC17, PTC16);



//functions
void eth_connection_init();
void connectToBroker();
void PublishMsg(char* topic, char* buf);
void PublishHandler();
void Error_Flag(int num);
void print_GpsDATA();
void parse_GpsDATA();
void Read_Gps();
void gpsFormatter(char *rawLat, char *rawLong);
void RST_GpsRxBuffer(void);

struct
{
  char GPS_DATA[80];
  bool GetData_Flag;      //Get GPS data flag bit
  bool ParseData_Flag;    //Parse completed flag bit 
  char UTCTime[11];       //UTC time
  char latitude[11];      //Latitude
  char N_S[2];            //N/S
  char longitude[12];     //Longitude
  char E_W[2];            //E/W
  bool Usefull_Flag;      //If the position information is valid flag bit 
} Save_Data;




//  buffers and related vars
const unsigned int gpsRxBufferLength = 1024;
char gpsRxBuffer[gpsRxBufferLength] = {0};
char tmpBuf[4] = {0};
char msg[256];

char jsonStrForm [] = "{\"gps\":{\"data\":\"%s\", \"UTCtime\":%s, \"latitude\":%s, \"N_S\":\"%s\", \"longitude\":%s, \"E_W\":\"%s\"}}";



unsigned int gpsRxLength = 0;
uint32_t num;

// main() runs in its own thread in the OS
int main()
{
    printf("Program started\n");
    // Set desired properties (9600-8-N-1).
    GpsSerial.set_baud(9600);
    GpsSerial.set_format(
        /* bits */ 8,
        /* parity */ BufferedSerial::None,
        /* stop bit */ 1
    );
    ThisThread::sleep_for(1s);

    // test the LED
    dataReadyLED.write(1);
    ThisThread::sleep_for(500ms);
    //set to zero at start
    dataReadyLED.write(0);
    
    //Network and MQTT setup
    eth_connection_init();
    connectToBroker();
    printf("<<< Network and connection setup is complete >>>\n\r");
    ThisThread::sleep_for(1s);


    Save_Data.GetData_Flag = false;
    Save_Data.ParseData_Flag = false;
    Save_Data.Usefull_Flag = false;

    printf("DFRobot Gps\n");
    printf("Wating...\n");
    Save_Data.GetData_Flag = false;
    Save_Data.ParseData_Flag = false;
    Save_Data.Usefull_Flag = false;
    
    publishThread.start(PublishHandler);

    while (true) {
        Read_Gps();         //Get GPS data 
        parse_GpsDATA();    //Analyze GPS data 
        print_GpsDATA();    //Output analyzed data
    }
    printf("Existed back to main\n"); //normally, it should not ever reach here

}
void eth_connection_init(){
        // Bring up the ethernet interface and connect to network
    eth.connect();
    
    //gather network address, netmask and gateway address and show them
    eth.get_ip_address(&DeviceAddress);
    ThisThread::sleep_for(1s);

    printf("IP address: %s\n", DeviceAddress.get_ip_address() ? DeviceAddress.get_ip_address() : "None");

    eth.get_gateway(&BuffAddr);
    printf("Gateway address: %s\n", BuffAddr.get_ip_address() ?BuffAddr.get_ip_address() : "None");
    
    eth.get_netmask(&BuffAddr);
    printf("Gateway address: %s\n",  BuffAddr.get_ip_address() ? BuffAddr.get_ip_address() : "None");

    // Tell the TCP socket to use the instantiated Ehternet Interface
    socket.open(&eth);
    ThisThread::sleep_for(2s);
}

void connectToBroker(){
    //get the ip address of remote broker host address
    eth.gethostbyname(MBED_CONF_APP_MQTT_BROKER_HOSTNAME, &MQTTBrokerAddress, NSAPI_IPv4, "myEthernet");
    //set port to Broker IP address to be used in MQTTClient 
    MQTTBrokerAddress.set_port(MBED_CONF_APP_MQTT_BROKER_PORT);
    socket.connect(MQTTBrokerAddress);

    //print the Translated IP address of MQTT host Broker
    printf("MQTT host Broker address: %s\n",  MQTTBrokerAddress.get_ip_address() ? MQTTBrokerAddress.get_ip_address() : "None");

    // configuring the settings and options for connecting to broker
    mqttData = MQTTPacket_connectData_initializer;
        // This MQTTClient ID/name
    mqttData.clientID.cstring = MBED_CONF_APP_MQTT_CLIENT_ID;
    
    client.connect(mqttData);
}

void PublishMsg(char* topic, char* buf){
    MQTT::Message msg;
    msg.qos = MQTT::QOS0;
    msg.retained = false;
    msg.dup = false;
    msg.payload = (void*)buf;
    msg.payloadlen = strlen(buf);
    client.publish(topic, msg);
    client.yield(250);
}

void PublishHandler(){
    while (true) {
            if (Save_Data.Usefull_Flag) {
                sprintf(msg, jsonStrForm, Save_Data.GPS_DATA, Save_Data.UTCTime, Save_Data.latitude, Save_Data.N_S, Save_Data.longitude, Save_Data.E_W);
                PublishMsg(MBED_CONF_APP_MQTT_TOPIC, msg);
            }
    }
}

void Error_Flag(int num){
    // printf("Entered Error_Flag(int num)\n");
    printf("ERROR %d\n", num);
    while (true) {
        errorLED = !errorLED;
        ThisThread::sleep_for(500ms);
    }
}


void print_GpsDATA(){
    // printf("Entered print_GpsDATA()\n");
    if (Save_Data.ParseData_Flag)
    {
        Save_Data.ParseData_Flag = false;

        printf("Save_Data.UTCTime = %s\n", Save_Data.UTCTime);


        if(Save_Data.Usefull_Flag)
        {
            gpsFormatter(Save_Data.latitude, Save_Data.longitude);
            Save_Data.Usefull_Flag = false;
            printf("Save_Data.latitude = %s\n", Save_Data.latitude);

            printf("Save_Data.N_S = %s\n", Save_Data.N_S);

            printf("Save_Data.longitude = %s\n", Save_Data.longitude);

            printf("Save_Data.E_W = %s\n", Save_Data.E_W);
            
        }
        else
        {
            printf("GPS DATA is not usefull!\n");
        }    
    }
}


void parse_GpsDATA(){
    // printf("Entered parse_GpsDATA()\n");
    char *subString;
    char *subStringNext;
    if (Save_Data.GetData_Flag)
    {
    Save_Data.GetData_Flag = false;
    printf("************************\n%s\n", Save_Data.GPS_DATA);

    for (int i = 0 ; i <= 6 ; i++)
    {
      if (i == 0)
      {
        if ((subString = strstr(Save_Data.GPS_DATA, ",")) == NULL)
          Error_Flag(1);    //Analysis error 
      }
      else
      {
        subString++;
        if ((subStringNext = strstr(subString, ",")) != NULL)
        {
          char usefullBuffer[2]; 
          switch(i)
          {
            case 1:memcpy(Save_Data.UTCTime, subString, subStringNext - subString);break;    //Get UTC time 
            case 2:memcpy(usefullBuffer, subString, subStringNext - subString);break;        //Get position status 
            case 3:memcpy(Save_Data.latitude, subString, subStringNext - subString);break;   //Get latitude information 
            case 4:memcpy(Save_Data.N_S, subString, subStringNext - subString);break;        //Get N/S
            case 5:memcpy(Save_Data.longitude, subString, subStringNext - subString);break;  //Get longitude information 
            case 6:memcpy(Save_Data.E_W, subString, subStringNext - subString);break;        //Get E/W

            default:break;

          }
          subString = subStringNext;
          Save_Data.ParseData_Flag = true;
          if(usefullBuffer[0] == 'A')
            Save_Data.Usefull_Flag = true;
          else if(usefullBuffer[0] == 'V')
            Save_Data.Usefull_Flag = false;
        }
        
        else
        {
          Error_Flag(2);    //Analysis error
        }
      }
    }
  }

}


void Read_Gps(){

    if (GpsSerial.readable()) {
        num = GpsSerial.read(tmpBuf, 1);
        gpsRxLength = gpsRxLength +  num;
        if (gpsRxLength == gpsRxBufferLength)RST_GpsRxBuffer();
        snprintf(gpsRxBuffer, gpsRxLength, "%s%s", gpsRxBuffer, tmpBuf);

    }
    char *GPS_DATAHead;
    char *GPS_DATATail;

    if ((GPS_DATAHead = strstr(gpsRxBuffer, "$GPRMC,")) != NULL ||
        (GPS_DATAHead = strstr(gpsRxBuffer, "$GNRMC,")) != NULL)
        {

            if (((GPS_DATATail = strstr(GPS_DATAHead, "\r\n")) != NULL) &&
                (GPS_DATATail > GPS_DATAHead))
            {
                memcpy(Save_Data.GPS_DATA, GPS_DATAHead, GPS_DATATail - GPS_DATAHead);
                Save_Data.GetData_Flag = true;

                RST_GpsRxBuffer();
            }
        }
}


void RST_GpsRxBuffer(void){

    memset(gpsRxBuffer, 0, gpsRxBufferLength - 20);
    gpsRxLength = 0;
}

void gpsFormatter(char* rawLat, char* rawLong){
    char buf[12];
    int intLat;
    int intLong;

    float fLat;
    float fLong;
    int i;

    // lat. format conversion
    for (i = 0; i < 2; i++) {
        buf[i] = rawLat[i];
    }

    intLat = atoi(buf);
    memset(buf, 0, 12);

    for (; i < strlen(rawLat); i++) {
        buf[i - 2] = rawLat[i];
    }
    fLat = atof(buf);
    memset(buf, 0, 12);

    // long. format conversion

    for (i = 0; i < 3; i++) {
        buf[i] = rawLong[i];
    }

    intLong = atoi(buf);
    memset(buf, 0, 12);

    for (; i < strlen(rawLong); i++) {
        buf[i - 3] = rawLong[i];
    }
    fLong = atof(buf);
    memset(buf, 0, 12);

    fLat = intLat + (fLat / 60);
    fLong = intLong + (fLong / 60);
    
    memset(Save_Data.latitude, 0, 11);
    memset(Save_Data.longitude, 0, 12);

    sprintf(Save_Data.latitude, "%f", fLat);
    sprintf(Save_Data.longitude, "%f", fLong);
}