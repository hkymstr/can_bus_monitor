#include <SPI.h>
#include <mcp_can.h>
#include <SD.h>

// CAN Bus Shield pins
const int SPI_CS_PIN = 10;
const int CAN_INT_PIN = 2;

// Joystick buttons
const int button_center = 27;
const int button_up = 24;
const int button_down = 26;
const int button_left = 25;
const int button_right = 28;

//joy stick variable
int joystick_center = 1;
int joystick_up = 1;
int joystick_down = 1;
int joystick_left = 1;
int joystick_right = 1;


// SD card pin
const int SD_CS_PIN = 9;

// Filename for logging
const char* LOG_FILENAME = "canlog.txt";

// Initialize CAN Bus object
MCP_CAN CAN(SPI_CS_PIN);

// Buffer for serial input
String inputString = "";
bool stringComplete = false;
bool sdCardPresent = false;

// Function prototypes
void printMenu();
void processSerialInput();
void receiveCANMessage();
String formatCANMessage(unsigned long id, unsigned char len, unsigned char *buf, bool isSent);
void logToSD(String message);

void setup() {
     //Initialize pins as necessary
      pinMode(SD_CS_PIN, OUTPUT);
    
    
    // Initialize serial communication and wait for port to open
    Serial.begin(115200);
    delay(2000);  // Give serial port time to connect
    
    Serial.println("-------------------");
    Serial.println("CAN Bus Logger Test");
    Serial.println("-------------------");
    
    // Initialize SPI first
    SPI.begin();
    Serial.println("SPI initialized");
    
    // Initialize SD card
    Serial.print("Initializing SD card...");
    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("Failed!");
        Serial.println("Program will continue without SD card logging.");
        sdCardPresent = false;
    } else {
        Serial.println("Success!");
        sdCardPresent = true;
        delay(1500);
                
        // Create header in log file if it's new
        if (!SD.exists(LOG_FILENAME)) {
            File logFile = SD.open(LOG_FILENAME, FILE_WRITE);
            if (logFile) {
                Serial.prinln("SD file opened!");
                logFile.println("Timestamp,Direction,ID,Length,Data");
                logFile.close();
            }
        }
        else {
          Serial.println("SD file did not open");
          }
    }
    
    // Initialize CAN Bus with proper parameters
    Serial.print("Initializing CAN Bus...");
    // Initialize MCP2515 running at 16MHz with a baudrate of 500kb/s and the masks and filters disabled.
    byte result = CAN.begin(MCP_ANY, CAN_500KBPS, MCP_16MHZ);
    if (result == 0) {
        Serial.println("Success!");
    } else {
        Serial.print("Failed! Error code: ");
        Serial.println(result);
        Serial.println("Check your connections and verify the following:");
        Serial.println("1. Is the CAN shield properly seated?");
        Serial.println("2. Are the CAN_L and CAN_H lines connected?");
        Serial.println("3. Is there proper power to the shield?");
        while (1); // Stop here
    }
    
    // Configure interrupt pin
    pinMode(CAN_INT_PIN, INPUT);
    
    Serial.println("\nSystem Ready!");
    printMenu();
}

void loop() {
    
     //read the state of the joystick
     
     int joystick_center = digitalRead(button_center);
     int joystick_up = digitalRead(button_up);
     int joystick_down = digitalRead(button_down);
     int joystick_left = digitalRead(button_left);
     int joystick_right = digitalRead(button_right);
     
     // Check for incoming CAN messages
    if (!digitalRead(CAN_INT_PIN)) {
        receiveCANMessage();
    }
    
    // Check for serial input
    while (Serial.available()) {
        char inChar = (char)Serial.read();
        if (inChar == '\n' || inChar == '\r') {
            if (inputString.length() > 0) {
                processSerialInput();
                inputString = "";
            }
        } else {
            inputString += inChar;
        }
    }

     if (!button_center) {
          Serial.println("\nCenter");
          logFile.println("Center");
     }

     if (!button_left) {
          Serial.println("\nLeft");
          logFile.println("Left");
     }

      if (!button_right) {
          Serial.println("\nRight");
          logFile.println("Right");
     }

     if (!button_down) {
          Serial.println("\nDown");
          logFile.println("Down");
     }
          
}

