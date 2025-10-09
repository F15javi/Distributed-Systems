#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <iostream>
#include <mosquitto.h>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>

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

int readSteamUserName();
int push_warning(std::wstring text);
void event_manager(std::string payload);

const char* host = "test.mosquitto.org";
const char* topicPub;
const char* topicSub;
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



void on_connect(struct mosquitto* mosq, void* obj, int rc) {
    if (rc == 0) {
        std::cout << "[MQTT IN] Connected to broker.\n";
        mosquitto_subscribe(mosq, NULL, topicSub, 1);
    }
    else {
        std::cerr << "[MQTT IN] Connection failed: " << rc << "\n";
    }
}

void on_message(struct mosquitto* mosq, void* obj, const struct mosquitto_message* msg) {
    
std::string payload((char*)msg->payload, msg->payloadlen);

event_manager(payload);

    
}

void subscriber_thread() {
    mosquitto_lib_init();

    // Ensure steamUserName is set
    std::string topicSubStr = "flights/" + steamUserName + "/warnings";
    topicSub = topicSubStr.c_str();

    mosquitto* mosq = mosquitto_new("subscriberClient", true, nullptr);
    if (!mosq) {
        std::cerr << "[MQTT IN] Failed to create client\n";
        return;
    }

    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_message_callback_set(mosq, on_message);



    int rc = mosquitto_connect(mosq, host, port_mqtt, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "[MQTT IN] Connect failed: " << mosquitto_strerror(rc) << "\n";
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        return;
    }
    // Start asynchronous network loop
    mosquitto_loop_start(mosq);

    // Keep thread alive
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    mosquitto_loop_stop(mosq, true);
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
}

void publisher_thread() {

    mosquitto_lib_init();

    std::string topicPubStr = "flights/" + steamUserName;
    const char* topicPub = topicPubStr.c_str();

    mosquitto* mosqPub = mosquitto_new("simpleClient", false, nullptr);


    while (true) {

        if (!mosqPub) {
            std::cerr << "Could not create client\n";
            return;
        }

        // TLS
        if (mosquitto_tls_insecure_set(mosqPub, true) != MOSQ_ERR_SUCCESS) {
            std::cerr << "Failed to set TLS\n\n";
            return;
        }

        // Connect
        int rc = mosquitto_connect(mosqPub, host, port_mqtt, 60);
        if (rc != MOSQ_ERR_SUCCESS) {
            std::cerr << "Connect failed: " << mosquitto_strerror(rc) << "\n";
            return;
        }

        // Open Socket
        XPCSocket sock = aopenUDP(xpIP, xpPort, port); //docs seem to be outdated on these...

        // Read 4 rows of data
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
            printf("\nSimulator not running\n");
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
        const char* msg = mqttPayload.c_str();

        rc = mosquitto_publish(mosqPub, nullptr, topicPub, std::strlen(msg), msg,0, false);

        if (rc == MOSQ_ERR_SUCCESS){
            std::cout << "[MQTT OUT] Sent: " << msg << "\n";
        }else {
            std::cerr << "[MQTT OUT] Publish failed: " << mosquitto_strerror(rc) << "\n";
        }
        

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    }

    mosquitto_disconnect(mosqPub);
    mosquitto_destroy(mosqPub);
    mosquitto_lib_cleanup();
}

int main() {
    

    printf("Start clientXplane\n\n");

    printf("Start mosquitto MQTT\n\n");
    mosquitto_lib_init();

    printf("Read Steam User\n\n");
    readSteamUserName();

    std::thread subThread(subscriber_thread);
    // Give subscriber time to connect before publisher starts
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::thread pubThread(publisher_thread);

    pubThread.join();
    subThread.join();
    
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

void event_manager(std::string payload) {

    json data = json::parse(payload);
    int code = data["code"];

  
    if (code == 0) {

        std::string username = data["username"];
        std::string countryname = data["countryname"];

        std::wstring wUsername(username.begin(), username.end());
        std::wstring wCountryname(countryname.begin(), countryname.end());

        push_warning(wUsername+L" is in "+ wCountryname);
    }
    else if (code == 1) {
        std::string username = data["username"];
        std::string countryname = data["countryname"];

        std::wstring wUsername(username.begin(), username.end());
        std::wstring wCountryname(countryname.begin(), countryname.end());

        push_warning(wUsername + L" has enter " + wCountryname);
    }
    else if (code == 2) {
        std::string username = data["username"];
        std::string countryname = data["countryname"];

        std::wstring wUsername(username.begin(), username.end());
        std::wstring wCountryname(countryname.begin(), countryname.end());

        push_warning(wUsername + L" has entered in a conflict zone on " + wCountryname);
    }
    else if (code == 3) {
        std::string username = data["username"];
        std::string distance = data["distance_km"];
        std::string username2 = data["other_aircraft"];

        std::wstring wUsername(username.begin(), username.end());
        std::wstring wUsername2(username2.begin(), username2.end());
        std::wstring wistance(distance.begin(), distance.end());
        std::wstring wDistance(distance.begin(), distance.end());

        push_warning(wUsername + L" and " + wUsername2 + L" are " + wDistance +L" km away");


    }
    else if (code == 4) {
        std::string username = data["username"];
        std::string countryname = data["countryname"];

        std::wstring wUsername(username.begin(), username.end());
        std::wstring wCountryname(countryname.begin(), countryname.end());

        push_warning(wUsername + L" has landed on " + wCountryname);
    }
    else if (code == 5) {
        std::string username = data["username"];
        std::string countryname = data["countryname"];

        std::wstring wUsername(username.begin(), username.end());
        std::wstring wCountryname(countryname.begin(), countryname.end());

        push_warning(wUsername + L" has taken off from " + wCountryname);
    }
}
int push_warning(std::wstring text) {

    std::wstring appName = L"ATC ALERT";
    std::wstring appUserModelID = L"ATC ALERT";
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
