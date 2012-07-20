
#if HAVE_CONFIG_H
  #include <config.h>
#endif

//#include <unistd.h>
//#include <string.h>

//#include <libplayercore/playercore.h>

#include <sstream>
#include <iostream>
#include "RFIDdriver.h"

static int TIMEOUT = 2;//Timeout in seconds
static bool debug = false;//Use for printing out debug statements during execution
static bool printlive = false;//Controls whether all of the tag information is printed to stdout 
//in addition to the logfile

static int eps = 20;//Extra time to wait during timeout

//This function updates a running CRC value
void CRC_calcCrc8(u16* crc_calc, u8 ch) {
        u8 i, v, xor_flag;
        
        //Align test bit with leftmost bit of the MsgObj byte
        v = 0x80;
        for (int i = 0; i < 8; i++) {
                if (*crc_calc & 0x8000)
                        xor_flag = 1;
                else
                        xor_flag = 0;
                *crc_calc = *crc_calc << 1;
                if (ch & v)
                        *crc_calc = *crc_calc + 1;
                if (xor_flag)
                        *crc_calc = *crc_calc ^ MSG_CCITT_CRC_POLY;
                //Align test bit with next bit of the MsgObj byte.  
                v = v >> 1;
        }
}

u16 CRC_calcCrcMsgObj(MsgObj* MsgObj) {
        u16 crcReg, i;
        crcReg = MSG_CRC_INIT;
        
        CRC_calcCrc8(&crcReg, MsgObj->dataLen);
        CRC_calcCrc8(&crcReg, MsgObj->opCode);
        for (int i = 0; i < MsgObj->dataLen; i++) {
                CRC_calcCrc8(&crcReg, MsgObj->data[i]);
        }
        return crcReg;
}

//Used to validate the CRC for the received bytes
bool validateCRC(u8* buf, int n) {
        //TODO: Fix this (it doesn't work right now)
        if (n < 7)
                return false;
        u16 crcReg = MSG_CRC_INIT;
        for (int i = 0; i < n; i++) {
                CRC_calcCrc8(&crcReg, buf[i]);
        }
        //This is the CRC returned from the reader
        u16 thiscrc = ((buf[n - 1] & 0x00FF) << 8) | (buf[n - 2] & 0x00FF);
        printf("%x %x", buf[n -2], buf[n - 1]);
        bool equal = (thiscrc == crcReg);
        if (!equal) {
                fprintf(stderr, "Warning: CRC check failed: Got %x, expecting %x\n", thiscrc, crcReg);
        }
        return equal;
}

//Construct a message object from some data that will be sent out
MsgObj::MsgObj(u8 dL, u8 oC, u8* d) {
        dataLen = dL;
        opCode = oC;
        for (int i = 0; i < dL; i++)
                data[i] = d[i];
        length = getLength();
}

//Construct a message object from a receive set of bytes
//[header 1 byte] [data length 1 byte] [command 1 byte] [status word 2 bytes] [data M bytes] [crc 2 bytes]
//[header 3 bytes][13 bytes of junk][4 bytes for tag ID][3 bytes for junk][1 byte CS][2 byte post amble]
MsgObj::MsgObj(u8* bytes, int len) {
	//new implementation
	
	//parse bytes to check for correctness
	//pull out and convert tag, store to member variable
	//pull out and store RSSIT, store to member variable

	bool makeBadMsjObj = false;

	if(len != 38){ //data passed in bytes is not correct size. Ignore this packet.
		fprintf(stderr, "ERROR: packet size (%d) is not 38. ",len);
		makeBadMsjObj = true;
	}
	else if(bytes[0] != 0x55) {
		fprintf(stderr, "ERROR: first header byte (%x) is not '0x55'. ",bytes[0]);
		makeBadMsjObj = true;
	}
	else if(bytes[1] != 0x20){
		fprintf(stderr, "ERROR: second header byte (%x) is not '0x20'. ",bytes[1]);
		makeBadMsjObj = true;
	}		
	else if(bytes[2] != 0x50){
		fprintf(stderr, "ERROR: third header byte (%x) is not '0x50' .", bytes[2]);
		makeBadMsjObj = true;
	}
	
	if(makeBadMsjObj){ //bytes is bad, so generate a garbage msjObj that can be skipped or ignored later.
		fprintf(stderr, "Bad packet created. \n");
		tagID = 0;
		RSSI = -1;
	}
	else{ //bytes is okay, so get the correct tagID and RSSI
		tagID = (((unsigned long int)bytes[21]) << 24) + (((unsigned long int)bytes[22]) << 16) + 
			(((unsigned long int)bytes[23]) << 8) + (((unsigned long int)bytes[24]));
		
		RSSI = bytes[27];

		if(debug){
			printf("\nCreated MsjObj with Tag Id(hex): %x %x %x %x and RSSI(hex): %x", bytes[21], bytes[22], 
				bytes[23], bytes[24], bytes[27]); 
			printf("\nCreated MsjObj with Tag Id(ulongint): %lu and RSSI(uint): %u \n", tagID, RSSI); 
		}
	}
}