void logToSD(String message) {
    if (sdCardPresent) {
        File logFile = SD.open(LOG_FILENAME, FILE_WRITE);
        if (logFile) {
            logFile.println(message);
            logFile.close();
        } else {
            Serial.println("Error opening log file!");
        }
    }
}

void receiveCANMessage() {
    unsigned long id;
    unsigned char len = 0;
    unsigned char buf[8];
    
    // Read CAN message
    byte status = CAN.readMsgBuf(&id, &len, buf);
    
    if (status == CAN_OK) {
        // Format message for logging
        String logMessage = formatCANMessage(id, len, buf, false);
        
        // Log to SD card
        logToSD(logMessage);
        
        // Print to Serial Monitor
        Serial.print("\nReceived: ID: 0x");
        Serial.print(id, HEX);
        Serial.print(" Len: ");
        Serial.print(len);
        Serial.print(" Data: ");
        for (int i = 0; i < len; i++) {
            if (buf[i] < 0x10) {
                Serial.print("0");
            }
            Serial.print(buf[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
    } else {
        Serial.println("Error reading message");
    }
}

String formatCANMessage(unsigned long id, unsigned char len, unsigned char *buf, bool isSent) {
    String message = String(millis()) + ",";
    message += isSent ? "TX," : "RX,";
    message += "0x" + String(id, HEX) + ",";
    message += String(len) + ",";
    
    for (int i = 0; i < len; i++) {
        if (buf[i] < 0x10) {
            message += "0";
        }
        message += String(buf[i], HEX);
        if (i < len - 1) {
            message += " ";
        }
    }
    
    return message;
}

void processSerialInput() {
    Serial.print("\nProcessing command: ");
    Serial.println(inputString);
    
    if (inputString.startsWith("send")) {
        // Format: send,ID,length,byte1 byte2 byte3...
        // Example: send,7DF,8,02 01 0C 00 00 00 00 00
        
        String parts[10];
        int partCount = 0;
        int startIndex = 5; // Skip "send,"
        
        // Parse the command
        while (startIndex < inputString.length() && partCount < 10) {
            int endIndex = inputString.indexOf(',', startIndex);
            if (endIndex == -1) {
                endIndex = inputString.indexOf(' ', startIndex);
            }
            if (endIndex == -1) {
                endIndex = inputString.length();
            }
            
            parts[partCount] = inputString.substring(startIndex, endIndex);
            partCount++;
            startIndex = endIndex + 1;
        }
        
        if (partCount >= 3) {
            // Parse ID
            unsigned long id = strtoul(parts[0].c_str(), NULL, 16);
            // Parse length
            unsigned char len = parts[1].toInt();
            // Parse data bytes
            unsigned char data[8] = {0};
            char *ptr;
            for (int i = 0; i < len && i < 8; i++) {
                data[i] = strtol(parts[2 + i].c_str(), &ptr, 16);
            }
            
            // Send CAN message
            byte sendStatus = CAN.sendMsgBuf(id, 0, len, data);
            if (sendStatus == CAN_OK) {
                // Format message for logging
                String logMessage = formatCANMessage(id, len, data, true);
                
                // Log to SD card
                logToSD(logMessage);
                
                Serial.println("Message sent successfully!");
                
                // Print sent message details
                Serial.print("Sent: ID: 0x");
                Serial.print(id, HEX);
                Serial.print(" Data: ");
                for (int i = 0; i < len; i++) {
                    if (data[i] < 0x10) {
                        Serial.print("0");
                    }
                    Serial.print(data[i], HEX);
                    Serial.print(" ");
                }
                Serial.println();
            } else {
                Serial.println("Error sending message");
            }
        } else {
            Serial.println("Invalid command format!");
            printMenu();
        }
    } else if (inputString.startsWith("help")) {
        printMenu();
    } else {
        Serial.println("Unknown command!");
        printMenu();
    }
}

void printMenu() {
    Serial.println("\n=== CAN Bus Logger Commands ===");
    Serial.println("1. send,ID,length,byte1 byte2 byte3...");
    Serial.println("   Example: send,7DF,8,02 01 0C 00 00 00 00 00");
    Serial.println("2. help - Show this menu");
    Serial.println("=============================");
}
