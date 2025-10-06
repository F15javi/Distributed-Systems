#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <iostream>
#include <mosquitto.h>
#include <cstring>
#include <fstream>
#include <string>
#include "../ClienteSimulador/header/json.hpp"
using json = nlohmann::json;
#include "../src/xplaneConnect.h"

#include "../ClienteSimulador/header/wintoastlib.h"
using namespace WinToastLib;


#ifdef WIN32
#include <Windows.h>
#define sleep(n) Sleep(n * 1000)
#endif

#define ROWS 4
#define COLUMNS 9

int publisher(std::string payload, mosquitto* mosq);
int readSteamUserName();
int subscriber(mosquitto* mosq);
int push_warning();
void on_message(struct mosquitto* mosq, void* obj, const struct mosquitto_message* msg);

const char* host = "test.mosquitto.org";
unsigned int port_mqtt = 1883;



const char* xpIP = "127.0.0.1"; //IP Address of computer running X-Plane
unsigned short xpPort = 49009;  //default port number XPC listens on
unsigned short port = 49003;    //port number to which X-Plane is set to send UDP packets

std::string steamUserName;
int speed;
float vvi;
int altitude;
float altitudeGND;
float lat;
float lon;
float hdg;

class CustomHandler : public IWinToastHandler {
public:
    void toastActivated() const {
        std::wcout << L"The user clicked in this toast" << std::endl;
    }

    void toastActivated(int actionIndex) const {
        std::wcout << L"The user clicked on action #" << actionIndex << std::endl;
    }

    void toastActivated(std::wstring response) const {
        std::wcout << L"The user replied with: " << response << std::endl;
    }

    void toastDismissed(WinToastDismissalReason state) const {
        switch (state) {
        case UserCanceled:
            std::wcout << L"The user dismissed this toast" << std::endl;
            break;
        case TimedOut:
            std::wcout << L"The toast has timed out" << std::endl;
            break;
        case ApplicationHidden:
            std::wcout << L"The application hid the toast using ToastNotifier.hide()" << std::endl;
            break;
        default:
            std::wcout << L"Toast not activated" << std::endl;
            break;
        }
    }

    void toastFailed() const {
        std::wcout << L"Error showing current toast" << std::endl;
    }
};

enum Results {
    ToastClicked,             // user clicked on the toast
    ToastDismissed,           // user dismissed the toast
    ToastTimeOut,             // toast timed out
    ToastHided,               // application hid the toast
    ToastNotActivated,        // toast was not activated
    ToastFailed,              // toast failed
    SystemNotSupported,       // system does not support toasts
    UnhandledOption,          // unhandled option
    MultipleTextNotSupported, // multiple texts were provided
    InitializationFailure,    // toast notification manager initialization failure
    ToastNotLaunched          // toast could not be launched
};

#define COMMAND_ACTION     L"--action"
#define COMMAND_AUMI       L"--aumi"
#define COMMAND_APPNAME    L"--appname"
#define COMMAND_APPID      L"--appid"
#define COMMAND_EXPIREMS   L"--expirems"
#define COMMAND_TEXT       L"--text"
#define COMMAND_HELP       L"--help"
#define COMMAND_IMAGE      L"--image"
#define COMMAND_SHORTCUT   L"--only-create-shortcut"
#define COMMAND_AUDIOSTATE L"--audio-state"
#define COMMAND_ATTRIBUTE  L"--attribute"
#define COMMAND_INPUT      L"--input"

