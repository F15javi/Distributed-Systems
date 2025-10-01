#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <iostream>
#include <mosquitto.h>
#include <cstring>

#include "../src/xplaneConnect.h"
#ifdef WIN32
#include <Windows.h>
#define sleep(n) Sleep(n * 1000)
#endif

#define ROWS 3
#define COLUMNS 9
int publisher();


const char* client_id = "1234";
const char* host = "test.mosquitto.org";
unsigned int port_mqtt = 1883;
const char* username = "Javier";
const char* password = "xX1234Xx";
const char* cafile = "C:\\Users\\Lenovo\\Documents\\cert\\DigiCertGlobalRootCA.pem";


const char* xpIP = "127.0.0.1"; //IP Address of computer running X-Plane
unsigned short xpPort = 49007;  //default port number XPC listens on
unsigned short port = 49003;    //port number to which X-Plane is set to send UDP packets

int main()
{
    printf("XPlaneConnect Example: readDATA()\n\n");

    printf("Start mosquitto MQTT\n\n");
    mosquitto_lib_init();
   

    while (1) {

        // Open Socket
        XPCSocket sock = aopenUDP(xpIP, xpPort, port); //docs seem to be outdated on these...

        // Read 2 rows of data
        
        float data[ROWS][COLUMNS]; //data[0] is dataset index number, data[1] to data[9] are the contents of the dataset
        readDATA(sock, data, ROWS);

        printf("\nVtas: %f,", data[0][2]);
        printf("\n pitch = %f, roll = %f, heading = %f, alt = %f", data[1][1], data[1][2], data[1][3], data[2][3]);

        closeUDP(sock);
        
        int speed = (int)data[0][2];
        int altitude = (int)data[2][3];
        float lat = data[2][0];
        float lon = data[2][1];
        float hdg = data[0][2];

        Sleep(10);        
        publisher();
    }
    return 0;
}



int publisher() {

    

    // Init
    mosquitto_lib_init();
    mosquitto* mosq = mosquitto_new("simpleClient", false, nullptr);
    if (!mosq) {
        std::cerr << "Could not create client\n";
        return 1;
    }

    // Authentication
    //mosquitto_username_pw_set(mosq, username, password);

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
    std::cout << "Connected\n\n";

    // Publish one message (QoS 1, wait for delivery)
    const char* topic = "prueba";
    const char* payload = "Hola Hector!";
    rc = mosquitto_publish(mosq, nullptr, topic, std::strlen(payload), payload, 1, false);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "Publish failed: " << mosquitto_strerror(rc) << "\n";
    }
    else {
        std::cout << "Message published\n";
    }

    mosquitto_log_callback_set(mosq, [](mosquitto*, void*, int level, const char* str) {
        std::cout << "[mosq] " << str << std::endl;
        });

    // Process network events briefly (needed for QoS>0 to complete handshake)
    for (int i = 0; i < 10; i++) {
        mosquitto_loop(mosq, 1000, 1);  // run network loop for 100ms
    }

    // Disconnect
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    std::cout << "Done\n";
    
    return 0;

}