#include <SPI.h>
#include <mcp_can.h>
#include <SD.h>

// Pin definitions
const int CAN0_CS_PIN = 10;
const int SD_CS_PIN = 9;
const int CAN0_INT = 2;

// Global variables
long unsigned int rxId;
unsigned char len = 0;
unsigned char rxBuf[8];
char msgString[128];
char filename[13];  // Changed to array for dynamic naming

MCP_CAN CAN0(CAN0_CS_PIN);
bool sdCardPresent = false;

// Find next available filename
void getNextFilename() {
    int fileNum = 0;
    do {
        fileNum++;
        sprintf(filename, "CAN%03d.txt", fileNum);
    } while (SD.exists(filename) && fileNum < 999);
    Serial.print("Using filename: ");
    Serial.println(filename);
}

void initSD() {
    digitalWrite(CAN0_CS_PIN, HIGH);
    delay(10);
    
    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("SD Init Failed!");
        sdCardPresent = false;
    } else {
        sdCardPresent = true;
        Serial.println("SD Init Success!");
        
        // Get next available filename
        getNextFilename();
        
        // Create new log file with header
        File logFile = SD.open(filename, FILE_WRITE);
        if (logFile) {
            logFile.println("Timestamp,ID Type,ID,DLC,Data");
            logFile.close();
            Serial.println("Log file created");
        }
    }
    
    digitalWrite(SD_CS_PIN, HIGH);
    SD.end();
    
    digitalWrite(CAN0_CS_PIN, LOW);
    delay(10);
}

void writeToSD(String dataString) {
    if (!sdCardPresent) return;
    
    digitalWrite(CAN0_CS_PIN, HIGH);
    delay(10);
    
    if (SD.begin(SD_CS_PIN)) {
        File logFile = SD.open(filename, FILE_WRITE);
        if (logFile) {
            logFile.println(dataString);
            logFile.close();
            Serial.println("Data logged");
        }
        
        digitalWrite(SD_CS_PIN, HIGH);
        SD.end();
    } else {
        Serial.println("SD Write Failed!");
        sdCardPresent = false;
    }
    
    digitalWrite(CAN0_CS_PIN, LOW);
    delay(10);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    pinMode(CAN0_CS_PIN, OUTPUT);
    pinMode(SD_CS_PIN, OUTPUT);
    pinMode(CAN0_INT, INPUT);
    
    digitalWrite(CAN0_CS_PIN, HIGH);
    digitalWrite(SD_CS_PIN, HIGH);
    
    SPI.begin();
    delay(100);
    
    Serial.print("Initializing CAN...");
    digitalWrite(CAN0_CS_PIN, LOW);
    if(CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_16MHZ) == CAN_OK) {
        Serial.println("Success!");
    } else {
        Serial.println("Failed!");
        while(1);
    }
    CAN0.setMode(MCP_NORMAL);
    
    initSD();
}

void loop() {
    if(!digitalRead(CAN0_INT)) {
        CAN0.readMsgBuf(&rxId, &len, rxBuf);
        
        // Determine message type and format
        String idType;
        String idString;
        
        if((rxId & 0x80000000) == 0x80000000) {
            idType = "Extended";
            idString = String((rxId & 0x1FFFFFFF), HEX);
        } else {
            idType = "Standard";
            idString = String(rxId, HEX);
        }
        
        // Format data bytes
        String dataBytes = "";
        for(byte i = 0; i < len; i++) {
            if(rxBuf[i] < 0x10) dataBytes += "0";
            dataBytes += String(rxBuf[i], HEX);
            if(i < (len - 1)) dataBytes += " ";
        }
        
        // Create formatted strings for display and logging
        String displayString = idType + " ID: 0x" + idString + 
                             "  DLC: " + String(len) + 
                             "  Data: " + dataBytes;
                             
        String logString = String(millis()) + "," +
                          idType + "," +
                          "0x" + idString + "," +
                          String(len) + "," +
                          dataBytes;
        
        // Print to serial with clear formatting
        Serial.println("----------------------------------------");
        Serial.println(displayString);
        Serial.println("----------------------------------------");
        
        // Write to SD
        writeToSD(logString);
    }
}
