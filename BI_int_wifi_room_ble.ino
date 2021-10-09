#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <esp_bt_main.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>

#include <WiFiMulti.h>

#include <HTTPClient.h>
#include <HTTPUpdate.h>

#define HAS_BATTERY_PROBE ADC1_GPIO35_CHANNEL // battery probe GPIO pin -> ADC1_CHANNEL_7
#define BATT_FACTOR 2 // voltage divider 100k/100k on board
#include <Preferences.h>
#include "battery.h"

/*OTA Change History */
/*  1037: History starts now
    1038: Changed Sleep to 2 Minutes (bitbucket test)
    1039: Changed Sleep to 10 Minutes
    1040: Changed Sleep to 0 Minutes
    1041: Test for new UI
    1042: Changed Sleep to 10 Minutes
    1043: testing 10 minutes update on OTA
    1044: testing 0 minutes update on OTA
    1045: testing 10 minutes update on OTA
    1046: Stress Test 0 minutes sleep on OTA
    1047: Switch by to 10 minutes via OTA
    1048: Switch by to 0 minutes via OTA on Win10 BI Laptop
    1049: Switch by to 10 minutes via OTA on Win10 BI Laptop
    1050: Test for ING Node OTA to 0 minutes sleep and adding pub.sh again, by accident deleted once updating to win10
    1051: FIX: DDAC (Dont Drink And Code) Sleeptime wasnt set to 0 :-)
    1052: OTA on all Ing Nodes
    1053: OTA on all BC & Ing Nodes
    1054: OTA on all BC & Ing Nodes to 10 min sleep
    1055: OTA on all BC & Ing Nodes to 0 min sleep
    1056: OTA on all BC & Ing Nodes to 10 min sleep
    1057: OTA on all BC & Ing Nodes to 0 min sleep
    1059: OTA on all BC & Ing Nodes to 10 min sleep
    1060: OTA on all BC & Ing Nodes to 0 min sleep
    1061: OTA on all BC & Ing Nodes to 10 min sleep
    1062: OTA on all BC & Ing Nodes to 0 min sleep
    1063: OTA on all BC & Ing Nodes to 10 min sleep
    1064: OTA on all BC & Ing Nodes to 0 min sleep
    1065: OTA on all BC & Ing Nodes to 10 min sleep
    1066: OTA on all BC & Ing Nodes to 0 min sleep
    1067: OTA on all BC & Ing Nodes to 10 min sleep
    1068: OTA development test
    1069: OTA development test
    1070: OTA development test
    1071: Added const char *DEVICE_NAME_KEEPER = "Gigaset keeper" and adapted
          if ((foundDevices.getDevice(i).getName() == DEVICE_NAME_GTAG) ||
               (foundDevices.getDevice(i).getName() == DEVICE_NAME_KEEPER))
          in funciton do_ble_scan()
    1072: OTA development test
    1073: MQTT Server AWS, JSON Format MQTT
    1074: Changed bufferlength in pubsubclient.h !!!
    1075: initial Test
    1076: move to BI Net
    1077: scantime set bakc to 50s
    1078: scantime set to 60 s, bugfix memory problem json -> increased buffer 1k

/*OTA Parameter ****DANGEROUS****/
const int FW_VERSION = 1078;
#define ML // ! Changed 08.11
#ifdef ML
const char* fwUrlBase = "http://martin-locher.com/ota/";
#else
const char* fwUrlBase = "http://s3.eu-west-1.amazonaws.com/smartland-ota/";
#endif

// Callback function header
void callback(char* topic, byte* payload, unsigned int length);

WiFiClient espClient;
PubSubClient client(espClient);

//Blocking but also deepsleep
#define BLOCKING

const char *DEVICE_NAME_GTAG = "Gigaset G-tag";
//const char *DEVICE_NAME_KEEPER = "Gigaset keeper";
const char *DEVICE_NAME_KEEPER = "Maddin";

//const char * SERVICE_DATA_UUID = "0000feaa-0000-1000-8000-00805f9b34fb";
//must be executed in root
//hciconfig hci0 up
//hciconfig hci0 leadv 3
//hcitool -i hci0 cmd 0x08 0x0008 1a 02 01 06 03 03 aa fe 12 16 aa fe 10 00 01 72 61 73 70 62 65 72 72 79 70 69 01 00 00 00 00 00

const int SCAN_TIME = 60; //CHANGED FROM 50 TO 10 and back to 50 may be have to be increased more 19.08 to 60

//#define BI // ! Changed 08.11
#ifdef BI 
const char * P_SSID = "bi-guest";
const char * P_PASSWORD = "";
#else
const char * P_SSID = "UPC5299841";
const char * P_PASSWORD = "Ximovan777";
#endif

