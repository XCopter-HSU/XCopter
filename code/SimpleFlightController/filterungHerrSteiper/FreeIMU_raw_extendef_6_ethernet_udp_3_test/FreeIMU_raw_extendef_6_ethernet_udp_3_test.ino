#include <Dhcp.h>
#include <Dns.h>
#include <Ethernet.h>
#include <EthernetClient.h>
#include <EthernetServer.h>
#include <EthernetUdp.h>
#include <util.h>

#include <SPI.h>
#include "I2Cdev.h"
#include <Wire.h>
#include <ADXL345.h>
#include <HMC58X3.h>
#include <ITG3200.h>
//#define DEBUG
#include "DebugUtils.h"
#include "FreeIMU.h"
#include <math.h>

/* Important: On Arduino Nano V.3 SDA is Analog Pin 4 and SCL is Analog Pin 5.
 * On Arduino Mega SCL is Pin 21 and SDA is Pin 20
 */

#define W5200_CS  10
#define SDCARD_CS 4
#define HW_CS     53

#define highWord(w) ((w) >> 16)       // get 2 higher bytes of a long variable
#define lowWord(w) ((w) & 0xffff)     // get 2 lower bytes of a long variable

#define outbufLength 36
#define dataNr       7                // number of smoothed parameters from Android device
#define numOfSamples 4                // number of smoothed sensor measurements
#define inbufLength  dataNr*4
#define numOfSenData 9                // number of sensed input parameters per sample
#define deadline     16000
#define samplerate   20000
#define tmax	     2147483647       // max. value of unsingned long variable

byte  to_value         = 0;
float kalmanData[dataNr];             // stores received filtered kalman data from Android device
byte  outbuf[outbufLength];           // stores output data for Serial interface
byte  inbuf[inbufLength];
int   raw_values[11];                 // stores raw sensor date: angle_x/y/z, omega_/x/y/z, magnet_x/y/z
int   raw_values_acc[11]     =  {0,0,0,0,0,0,0,0,0,0,0};        // stores accumulated raw_values: angle_x/y/z, omega_/x/y/z, magnet_x/y/z
int   raw_values_acc_old[11] =  {0,0,0,0,0,0,0,0,0,0,0};        // stores accumulated raw_values of foregoing epoche

const float gyroSens  = 14.375;               // convert lsb to rad/sec (gyro output)
const float dtor      = M_PI/180.0F;          // convert degree to rad
const float rtod      = 180.0F/M_PI;          // convert rad to degree
const float step      = samplerate/1000000.0; // time steps in units of seconds

int   counter         = -1;
int   tag             = 32767; 
int   buffIndex       = 0;
int   delIndex        = 0;
int   bytecount       = 0;
int   bytenr          = 0;

int    ax_offset      = +10.0;        // calibration of accelerometer values:
int    ay_offset      =  +0.0;        // (accelX*accelX) + (accelY*accelY) * (accelZ*accelZ) should be independant of the orientation of the accelerometer
int    az_offset      = -18.0;
float  ax_scale       = 0.983;
float  ay_scale       = 1.000;
float  az_scale       = 1.010;

boolean timeout       = false;        // whether data from android are delayed
boolean dataValid     = true;         // whether the delivered data are valid

unsigned int loWord;                  // used to store 2 lower bytes of a long variable
unsigned int hiWord;                  // user to store 2 higher bytes of a long variable

unsigned long t_i     = 0;
unsigned long t_o     = 0;
unsigned long dt      = 0;
boolean first         = true;
boolean reduced[3]    = {
  false, false, false};


float x_m_inc[3] = {0.0, 0.0, 0.0};
long  yaw_inc    = 0;

float x_m[3]     = {0.0, 0.0, 0.0};
float v_m[3]     = {0.0, 0.0, 0.0};
float vo_m[3]    = {0.0, 0.0, 0.0};

float x_p[3]     = {0.0, 0.0, 0.0};
float v_p[3]     = {0.0, 0.0, 0.0};

float x_d[3]     = {0.0, 0.0, 0.0};
float v_d[3]     = {0.0, 0.0, 0.0};

float x_s[3]     = {0.0, 0.0, 0.0};
float v_s[3]     = {0.0, 0.0, 0.0};

float r_f[3]     = {0.3, 0.3, 0.3};
float r_a[3]     = {0.4, 0.4, 0.4};
float r_b[3]     = {0.1, 0.1, 0.1};

float angle[3]   = {0.0, 0.0, 0.0};
float omega[3]   = {0.0, 0.0, 0.0};
float mag[3]     = {0.0, 0.0, 0.0};

float angle_p[3] = {0.0, 0.0, 0.0};
float omega_p[3] = {0.0, 0.0, 0.0};