MsgObj::~MsgObj() {

}

int MsgObj::getLength() {
        return (int)dataLen + 5;
}

//returns data in a format applicable for publishing to other Player drivers
player_rfid_data_t MsgObj::GetTagAsPlayerRFID(){
	
	//fill in and return player structure.
	player_rfid_data_t player_data;
	player_data.tags_count = 1;
	player_data.tags = (player_rfid_tag_t*)calloc(player_data.tags_count, sizeof(player_data.tags[0]));
	
	player_rfid_tag_t tag;
	
	tag.type = 1;

	//it doesn't look like player's rfid struct has a dedicated
	//member for RSSI. Therefore it's appended to to the 6 digit guid as
	//a 3 digit number. Therefore, guid_count will always = 9.

	tag.guid_count = 9; 
	
	std::ostringstream IDandRSSI;
	IDandRSSI << tagID;

	if(RSSI < 10) IDandRSSI<<0<<0<<RSSI;
	else if(RSSI < 100) IDandRSSI<<0<<RSSI;
	else IDandRSSI << RSSI;

	if(debug){
		std::cout<<"\nAppended tag and RSSI: "<<IDandRSSI.str()<<"\n";
	}

	const char *tagRSSI = IDandRSSI.str().c_str();

	tag.guid = (char*)calloc(tag.guid_count, sizeof(tag.guid[0]));
	for(int i = 0; i < 9; i++){
		tag.guid[i] = tagRSSI[i];
	}

	player_data.tags[0] = tag;
	if(debug){
		std::cout<<"Contents of RFID tag data published to Player: \n";
		std::cout<<"GUID and RSSI: ";
		for(int i = 0; i < 9; i++){
			std::cout<<player_data.tags[0].guid[i];
		}
		std::cout<<"\n";
	}

	return player_data;	
}

//for communicating with the reader. I don't think this is necessary in this application.
void MsgObj::getBytesToSend(u8* bytes) {
        bytes[0] = 0xFF;
        bytes[1] = dataLen;
        bytes[2] = opCode;
        for (int i = 3; i < length && (i - 3) < dataLen; i++) {
                bytes[i] = data[i - 3];
        }
        u16 crc = CRC_calcCrcMsgObj(this);
        u8 lo = (u8)(crc & 0x00FF);
        u8 hi = (u8)((crc & 0xFF00) >> 8);
        bytes[length - 2] = hi;
        bytes[length - 1] = lo;
}



/////////////////////////////////////////////////////////////////////

Driver* 
RFIDdriver_Init(ConfigFile* cf, int section) {
        // Create and return a new instance of this driver
        return((Driver*)(new RFIDdriver(cf, section)));
}

void RFIDdriver_Register(DriverTable* table) {
        table->AddDriver("rfiddriver", RFIDdriver_Init);
}

RFIDdriver::RFIDdriver(ConfigFile* cf, int section)
    :ThreadedDriver(cf, section, false, PLAYER_MSGQUEUE_DEFAULT_MAXLEN, 
             PLAYER_RFID_CODE) {
        this->port = (char*)cf->ReadString(section, "port", "/dev/ttyS3");
        debug = atoi((char*)cf->ReadString(section, "debug", "0"));
        printlive = atoi((char*)cf->ReadString(section, "printlive", "0"));
        querytimeout = atoi((char*)cf->ReadString(section, "timeout", "50"));
        
        readPwr = 3000;
        //Let the user specify the location of the logfile
        logfile = fopen((char*)cf->ReadString(section, "logfile", "rfidtags.log"), "w");
        return;
}


