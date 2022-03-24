#include "mbed.h"
#include <cstdio>
#include <cstring>


// code is based on Arduino example, found in: https://wiki.dfrobot.com/GPS_+_BDS_BeiDou_Dual_Module_SKU_TEL0132


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


DigitalOut errorLED(D8);
DigitalIn gpsPosReady(D7, PullDown);

const unsigned int gpsRxBufferLength = 1024;
char gpsRxBuffer[gpsRxBufferLength] = {0};
char tmpBuf[4] = {0};

unsigned int gpsRxLength = 0;
uint32_t num;

static UnbufferedSerial GpsSerial(PTC17, PTC16);

//functions
void Error_Flag(int num);
void print_GpsDATA();
void parse_GpsDATA();
void Read_Gps();
void RST_GpsRxBuffer(void);


// main() runs in its own thread in the OS
int main()
{
    printf("DFRobot Gps\n");
    printf("Wating...\n");
    
    Save_Data.GetData_Flag = false;
    Save_Data.ParseData_Flag = false;
    Save_Data.Usefull_Flag = false;


    while (true) {
        Read_Gps();         //Get GPS data 
        parse_GpsDATA();    //Analyze GPS data 
        print_GpsDATA();    //Output analyzed data 
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