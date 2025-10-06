#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <iostream>
#include <mosquitto.h>
#include <cstring>
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "../src/xplaneConnect.h"
#ifdef WIN32
#include <Windows.h>
#define sleep(n) Sleep(n * 1000)
#endif

#define ROWS 3
#define COLUMNS 9

int publisher(std::string payload);
int readSteamUserName();

const char* client_id = "1234";
const char* host = "test.mosquitto.org";
unsigned int port_mqtt = 1883;
const char* username = "Javier";
const char* password = "xX1234Xx";


const char* xpIP = "127.0.0.1"; //IP Address of computer running X-Plane
unsigned short xpPort = 49009;  //default port number XPC listens on
unsigned short port = 49003;    //port number to which X-Plane is set to send UDP packets

std::string steamUserName;
int speed;
int altitude;
float lat;
float lon;
float hdg;

int main()
{
    printf("Start clientXplane\n\n");

    printf("Start mosquitto MQTT\n\n");
    mosquitto_lib_init();

    printf("Read Steam User\n\n");
    readSteamUserName();

    while (1) {

        // Open Socket
        XPCSocket sock = aopenUDP(xpIP, xpPort, port); //docs seem to be outdated on these...

        // Read 2 rows of data
        
        float data[ROWS][COLUMNS]; //data[0] is dataset index number, data[1] to data[9] are the contents of the dataset
        int error = readDATA(sock, data, ROWS);
        
        if (error != -1) {
            //printf("\nlat = %f, lon = %f, alt = %d, speed = %f", lat, lon, altitude, speed);

            speed = data[0][2];
            altitude = (int)data[2][3];
            lat = data[2][1];
            lon = data[2][2];
            hdg = data[1][3];
        }
        else {
            printf("\nSimulator not running");
            speed = 0;
            altitude = 0;
            lat = 37.2828;
            lon = -5.92088;
            hdg = 0;
        }
        

        closeUDP(sock);
       


        
            
        json jsonPayload;

        jsonPayload["aircraft"] = steamUserName;
        jsonPayload["altitude"] = altitude;
        jsonPayload["heading"] = hdg;
        jsonPayload["latitude"] = lat;
        jsonPayload["longitude"] = lon;
        jsonPayload["speed"] = speed;

        std::string mqttPayload = jsonPayload.dump();
        publisher(mqttPayload);
        Sleep(10);        
    }
    return 0;
}



int publisher(std::string payload) {

    
    // Init
    mosquitto_lib_init();
    mosquitto* mosq = mosquitto_new("simpleClient", false, nullptr);
    if (!mosq) {
        std::cerr << "Could not create client\n";
        return 1;
    }

    // TLS
    if (mosquitto_tls_insecure_set(mosq, true) != MOSQ_ERR_SUCCESS) {
        std::cerr << "Failed to set TLS\n\n";
        return 1;
    }

    // Connect
    int rc = mosquitto_connect(mosq, host, port_mqtt, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "Connect failed: " << mosquitto_strerror(rc) << "\n";
        return 1;
    }
    std::cout << "\nConnected";

    // Publish one message (QoS 1, wait for delivery)
    std::string topicStr = "flights/"+steamUserName;

    const char* topic = topicStr.c_str();


    const char* msg = payload.c_str();
    printf(msg);
    rc = mosquitto_publish(mosq, nullptr, topic, std::strlen(msg), msg, 1, false);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "Publish failed: " << mosquitto_strerror(rc) << "\n";
    }
    else {
        std::cout << "Message published\n\n";
    }

    mosquitto_log_callback_set(mosq, [](mosquitto*, void*, int level, const char* str) {
        std::cout << "[mosq] " << str << std::endl;
        });

    // Process network events briefly (needed for QoS>0 to complete handshake)
    for (int i = 0; i < 10; i++) {
        mosquitto_loop(mosq, 100, 1);  // run network loop for 100ms
    }

    // Disconnect
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    std::cout << "Done\n";
    
    return 0;

}
int readSteamUserName() {
    std::ifstream inputFile("C:\\Program Files (x86)\\Steam\\config\\loginusers.vdf"); // Replace with your file name

    if (!inputFile) {
        std::cerr << "Error opening file!" << std::endl;
        return 1;
    }

    std::string line;
    std::string username;
    while (std::getline(inputFile, line)) {
        std::cout << line << std::endl; // Print each line to console
        if (line.find("PersonaName") != std::string::npos) {
            username = line.substr(18,line.size()-1);
            username = username.erase(username.size() - 1);
            steamUserName = username;
        }
    }
    inputFile.close(); // Close the file
    return 0;
}
