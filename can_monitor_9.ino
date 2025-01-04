#include <SPI.h>
#include <mcp_can.h>
#include <SD.h>

// Pin definitions
const int CAN_CS_PIN = 10;
const int SD_CS_PIN = 9;
const int CAN_INT_PIN = 2;

// Initialize CAN Bus object
MCP_CAN CAN(CAN_CS_PIN);

// Active log filename
String activeLogFile;

void setup() {
    // Initialize pins
    pinMode(CAN_CS_PIN, OUTPUT);
    pinMode(SD_CS_PIN, OUTPUT);
    pinMode(CAN_INT_PIN, INPUT);
    
    digitalWrite(CAN_CS_PIN, HIGH);
    digitalWrite(SD_CS_PIN, HIGH);
    
    // Start serial
    Serial.begin(115200);
    delay(1000);
    Serial.println("CAN Bus Logger");
    
    // Initialize SPI
    SPI.begin();
    
    // Initialize SD Card
    Serial.print("SD init...");
    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("Failed!");
        while(1);
    }
    Serial.println("OK!");
    
    // Find next available filename
    char filename[13];
    int fileNum = 0;
    do {
        fileNum++;
        sprintf(filename, "CAN%03d.TXT", fileNum);
    } while (SD.exists(filename) && fileNum < 999);
    
    activeLogFile = String(filename);
    Serial.print("Logging to: ");
    Serial.println(activeLogFile);
    
    // Create file with header
    File logFile = SD.open(activeLogFile.c_str(), FILE_WRITE);
    if (logFile) {
        logFile.println("Time,ID,Length,Data");
        logFile.close();
    }
    
    // Initialize CAN Bus
    Serial.print("CAN init...");
    digitalWrite(CAN_CS_PIN, LOW);
    if (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_16MHZ) == CAN_OK) {
        Serial.println("OK!");
    } else {
        Serial.println("Failed!");
        while(1);
    }
}

void loop() {
    if (!digitalRead(CAN_INT_PIN)) {  // Check for CAN message
        unsigned long id;
        unsigned char len = 0;
        unsigned char buf[8];
        
        // Read the message
        if (CAN.readMsgBuf(&id, &len, buf) == CAN_OK) {
            // Print to Serial
            Serial.print("ID: 0x");
            Serial.print(id, HEX);
            Serial.print(" Len: ");
            Serial.print(len);
            Serial.print(" Data: ");
            for (int i = 0; i < len; i++) {
                if (buf[i] < 0x10) Serial.print("0");
                Serial.print(buf[i], HEX);
                Serial.print(" ");
            }
            Serial.println();
            
            // Log to SD card
            digitalWrite(CAN_CS_PIN, HIGH);  // Disable CAN
            
            File logFile = SD.open(activeLogFile.c_str(), FILE_WRITE);
            if (logFile) {
                logFile.print(millis());
                logFile.print(",0x");
                logFile.print(id, HEX);
                logFile.print(",");
                logFile.print(len);
                logFile.print(",");
                
                for (int i = 0; i < len; i++) {
                    if (buf[i] < 0x10) logFile.print("0");
                    logFile.print(buf[i], HEX);
                    if (i < len - 1) logFile.print(" ");
                }
                logFile.println();
                logFile.close();
            }
            
            digitalWrite(CAN_CS_PIN, LOW);  // Re-enable CAN
        }
    }
}