#define MLU ///// ! Changed 08.11
#ifdef MLU
const char * P_MQTT_SERVER = "martin-locher.com";
const int MQTT_PORT = 1883;
//const char * P_MQTT_CHANNEL = "honk";// ! Changed 08.11
//const char * P_MQTT_PASSWD = "Ximovan777";
const char * P_MQTT_CHANNEL = "isee";
const char * P_MQTT_PASSWD = "isee";
#else
const char * P_MQTT_SERVER = "ec2-34-247-20-141.eu-west-1.compute.amazonaws.com";
const int MQTT_PORT = 1883;
const char * P_MQTT_CHANNEL = "honkidonk";
const char * P_MQTT_PASSWD = "Wa11aBi11a";
#endif


//const char * P_TOPIC = "martin/";
const char * P_SUBTOPIC_BLE = "ble";
const char * P_SUBTOPIC_VLT = "voltage";
const char * P_SUBTOPIC_VERS = "version";

long lastMsg = 0;

#define BIBERNODE  "BIBERNODE/"

#define ONE_MINUTE 60
RTC_DATA_ATTR unsigned TX_INTERVAL = 0; //in minutes !!! CHANGED
#define uS_TO_S_FACTOR 1000000


char * get_wlan_mac(char * p_buff)
{
  byte mac[6];
  WiFi.macAddress(mac);

  sprintf (p_buff, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.println(p_buff);

  return (p_buff);
}

void create_ble_topic (char * p_dest)
{
  char a_mac[30];

  //sprintf(p_dest, "%s%s%s/%s", P_TOPIC, BIBERNODE, get_wlan_mac(a_mac), P_SUBTOPIC_BLE);
  sprintf(p_dest, "%s/%s", get_wlan_mac(a_mac), P_SUBTOPIC_BLE);
}

void create_voltage_topic (char * p_dest)
{
  char a_mac[30];

  //sprintf(p_dest, "%s%s%s/%s", P_TOPIC, BIBERNODE, get_wlan_mac(a_mac), P_SUBTOPIC_VLT);
  sprintf(p_dest, "%s/%s", get_wlan_mac(a_mac), P_SUBTOPIC_VLT);
}

void create_version_topic (char * p_dest)
{
  char a_mac[30];

  //sprintf(p_dest, "%s%s%s/%s", P_TOPIC, BIBERNODE, get_wlan_mac(a_mac), P_SUBTOPIC_VERS);
  sprintf(p_dest, "%s/%s", get_wlan_mac(a_mac), P_SUBTOPIC_VERS);
}



class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    /**
        Called for each advertising BLE server.
    */
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
      Serial.printf("Advertised Device: %s \n", advertisedDevice.toString().c_str());
    }
};
char a_buff [10000];

