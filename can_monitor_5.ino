#include <SPI.h>
#include <mcp_can.h>
#include <SD.h>

// CAN Bus Shield pins
const int SPI_CS_PIN = 10;
const int CAN_INT_PIN = 2;

// SD card pin
const int SD_CS_PIN = 4;

// CAN Bus configuration
#define CAN_ID_MODE     0x00          // Standard CAN frame
#define CAN_SPEED      0x00          // 500kbps
#define CAN_CLOCK      0x00          // 16MHz oscillator

// Initialize CAN Bus object
MCP_CAN CAN(SPI_CS_PIN);

// Buffer for serial input
String inputString = "";
bool stringComplete = false;

// Function prototypes
void printMenu();
void processSerialInput();
void receiveCANMessage();
String formatCANMessage(unsigned long id, unsigned char len, unsigned char *buf);

void setup() {
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
        Serial.println("Program will continue without SD card.");
    } else {
        Serial.println("Success!");
    }
    
    // Initialize CAN Bus with proper parameters
    Serial.print("Initializing CAN Bus...");
    byte result = CAN.begin(CAN_ID_MODE, CAN_SPEED, CAN_CLOCK);
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
}

void receiveCANMessage() {
    unsigned long id;
    unsigned char len = 0;
    unsigned char buf[8];
    
    // Read CAN message
    byte status = CAN.readMsgBuf(&id, &len, buf);
    
    if (status == CAN_OK) {
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