union u_tag {
  byte b[4];
  float fval;
} 
u;

//Enter MAC address and IP address for the Ethernet shield.
byte           macLocal[]  = {0x00, 0xAB, 0xBC, 0xCD, 0xDE, 0x01};
byte           ipLocal[]   = {192, 168, 0, 2};
byte           ipServer[]  = {192, 168, 0, 1};
byte           subnet[]    = {255,255,255,0};
unsigned int   portServer  = 50000;
unsigned int   portLocal   = 50000;
EthernetUDP    Udp;
EthernetClient ethClient;

// Set the imu default object
FreeIMU my3IMU = FreeIMU();



void getRawData(float angle[], float omega[], float mag[], const int raw_values[]) { 
  //
  float accelX   =  ((float) ( raw_values[1] - ax_offset)) * ax_scale;    // calibration of x-accelerometer
  float accelY   =  ((float) (-raw_values[0] - ay_offset)) * ay_scale;    // calibration of y-accelerometer
  float accelZ   =  ((float) (-raw_values[2] - az_offset)) * az_scale;    // calibration of z-accelerometer
  float g        =   sqrt(accelX*accelX + accelY*accelY + accelZ*accelZ); // the measured value of g should be independant of the sensors orientation
  //
  accelX         =   accelX / g;
  accelY         =   accelY / g;
  accelZ         =   accelZ / g;
  //
  float omegaX   =  -raw_values[4] * dtor / gyroSens; // [rad/s]
  float omegaY   =   raw_values[3] * dtor / gyroSens; // [rad/s]
  float omegaZ   =   raw_values[5] * dtor / gyroSens; // [rad/s]
  //
  float magnetX  =  raw_values[6];
  float magnetY  =  raw_values[7];
  float magnetZ  =  raw_values[8];
  //
  angle[0]       =  atan2(  accelY, accelZ ); // Roll angle around x axis, [-180, 180] deg, numerically unstable
  angle[1]       =  atan2( -accelX, ((accelY*sin(angle[0])) + (accelZ*cos(angle[0]))) );	// Pitch angle around y axis, restricted to [-90, 90] deg  
  angle[2]       =  atan2( (magnetZ*sin(angle[0])-magnetY*cos(angle[0])), (magnetX*cos(angle[1]) + (magnetY*sin(angle[1])*sin(angle[0])) + (magnetZ*sin(angle[1])*cos(angle[0])) ) ); // Yaw angle around z axis. [-180, 180] deg
  //
  omega[0]       =  omegaX;
  omega[1]       =  omegaY;
  omega[2]       =  omegaZ;
  //
  mag[0]         =  magnetX;
  mag[1]         =  magnetY;
  mag[2]         =  magnetZ;
  //
}