int do_ble_scan()
{
  BLEAdvertisedDevice device;
  uint8_t * macptr;

  Serial.println("Starting BLE scan...");
  Serial.printf("Scan will take %d secs\n", SCAN_TIME);
  BLEDevice::init("");
  BLEScan* pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(false); //active scan uses more power, but get results faster
  BLEScanResults foundDevices = pBLEScan->start(SCAN_TIME);

  int n = foundDevices.getCount();
  int rssi=0;
  int act_cnt = 0;
  int real_devs = 0;
  char a_buff[100];
  char a_value[3048];
  char a_wifi_mac[50];
  char a_version [20];
  char a_temp [20];



  esp_bluedroid_disable();
  esp_bluedroid_deinit();
  esp_bt_controller_disable();
  esp_bt_controller_deinit();

  esp_bt_mem_release(ESP_BT_MODE_BTDM);


  reconnect();
  create_ble_topic (a_buff);

  DynamicJsonDocument doc(3048);
  doc["TRANSPONDER_MAC"] = get_wlan_mac (a_wifi_mac);
  JsonArray nested = doc.createNestedArray("BLE");


   
  for (int i = 0; i < n; i++)
  {
    // Serial.println (foundDevices.getDevice(i).getName().c_str());   
   if ((foundDevices.getDevice(i).getName() == DEVICE_NAME_GTAG) ||
        (foundDevices.getDevice(i).getServiceUUID().toString() == "4fafc201-1fb5-459e-8fcc-c5c9c331914b") ||
        (foundDevices.getDevice(i).getName() == DEVICE_NAME_KEEPER)) 
    {
      Serial.print("**** "); Serial.print(act_cnt); Serial.println(" ****");
      Serial.println(foundDevices.getDevice(i).getName().c_str());
      Serial.print ("Device Mac: ");
      Serial.println(foundDevices.getDevice(i).getAddress().toString().c_str());

      
      Serial.print("**** "); Serial.print(act_cnt); Serial.println(" ****");
      Serial.print ("UUID: ");
      Serial.println(foundDevices.getDevice(i).getServiceUUID().toString().c_str());
      Serial.println ("Additional Meta Data:");
      
      
      Serial.println ("Additional Meta Data:");


      if (foundDevices.getDevice(i).haveAppearance()) {
        Serial.println ("Have appearance");
        // Serial.println (foundDevices.getDevice(i).getAppearance());
      } // haveAppearance

      if (foundDevices.getDevice(i).haveManufacturerData()) {
        Serial.println ("Have manufacturer-data");
        //Serial.println (foundDevices.getDevice(i).getManufacturerData().c_str());
      } // haveManufacturerData

      if (foundDevices.getDevice(i).haveRSSI())
      {
        Serial.print ("RSSI: ");
        Serial.println (foundDevices.getDevice(i).getRSSI());
      }

      if (foundDevices.getDevice(i).haveServiceData())
      {
        Serial.println ("Have Service Data");
        //Serial.println (foundDevices.getDevice(i).getServiceData().c_str());
      }

      if (foundDevices.getDevice(i).haveTXPower())
      {
        Serial.println ("Have ServiceTXPower");
        //Serial.println (foundDevices.getDevice(i).getTXPower());
      }

      if (foundDevices.getDevice(i).haveRSSI())
      {
        if (strlen((foundDevices.getDevice(i).getAddress()).toString ().c_str()))
        {
        char a_ble_mac [100];

        sprintf(a_ble_mac, "%s", (foundDevices.getDevice(i).getAddress()).toString ().c_str());
        rssi = foundDevices.getDevice(i).getRSSI();
        char a_rssi [100];
        sprintf (a_rssi, "%d", foundDevices.getDevice(i).getRSSI());

        JsonObject objs  = nested.createNestedObject();
         act_cnt ++;
        if (i > 29)
        {
        Serial.println ("BLE_MAC:");
        Serial.println (a_ble_mac);

        Serial.println ("RSSI:");
        Serial.println (a_rssi);
        }
        objs["MAC"] = a_ble_mac;
        objs["RSSI"] = a_rssi;

        //        nested.add (objs);
        }


      }
      else
      {
        /*
        client.publish(a_buff, foundDevices.getDevice(i).getAddress().toString().c_str());
        Serial.println("FUCKED"); */
      }
     
      delay (100);

    }
  } 
 
  Serial.println("******************************************************************");
  Serial.print("act_cnt = "); Serial.println(act_cnt);
  Serial.println("******************************************************************");
  
  sprintf (a_version, "%d", FW_VERSION);
  doc["VERSION"] =  a_version;

  //int randNumber = random(18, 22);

  sprintf (a_temp,"%d", rssi);
  doc["RSSI"] =  a_temp;
  
  serializeJson(doc, a_value);

  Serial.println("Sending message to MQTT topic..");
  Serial.println(a_value);
  
  if (!client.publish(a_buff, a_value))
    Serial.println ("Publish ERROR!!!");
    
  Serial.print ("Published: ");
  Serial.print (a_buff);
  Serial.print (":");
  Serial.println(a_value);

  disconnect ();
  //esp_bt_mem_release(ESP_BT_MODE_BTDM);
  return act_cnt;
}


void setup_wifi()
{
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(P_SSID);


  WiFi.begin (P_SSID, P_PASSWORD);
  Serial.println();
  Serial.println();
  Serial.print("Wait for WiFi... ");

  int i = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    i++;
    if (i > 40) // bug from esp32 after deepsleep is the wifi chip not in a stable state :-(
    {
      Serial.println ("Hardware reset");
      digitalWrite (19, LOW);
      ESP.restart();
    }
     
    Serial.print(".");
    delay(500);
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* message, unsigned int length)
{
  Serial.print("Message arrived on topic : ");
  Serial.print(topic);
  Serial.print(". Message : ");

  String messageTemp;

  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }

  Serial.println();

  /*
    if (String(topic) == "ESP32/output")
    {

    }*/
}

void disconnect()
{
  client.disconnect ();
}