//I borrowed most of the code for this function (that sets up the serial connection)
//from "rfi341_protocol.cc" in the player drivers directory 
int RFIDdriver::Connect (int port_speed) {
        // Open serial port
        fd = open (port, O_RDWR);
        if (fd < 0) {
                PLAYER_ERROR2 ("> Connecting to RFID Device on [%s]; [%s]...[failed!]",
                           (char*) port, strerror (errno));
                return -1;
        }

        // Change port settings
        struct termios options;
        memset (&options, 0, sizeof (options));// clear the struct for new port settings
        // Get the current port settings
        if (tcgetattr (fd, &options) != 0) {
                PLAYER_ERROR (">> Unable to get serial port attributes !");
                return (-1);
        }
        tcgetattr (fd, &initial_options);

        // turn off break sig, cr->nl, parity off, 8 bit strip, flow control
        options.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

        // turn off echo, canonical mode, extended processing, signals
        options.c_lflag &= ~(ECHO | ECHOE | ICANON | IEXTEN | ISIG);

        options.c_cflag &= ~(CSTOPB);   // use one stop bit
        options.c_cflag &= ~(PARENB);   // no parity
        options.c_cflag &= ~(CSIZE );   // clear size
        options.c_cflag |= (CS8);     // set bit size (default is 8)
        options.c_oflag &= ~(OPOST);  // turn output processing off


        options.c_cc[VMIN] = 0;
        options.c_cc[VTIME] = 10 * TIMEOUT;//Have a timeout of 2 seconds

        // read satisfied if TIME is exceeded (t = TIME *0.1 s)
        //  options.c_cc[VTIME] = 1;
        //  options.c_cc[VMIN] = 0;

        // Change the baud rate
        switch (port_speed) {
                case 1200: baudrate = B1200; break;
                case 2400: baudrate = B2400; break;
                case 4800: baudrate = B4800; break;
                case 9600: baudrate = B9600; break;
                case 19200: baudrate = B19200; break;
                case 230400: baudrate = B230400; break;
                case 38400: baudrate = B38400; break;
                case 57600: baudrate = B57600; break;
                case 115200: baudrate = B115200; break;
                default: {
                        PLAYER_ERROR1 (">> Unsupported speed [%d] given!", port_speed);
                        return (-1);
                }
        }
        // Set the baudrate to the given port_speed
        cfsetispeed (&options, baudrate);
        cfsetospeed (&options, baudrate);

        // Activate the settings for the port
        if (tcsetattr (fd, TCSAFLUSH, &options) < 0) {
                PLAYER_ERROR (">> Unable to set serial port attributes !");
                return (-1);
        }

        //PLAYER_MSG1 (1, "> Connecting to RFID Reader at %dbps...[done]", port_speed);
        // Make sure queues are empty before we begin
        tcflush (fd, TCIOFLUSH);

        return (0);
}

void RFIDdriver::Disconnect() {
        close(fd);
}

//Pass the command and data parts of the MsgObj here
void RFIDdriver::sendMessage(u8 command, u8* data, int length) {
        MsgObj message(length, command, data);
        u8 bytes[256];
        message.getBytesToSend(bytes);
        if (debug) {
                printf("\n\nSending hex: ");
                for (int i = 0; i < message.getLength(); i++)
                        printf("%x ", bytes[i]);
                printf("\n\n");
        }
        //originally uncommented.I don't think it is necessary to send anything to the reader.
        //write(fd, bytes, message.getLength());
        tcflush(fd, TCIOFLUSH);
}

//old specification: [SOH 1 byte] [length 1 byte] [op code 1 byte] [status 2 bytes] [data n bytes] [CRC 2 bytes]
//new specification: [1 byte header 0x55][no. data 0x20][reader ID 0x50][0x01][0x02][header !** 3 bytes][0x20][1 byte counter][0x28][0x42][0x43][8 bytes junk]
//[4 byte tag id][0x33][1 byte reader address][1 byte RSSI][1 byte CS][0x20][1 byte alarm][4 bytes reserved][LF][CR][1 byte CS]
int RFIDdriver::readMessage(u8* data, int length, int timeout) {
        memset(data, 0x00, 38);//Clear the previous contents from the buffer
        int minlength = 38; //was 7
        int n = 0;
	int tempN = 0;
        time_t start = time(NULL);
	//must read data until 3 byte hex string 0x55 0x20 0x50 is encountered. Then, read the next 23 bytes.
	while(data[0] != 0x55 || data[1] != 0x20 || data[2] != 0x50){
		int k = read(fd, &data[tempN],1);
		if(data[tempN] == 0x055){
			data[0] = data[tempN];
			data[tempN] = 0x07E;
			tempN = 1;
		}
		else{
			tempN += k;
		}

		if(tempN >2) tempN = 0;
		//if(debug) printf("Read %d bytes: %x, %x, %x\n", tempN, data[0], data[1], data[2]); 
	}

	//if we make it here, we found the header and can proceed appending to data at index 3. This will give header and tag data in data[].
	n = 3;

	if(debug){
		printf("Found header bytes. Reading data stream \n");
	}

	//Old code -- read in on loop
       /* while (n < minlength - 2) {
                int elapsed = (int)(time(NULL) - start);
                int k = read(fd, &data[n], minlength - 3);
                n += k;
                if (k == 0 && elapsed >= timeout) break;//At the point when no data is left, stop reading it
                //(this seems to work, and it's faster than waiting for the timeout timer
                //which I commented out above)
               // if (n  >= 2) {
               //         minlength = 7 + data[1];//Add in the length of the data;
               // }
        }*/

	//New code - read in a fixed length of data
	
	while(n < minlength){ //read until you get a full buffer
		int elapsed = (int)(time(NULL) - start);
		int k = read(fd, &data[n], 37);
		n+=k;
	}
	
        if (n <= 3)  fprintf(stderr, "Error: Serial communication timed out\n");
        else if (debug) {
                printf("\n\nReceived hex length %i (supposed to be %i): ", n, minlength);
                for (int i = 0; i < n; i++)
                        printf("%x ", data[i]);
                printf("\n\n");
	
		//for testing, attempt to extract the tag ID and signal strength.
		//tag ID is 4 bytes and should start at index 16 of the byte array.
		//Signal strength is appended to the end of the tag information, 
		//it is one byte and always occurrs directly after a Carriage return.
		
		printf("Tag ID(hex): %x %x %x %x   RSSI(hex): %x \n", data[21], data[22], data[23], data[24], data[27]);
		
		unsigned long int tagID = (((unsigned long int)data[21])<< 24) + (((unsigned long int)data[22])<< 16) +
						(((unsigned long int) data[23])<< 8) + (((unsigned long int)data[24]));

		printf("Tag ID(int): %lu RSSI(int): %u \n", tagID, data[27]);

        }
        //validateCRC(data, n);
        return n;
}

