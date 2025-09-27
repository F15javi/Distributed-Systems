#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <iostream>

#include "../src/xplaneConnect.h"
#ifdef WIN32
#include <Windows.h>
#define sleep(n) Sleep(n * 1000)
#endif

int main()
{
    printf("XPlaneConnect Example: readDATA()\n\n");

    // Open Socket
    const char* xpIP = "127.0.0.1"; //IP Address of computer running X-Plane
    unsigned short xpPort = 49007;  //default port number XPC listens on
    unsigned short port = 49003;    //port number to which X-Plane is set to send UDP packets



    while (1) {




        XPCSocket sock = aopenUDP(xpIP, xpPort, port); //docs seem to be outdated on these...

        // Read 2 rows of data
        const int rows = 9;
        float data[rows][9]; //data[0] is dataset index number, data[1] to data[9] are the contents of the dataset
        readDATA(sock, data, rows);



        printf("\nVtas: %f,", data[0][2]);

        printf("\n pitch = %f, roll = %f, heading = %f, alt = %f", data[1][1], data[1][2], data[1][3], data[2][3]);


        closeUDP(sock);

        int speed = (int)data[0][2];
        int altitude = (int)data[2][3];
        int rpm = (int)data[3][1];
        int temp = (int)data[5][1];
        int OilP = (int)data[6][1];
        int OilT = (int)data[7][1];
        char telemetry1[64];
        char telemetry2[64];

        sprintf_s(telemetry1, "1 %d %f %f %f %d %d %f",
            speed,
            data[1][1], data[1][2], data[1][3],
            altitude,
            rpm,
            data[4][1]);

        sprintf_s(telemetry2, "2 %d %d %d %f %f",
            temp,
            OilP,
            OilT,
            data[8][1], data[8][2]);

        Sleep(10);


        


    }


    return 0;
}