void print_help() {
    std::wcout << "WinToast Console Example [OPTIONS]" << std::endl;
    std::wcout << "\t" << COMMAND_ACTION << L" : Set the actions in buttons" << std::endl;
    std::wcout << "\t" << COMMAND_AUMI << L" : Set the App User Model Id" << std::endl;
    std::wcout << "\t" << COMMAND_APPNAME << L" : Set the default appname" << std::endl;
    std::wcout << "\t" << COMMAND_APPID << L" : Set the App Id" << std::endl;
    std::wcout << "\t" << COMMAND_EXPIREMS << L" : Set the default expiration time" << std::endl;
    std::wcout << "\t" << COMMAND_TEXT << L" : Set the text for the notifications" << std::endl;
    std::wcout << "\t" << COMMAND_IMAGE << L" : set the image path" << std::endl;
    std::wcout << "\t" << COMMAND_ATTRIBUTE << L" : set the attribute for the notification" << std::endl;
    std::wcout << "\t" << COMMAND_SHORTCUT << L" : create the shortcut for the app" << std::endl;
    std::wcout << "\t" << COMMAND_AUDIOSTATE << L" : set the audio state: Default = 0, Silent = 1, Loop = 2" << std::endl;
    std::wcout << "\t" << COMMAND_INPUT << L" : add an input to the toast" << std::endl;
    std::wcout << "\t" << COMMAND_HELP << L" : Print the help description" << std::endl;
}
void on_message(struct mosquitto* mosq, void* obj, const struct mosquitto_message* msg) {
    std::cout << "\nReceived message on topic: " << msg->topic << std::endl;
    if (msg->payloadlen > 0)
        std::cout << "Payload: " << (char*)msg->payload << std::endl;
    else
        std::cout << "(empty message)" << std::endl;
}
int main() {
    

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
            vvi = data[1][3];
            altitude = (int)data[3][3];
            altitudeGND = (int)data[3][4];
            lat = data[3][1];
            lon = data[3][2];
            hdg = data[2][3];
        }
        else {
            printf("\nSimulator not running");
        }
        

        closeUDP(sock);
       


        
            
        json jsonPayload;

        jsonPayload["aircraft"] = steamUserName;
        jsonPayload["altitude"] = altitude;
        jsonPayload["altitudeGND"] = altitudeGND;
        jsonPayload["heading"] = hdg;
        jsonPayload["latitude"] = lat;
        jsonPayload["longitude"] = lon;
        jsonPayload["speed"] = speed;
        jsonPayload["vvi"] = vvi;

        std::string mqttPayload = jsonPayload.dump();

        // Init

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
        //std::cout << "Connected\n";
        mosquitto_message_callback_set(mosq, on_message);

        publisher(mqttPayload, mosq);
        subscriber(mosq);

        // Process network events briefly (needed for QoS>0 to complete handshake)
        for (int i = 0; i < 10; i++) {
            mosquitto_loop(mosq, 100, 1);  // run network loop for 100ms
        }
        // Disconnect
        mosquitto_disconnect(mosq);
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        std::cout << "\nDone\n";
        Sleep(10); 

    }
    return 0;
}



int publisher(std::string payload, mosquitto* mosq) {

    // Publish one message (QoS 1, wait for delivery)
    std::string topicStr = "flights/"+steamUserName;

    const char* topic = topicStr.c_str();
    const char* msg = payload.c_str();
    printf(msg);

    int rc = mosquitto_publish(mosq, nullptr, topic, std::strlen(msg), msg, 1, false);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "Publish failed: " << mosquitto_strerror(rc) << "\n";
    }
    else {
        std::cout << "\nMessage published\n\n";
    }

    mosquitto_log_callback_set(mosq, [](mosquitto*, void*, int level, const char* str) {
        std::cout << "\n\n[mosq out] " << str << std::endl;
        });    

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
int subscriber(mosquitto* mosq, const struct mosquitto_message* msg) {

    // Subscribe one message (QoS 1, wait for delivery)
    std::string topicStr = "flights/" + steamUserName + "/warnings";

    const char* topic = topicStr.c_str();


    int rc = mosquitto_subscribe(mosq, nullptr, topic, 1);

    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "Failed to subscribe: " << mosquitto_strerror(rc) << "\n";
    }
    else {
        std::cout << "\n[mosq in] Message receive\n\n";
    }

    mosquitto_log_callback_set(mosq, [](mosquitto*, void*, int level, const char* str) {
        std::cout << "[mosq in] " << str << std::endl;
        });
    
    //push_warning();
    return 1;


}
int push_warning() {

    std::wstring appName = L"ATC Alert";
    std::wstring appUserModelID = L"ATC Alert";
    std::wstring text = L"";
    std::wstring imagePath = L"";
    std::wstring attribute = L"";
    std::vector<std::wstring> actions;
    INT64 expiration = 0;
    bool input = false;

    bool onlyCreateShortcut = false;
    WinToastTemplate::AudioOption audioOption = WinToastTemplate::AudioOption::Default;



    WinToast::instance()->setAppName(appName);
    WinToast::instance()->setAppUserModelId(appUserModelID);


    if (!WinToast::instance()->initialize()) {
        std::wcerr << L"Error, your system in not compatible!" << std::endl;
        return Results::InitializationFailure;
    }

    WinToastTemplate templ(!imagePath.empty() ? WinToastTemplate::ImageAndText02 : WinToastTemplate::Text02);
    templ.setTextField(text, WinToastTemplate::FirstLine);
    templ.setAudioOption(audioOption);
    templ.setAttributionText(attribute);
    templ.setImagePath(imagePath);
    if (input) {
        templ.addInput();
    }

    for (auto const& action : actions) {
        templ.addAction(action);
    }

    if (expiration) {
        templ.setExpiration(expiration);
    }

    if (WinToast::instance()->showToast(templ, new CustomHandler()) < 0) {
        std::wcerr << L"Could not launch your toast notification!";
        return Results::ToastFailed;
    }
}