//Get all of the tags in the environment and log them to the logfile
void RFIDdriver::QueryEnvironment(u16 timeout) {
        u8 buf[512];

        if(debug) printf("\nQuerying Environment for RFID tags.\n");

        timeval tim;
        gettimeofday(&tim, NULL);
        double readtime = tim.tv_sec + (tim.tv_usec / 1000000.0);//The system time
        
	//look for a tag.
	int n = readMessage(buf, 256, 0);
       
	//attempt to create a MsgObj from anything that is found in buf..
        MsgObj* tagread = new MsgObj(buf, n);
        
	//This section of code outputs to the logfile. Needs to be updated to work
	//with new tags and reader.
	
	//make sure MsgObj isn't junk before outputting to the logfile.
	if(tagread->tagID != 0 && tagread->RSSI != -1){
		if (printlive)  printf("%.3lf %lu %u ", readtime, tagread->tagID, tagread->RSSI);	
		fprintf(logfile, "%.3lf %lu %u ", readtime, tagread->tagID, tagread->RSSI);
	}
	else if(debug){
		printf("Skipping invalid MsgObj.\n");
	}

	//Publish the found tag.

	player_rfid_data_t data = tagread->GetTagAsPlayerRFID();

	Publish(device_addr, PLAYER_MSGTYPE_DATA, PLAYER_RFID_DATA_TAGS, &data, sizeof(data), NULL);

	//cleanup
	player_rfid_data_t_cleanup(&data);	
	delete tagread;
}

////////////////////////////////////////////////////////////////////////////////
// Set up the device.  Return 0 if things go well, and -1 otherwise.
int RFIDdriver::Setup() {   
        printf("RFID driver initialising\n\n");
        
        //Connect at 57600 baud.
        printf("\tAttempting 57600 bps...\n");
        Connect(57600);
        
	u8 buf[256];
        readMessage(buf, 256, 0);//Read out any junk that's left

        // Start the device thread; spawns a new thread and executes
        // RFIDdriver::Main(), which contains the main loop for the driver.
        StartThread();

        return 0;
}


////////////////////////////////////////////////////////////////////////////////
// Shutdown the device
int RFIDdriver::Shutdown() {
        // Stop and join the driver thread
        StopThread();
        return 0;
}

int RFIDdriver::ProcessMessage(QueuePointer & resp_queue, 
                                  player_msghdr * hdr,
                                  void * data) {
        // Process MsgObjs here.  Send a response if necessary, using Publish().
        // If you handle the MsgObj successfully, return 0.  Otherwise,
        // return -1, and a NACK will be sent for you, if a response is required.
        return(0);
}

////////////////////////////////////////////////////////////////////////////////
// Main function for device thread
void RFIDdriver::Main() {
    while (true)  {
                // Check to see if Player is trying to terminate the plugin thread
                pthread_testcancel();
        
                // Process MsgObjs
                ProcessMessages(); 
                
                //Query the tags in a loop in the main thread while player is
                //executing all of the drivers
                QueryEnvironment(querytimeout);
                
    }
}

////////////////////////////////////////////////////////////////////////////////
// Extra stuff for building a shared object.

/* need the extern to avoid C++ name-mangling  */
extern "C" {
        int player_driver_init(DriverTable* table) {
                puts("New RFID driver initializing");
                RFIDdriver_Register(table);
                puts("New RFID driver done");
                return(0);
        }
}