void getFilteredData(float angle_p[], float omega_p[], float angle_m[], float omega_m[], float step) {

  if (first) {
    for (int i=0; i<3; i++){
      x_m[i]     = angle_m[i];
      x_p[i]     = angle_m[i];
      v_p[i]     = omega_m[i];
      angle_p[i] = x_p[i];   
      omega_p[i] = v_p[i];
      first = false;
    }
  }
  else {
    for (int i=0; i<3; i++) {
      if (dataValid) { 
        vo_m[i] = kalmanData[i+3]; 
      }
      v_m[i]    = omega_m[i] - vo_m[i];                           //adjust measured omega values for sensor offset  
      x_m[i]   += (v_m[i] * step);                                //integrate adjusted omega values to calculate angles
      //
      if (x_m[i] < -(180.0 * dtor)) x_m[i] += (360.0 * dtor);     // -180 deg. <= x_m[i]*rtod <= +180 deg.
      if (x_m[i] >  (180.0 * dtor)) x_m[i] -= (360.0 * dtor);
      if (i==1) {                                                 // pitch angle
        if (x_m[i] > (90.0 * dtor)) {                             // transformation of "0 deg.->90 deg.->180 deg." to "0 deg.->90 deg.->0 deg."
          x_m[i] = (180.0 * dtor) - x_m[i];                        
        }
        else if (x_m[i] < -(90.0 * dtor)) {                       // transformation of "0 deg.->-90 deg.->-180 deg." to "0 deg.->-90 deg.->0 deg."
          x_m[i] = -(180.0 * dtor) - x_m[i];
        }
      }  
      //
      if (dataValid) { 
        angle_m[i] = kalmanData[i]; 
      }
      //
      float diff = angle_m[i] - x_m[i];
      if (diff >= (180.0 * dtor)) {
        x_m[i]  = angle_m[i] + r_f[i] * ( (360.0 * dtor) - diff); 
      } 
      else if (diff <= -(180.0 * dtor)) {
        x_m[i]  = angle_m[i] + r_f[i] * (-(360.0 * dtor) - diff); 
      }
      else {   // abs(diff) <   (180.0 * dtor)
        x_m[i]  = (r_f[i]*angle_m[i]) + ((1-r_f[i])*x_m[i]); 
      } 
      //
      if (x_m[i] < -(180.0 * dtor)) x_m[i] += (360.0 * dtor);    // -180 deg. <= x_m[i]*rtod <= +180 deg.
      if (x_m[i] >  (180.0 * dtor)) x_m[i] -= (360.0 * dtor);
      if (i==1) {                                                 // pitch angle
        if (x_m[i] > (90.0 * dtor)) {                             // transformation of "0 deg.->90 deg.->180 deg." to "0 deg.->90 deg.->0 deg."
          x_m[i] = (180.0 * dtor) - x_m[i];                        
        }
        else if (x_m[i] < -(90.0 * dtor)) {                      // transformation of "0 deg.->-90 deg.->-180 deg." to "0 deg.->-90 deg.->0 deg."
          x_m[i] = -(180.0 * dtor) - x_m[i];
        }
      }        
      //
      x_d[i] = x_m[i] - x_p[i];
      v_d[i] = v_m[i] - v_p[i];
      //
      if (x_d[i] < -(180.0 * dtor)) x_d[i] += (360.0 * dtor); // correct for discont. +180 deg. <-> -180 deg.
      if (x_d[i] >  (180.0 * dtor)) x_d[i] -= (360.0 * dtor); // always use shortest distance
      //
      if((abs(x_d[i]) <= 1.0*dtor) && (reduced[i]==false)) {
        r_a[i] = r_a[i] / 2.0;
        r_b[i] = r_b[i] / 2.0;
        reduced[i] = true;
      }
      else if ((abs(x_d[i]) > 1.0*dtor) && (reduced[i]==true)) {
        r_a[i] = r_a[i] * 2.0;
        r_b[i] = r_b[i] * 2.0;
        reduced[i] = false;
      }
      //
      x_s[i]     = x_p[i] + ( r_a[i] * x_d[i]); 
      v_s[i]     = v_p[i] + ( r_b[i] * v_d[i]); 
      //
      x_p[i]     = x_s[i] + (step * v_s[i]);
      //
      if (x_p[i] < -(180.0 * dtor)) x_p[i] += (360.0 * dtor);                     // -180 deg. <= x_p[i]*rtod <= +180 deg.
      if (x_p[i] >  (180.0 * dtor)) x_p[i] -= (360.0 * dtor);  
      if (i==1) {                                                 // pitch angle
        if (x_p[i] > (90.0 * dtor)) {                             // transformation of "0 deg.->90 deg.->180 deg." to "0 deg.->90 deg.->0 deg."
          x_p[i] = (180.0 * dtor) - x_p[i];                        
        }
        else if (x_p[i] < -(90.0 * dtor)) {                      // transformation of "0 deg.->-90 deg.->-180 deg." to "0 deg.->-90 deg.->0 deg."
          x_p[i] = -(180.0 * dtor) - x_p[i];
        }
      }  
      //
      v_p[i]     = v_s[i];
      //
      angle_p[i] = x_p[i];   
      omega_p[i] = v_p[i];
    } 
  }
}




void setup() { 
  // 
  pinMode(SDCARD_CS,OUTPUT);
  pinMode(W5200_CS,OUTPUT);
  digitalWrite(SDCARD_CS,HIGH);//Deselect the SD card reader
  digitalWrite(W5200_CS,LOW);//Select ethernet module
  pinMode(HW_CS,OUTPUT);
  //
  Serial.begin(115200);
  Serial.println("starting...");
  //
  Wire.begin();
  delay(500);
  //
  my3IMU.init(true); // the parameter enables or disables fast mode
  delay(5000);
  //  
  // if (!Ethernet.begin(macLocal)) { // attempt a DHCP connection setup as default  
  Ethernet.begin(macLocal, ipLocal);  // if DHCP fails, use hard coded address
  // };
  Udp.begin(portLocal);
  delay(1000);
}