void reconnect() {
  // Loop until we're reconnected
  int i = 0;
  char a_buff[255];

  client.setServer(P_MQTT_SERVER, MQTT_PORT);
  
#ifndef BLOCKING
  client.setCallback(callback);
#endif

  while (!client.connected())
  {
    i ++;

    if (i > 10)
      ESP.restart();

    Serial.print("Attempting MQTT connection...");
    
    if (client.connect(get_wlan_mac(a_buff), P_MQTT_CHANNEL, P_MQTT_PASSWD))
    {
      Serial.println("Connected !");
      create_ble_topic (a_buff);
      if (client.subscribe(a_buff, 1))
      {
        Serial.println("Subscribed !");
      }
      else
      {
        Serial.println("Subscribed FAILED !!!");
      }

    }
    else
    {
      Serial.print("failed, rc = ");
      Serial.print(client.state());
      Serial.print(" ");
      Serial.print(P_MQTT_SERVER);
      Serial.print(" ");
      Serial.print(P_MQTT_PASSWD);
      Serial.print(" ");
      Serial.print(P_MQTT_CHANNEL);
        Serial.print(" ");
      Serial.print(MQTT_PORT);
      Serial.println(" try again in 5 seconds");

      delay(5000);
    }
  }
}

void setup()
{
  Serial.begin(115200);
  delay(5000);

  Serial.printf ("****FirmWare Version: %d****\n", FW_VERSION);

  calibrate_voltage();

  setup_wifi ();

  client.setServer(P_MQTT_SERVER, MQTT_PORT);
  reconnect ();
  Serial.printf("Going into loop, depending if no block is enabled expect wait time for %d secs\n", (TX_INTERVAL * ONE_MINUTE));
}

void checkForUpdates(String url)
{

  String fwURL = String( url );
  String fwVersionURL = fwURL;
  fwVersionURL.concat( "all.version" );

  Serial.println( "Checking for firmware updates." );
  Serial.print( "Firmware version URL: " );
  Serial.println( fwVersionURL );

  WiFiClient client;
  HTTPClient httpClient;
  httpClient.begin( fwVersionURL );
  int httpCode = httpClient.GET();
  if ( httpCode == 200 ) {
    String newFWVersion = httpClient.getString();

    Serial.print( "Current firmware version: " );
    Serial.println( FW_VERSION );
    Serial.print( "Available firmware version: " );
    Serial.println( newFWVersion );

    int newVersion = newFWVersion.toInt();

    if ( newVersion > FW_VERSION ) {
      Serial.println( "Preparing to update" );

      String fwImageURL = fwURL;
      fwImageURL.concat( "all.bin" );
      client.setTimeout(12000);
      t_httpUpdate_return ret = httpUpdate.update(client, fwImageURL );
      Serial.printf ("Http ret value: %d\n", ret);


      switch (ret) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
          break;

        case HTTP_UPDATE_NO_UPDATES:
          Serial.println("HTTP_UPDATE_NO_UPDATES");
          break;
      }
    }
    else
    {
      Serial.println( "Already at latest release" );
    }
  }
  else
  {
    Serial.print( "Firmware version check failed, got HTTP response code " );
    Serial.println( httpCode );
  }
  httpClient.end();
}

bool checkForFileExistance(String p_path)
{
  String complete_path = p_path;
  complete_path.concat("all.version");
  bool retVal = false;

  HTTPClient httpClient;
  httpClient.begin( complete_path );
  int httpCode = httpClient.GET();
  if ( httpCode == 200 )
  {
    retVal = true;
  }
  else
  {
    retVal = false;
  }

  return (retVal);

}



#ifdef BLOCKING
void loop ()
{
  String fwURL = String(fwUrlBase);
  String fwVersionURL = fwURL;
  char a_buff[20];
  Serial.println(get_wlan_mac(a_buff));
  fwVersionURL.concat(a_buff);
  fwVersionURL.concat("/");

  if (checkForFileExistance(fwVersionURL) == true)
  {
    Serial.println("#########################DEV");
    checkForUpdates(fwVersionURL);
  }
  else
  {
    Serial.println("#########################PROD");
    checkForUpdates(fwUrlBase);
  }

  do_ble_scan();

  Serial.println();
  Serial.printf("Going into Deepsleep for %d secs\n", (TX_INTERVAL * ONE_MINUTE));

  delay (100);
  client.loop();
  delay (100);

  esp_sleep_enable_timer_wakeup(TX_INTERVAL * ONE_MINUTE * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}
#else

void loop ()
{

  long now = millis();

  if (now - lastMsg > TX_INTERVAL * ONE_MINUTE * 1000)
  {
    lastMsg = now;

    checkForUpdates();

    do_ble_scan();

    Serial.printf("Going into into no block mode, expect wait time for %d secs\n", (TX_INTERVAL * ONE_MINUTE));
  }
  else
  {
    client.loop();
  }

}
#endif
