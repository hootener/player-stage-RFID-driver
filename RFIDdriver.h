/*
* RFID Driver for the Player robot control software. This particular driver was designed to 
* work with the GAO RFID 433 MHz Active RFID reader with RS232 connection. This reader can be
* found at (http://www.gaorfid.com/index.php?main_page=product_info&cPath=133&products_id=669).
* However, with modification, this driver can probably work with any RFID reader.
* 
* Compatible tags can also be purchased from GAO RFID at 
* (http://www.gaorfid.com/index.php?main_page=product_info&cPath=135&products_id=678). Tags
* are 433 MHz active cards. This code also contains the ability to send messages to readers,
* but was not needed for my project, so the code is untested.
*
* Code is adapted from work done by Chris Tralie. His personal website can be found at: http://www.ctralie.com/
* His writeup concerning this code can be found at: http://www.ctralie.com/PrincetonUGRAD/Projects/dukereu/report.pdf
*/

#ifndef RFIDDRIVER_H 
#define RFIDDRIVER_H

#include <unistd.h>
#include <string.h>

#include <libplayercore/playercore.h>
#include <libplayerinterface/playerxdr.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctime>

#define DEFAULT_BAUD 9600

#define MSG_CRC_INIT 0xFFFF
#define MSG_CCITT_CRC_POLY 0x1021

typedef unsigned char u8;
typedef unsigned short u16;

//Make a class "MsgObj" that is able to split the streams of bytes
//into more sensible components
////[Header (1 byte)] [Data length (1 byte)] [Command (1 byte)] [Data* ] [CRC HI] [CRC LO]
class MsgObj {
public:
        MsgObj(u8 dL, u8 oC, u8* d);//Used for constructing a command to send to the reader
        MsgObj(u8* bytes, int len); //Used for constructing a command to parse from received bytes from the reader
        ~MsgObj();
        u8 dataLen;
        u8 opCode;
        u8 data[256];
        u8 crc_high;
        u8 crc_low;
        u16 status;
        int length;

	//new methods
	unsigned long int tagID;
	unsigned int RSSI;
        
        int getLength();
        void getBytesToSend(u8* bytes);
	player_rfid_data_t GetTagAsPlayerRFID();
};

class RFIDdriver : public ThreadedDriver {
public:
        // Constructor; need that
        RFIDdriver(ConfigFile* cf, int section);

        // Must implement the following methods.
        virtual int Setup();
        virtual int Shutdown();

        // This method will be invoked on each incoming Message
        virtual int ProcessMessage(QueuePointer &resp_queue, 
                               player_msghdr * hdr,
                               void * data);

private:
        // Main function for device thread.
        virtual void Main();
	
        int Connect(int connect_speed);
        void Disconnect();
        void sendMessage(u8 command, u8* data, int length);
        int readMessage(u8* data, int length, int timeout);	
        void QueryEnvironment(u16 timeout);

        struct termios initial_options;
        char* port;//Port to connect with RFID reader
        player_devaddr_t rfid_id;
        int fd;//File descriptor for serial connection
        u16 readPwr;//set to 3000 in the main program
        FILE* logfile;//Logfile to log RFID readings along with
        //system time
        int baudrate;
        u16 querytimeout;//Timeout for reading tags
};

#endif