void loop() {
  //
  t_o   = t_i;                                 // save foregoing timestamp 
  t_i   = micros();                            // get actual timestamps
  if (t_i >= t_o) {
    dt = t_i - t_o;                            // sample time in units of microseconds
  }
  else {
    dt = tmax - t_o + t_i; 
  }

  //
  if (counter==tag) {
    counter=-1;
  }
  counter++;   
  //  
  for (byte j=0; j<numOfSenData; j++) {
    raw_values_acc_old[j] = raw_values_acc[j]; // save old values
    raw_values_acc[j]     = numOfSamples/2;    // reset values
  }
  for (byte i=0; i<numOfSamples; i++) {  
    my3IMU.getRawValues(raw_values);
    for (byte j=0; j<numOfSenData; j++) {
      raw_values_acc[j] += raw_values[j];
    }
  }
  for (byte j=0; j<numOfSenData; j++) {
    raw_values_acc[j] /= numOfSamples ;
  }   
  //
  outbuf[0] = lowByte(tag);
  outbuf[1] = highByte(tag);
  outbuf[2] = lowByte(counter);
  outbuf[3] = highByte(counter);
  //
  hiWord    = highWord(t_i/2);
  loWord    = lowWord(t_i/2); 
  outbuf[4] = lowByte(loWord);
  outbuf[5] = highByte(loWord);
  outbuf[6] = lowByte(hiWord);
  outbuf[7] = highByte(hiWord);
  //
  for (unsigned int i=4; i<13; i++) {
    if (raw_values_acc[i-4] == tag) {
      raw_values_acc[i-4]--;
    }//reduce raw sensor output by 1 if equal max. value of integer! 
    outbuf[2*i]   = lowByte(raw_values_acc[i-4]);           //because start of packet needs to be unique!
    outbuf[2*i+1] = highByte(raw_values_acc[i-4]);
  }
  //
  for (unsigned int i=13; i<16; i++) {
    outbuf[2*i]   = lowByte((int)(angle_p[i-13]  * rtod));
    outbuf[2*i+1] = highByte((int)(angle_p[i-13] * rtod));
  }  
  //
  for (int i=0; i<3; i++) {    
      //x_m_inc[i] +=((omega[i] - vo_m[i]) * step);
      x_m_inc[i] +=((omega[i] - vo_m[i])  * (dt/1000000.0));
  }
  yaw_inc = (long) (x_m_inc[2]*rtod*1000.0);
  hiWord    = highWord(yaw_inc);
  loWord    = lowWord(yaw_inc); 
  outbuf[32] = lowByte(loWord);
  outbuf[33] = highByte(loWord);
  outbuf[34] = lowByte(hiWord);
  outbuf[35] = highByte(hiWord);
  //
  Udp.beginPacket(ipServer, portServer);
  Udp.write(outbuf,outbufLength);
  Udp.endPacket();

  //
  timeout   = true;
  dataValid = false;
  //
  
  getRawData(angle, omega, mag, raw_values_acc);
  //
  for (int i=0; i<dataNr; i++) {
    kalmanData[i] = 0.0;
  }
  //
  
  while (micros() - t_i < deadline) {  // read data from host

    bytecount = Udp.parsePacket();
    bytenr    = Udp.available();
    if (bytenr==(dataNr*4)) {
      Udp.read(inbuf,dataNr*4);
      for (unsigned int j=0; j<dataNr; j++) {
        for (unsigned int k=0; k<4; k++) {
          u.b[k] = inbuf[j*4+k];
        }
        kalmanData[j] = u.fval; 
      }
      timeout = false;
    }
    else{
      if (bytenr>0) {
        for (unsigned int i=0; i<bytenr; i++){
          Udp.read();
        } 
        timeout = false;
      }
    }
    if (((int)kalmanData[dataNr-1])==counter) break;
     
  }
  
  if (((int)kalmanData[dataNr-1])==counter) {
    dataValid = true;
  }
  
  Serial.print("\nbyteount = ");
  Serial.print(bytecount);
  Serial.print("\t");
  //
  if (timeout) {
    Serial.print("\ttimeout observed ");
  }
  //
  getFilteredData(angle_p, omega_p, angle, omega, (float) dt/1000000.0);
  //
  
  for (unsigned int j=0; j<3; j++) {
    Serial.print(kalmanData[j] * rtod);
    Serial.print("\t");
  }
 /* 
  Serial.print("\t");
  for (unsigned int j=0; j<3; j++) {
    Serial.print(kalmanData[j+3] * rtod);
    Serial.print("\t");
  }
  */
  Serial.print("\t");
  for (unsigned int j=0; j<3; j++) {
    Serial.print(angle_p[j] * rtod);
    Serial.print("\t");
  } 
  Serial.print("\tdt =");
  Serial.print(dt/1000000.0);
  Serial.print("\tstep =");
  Serial.print(step);
  Serial.print("\tdataValid =");
  Serial.print(dataValid);

  // force predefined sampling rate: 
  // actual time micros() - t_i must be smaller than samplerate!
  long time = samplerate - (micros() - t_i) - 520;
  Serial.print("\tdt_offset =");
  Serial.print(time);
  if (time>=0) delayMicroseconds((unsigned long)time);
}

