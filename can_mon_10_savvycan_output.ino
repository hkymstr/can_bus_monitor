#include <SPI.h>
#include <mcp_can.h>
#include <SD.h>

// Pin definitions
const int CAN0_CS_PIN = 10;
const int SD_CS_PIN = 9;
const int CAN0_INT = 2;
const int button_center = A4;  // Action button

// Global variables
long unsigned int rxId;
unsigned char len = 0;
unsigned char rxBuf[8];
char msgString[128];
char filename[13];

// Button and action tracking
bool lastButtonState = HIGH;    
bool buttonPressed = false;     
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;
int actionCounter = 0;         

// Bus identifier (can be changed if multiple CAN buses)
const int BUS_ID = 1;

MCP_CAN CAN0(CAN0_CS_PIN);
bool sdCardPresent = false;

void getNextFilename() {
    int fileNum = 0;
    do {
        fileNum++;
        sprintf(filename, "CAN%03d.txt", fileNum);
    } while (SD.exists(filename) && fileNum < 999);
    Serial.print("Using filename: ");
    Serial.println(filename);
}

bool initSD() {
    // Disable CAN CS
    digitalWrite(CAN0_CS_PIN, HIGH);
    delay(10);
    
    // Initialize SD card
    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, LOW);
    
    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("SD Init Failed! Check card and connections");
        sdCardPresent = false;
        digitalWrite(SD_CS_PIN, HIGH);
        digitalWrite(CAN0_CS_PIN, LOW);
        return false;
    }
    
    sdCardPresent = true;
    Serial.println("SD Init Success!");
    
    // Get next available filename
    getNextFilename();
    
    // Create new log file with header
    File logFile = SD.open(filename, FILE_WRITE);
    if (logFile) {
        // New header format
        logFile.println("Time Stamp,ID,Extended,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8");
        logFile.close();
        Serial.println("Log file created");
    } else {
        Serial.println("Error creating log file!");
        sdCardPresent = false;
        return false;
    }
    
    // Cleanup
    digitalWrite(SD_CS_PIN, HIGH);
    SD.end();
    
    // Re-enable CAN CS
    digitalWrite(CAN0_CS_PIN, LOW);
    delay(10);
    
    return true;
}

void writeToSD(String dataString) {
    if (!sdCardPresent) return;
    
    // Switch from CAN to SD
    digitalWrite(CAN0_CS_PIN, HIGH);
    delay(10);
    
    if (SD.begin(SD_CS_PIN)) {
        File logFile = SD.open(filename, FILE_WRITE);
        if (logFile) {
            logFile.println(dataString);
            logFile.close();
        } else {
            Serial.println("Failed to open log file!");
        }
        
        digitalWrite(SD_CS_PIN, HIGH);
        SD.end();
    } else {
        Serial.println("SD Write Failed - Card Init Error!");
        sdCardPresent = false;
    }
    
    // Switch back to CAN
    digitalWrite(CAN0_CS_PIN, LOW);
    delay(10);
}

void logAction() {
    actionCounter++;
    
    // Write action events in the same format as regular CAN messages
    // Fill with zeros/empty for the data fields
    String actionString = String(millis()) + 
                         ",ACTION,0," + 
                         String(BUS_ID) + 
                         ",0,0,0,0,0,0,0,0,0";
    
    Serial.println("----------------------------------------");
    Serial.println("ACTION " + String(actionCounter) + " LOGGED");
    Serial.println("----------------------------------------");
    
    // Also send to serial for plotting
    Serial.println(actionString);
    
    writeToSD(actionString);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Initialize pins
    pinMode(CAN0_CS_PIN, OUTPUT);
    pinMode(SD_CS_PIN, OUTPUT);
    pinMode(CAN0_INT, INPUT);
    pinMode(button_center, INPUT_PULLUP);
    
    // Set CS pins high initially
    digitalWrite(CAN0_CS_PIN, HIGH);
    digitalWrite(SD_CS_PIN, HIGH);
    
    // Initialize SPI
    SPI.begin();
    delay(100);
    
    // Initialize CAN
    Serial.print("Initializing CAN...");
    digitalWrite(CAN0_CS_PIN, LOW);
    if(CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_16MHZ) == CAN_OK) {
        Serial.println("Success!");
    } else {
        Serial.println("Failed!");
        while(1);
    }
    CAN0.setMode(MCP_NORMAL);
    
    // Try to initialize SD card multiple times
    bool sdInitialized = false;
    for(int i = 0; i < 3 && !sdInitialized; i++) {
        Serial.print("Initializing SD card (attempt ");
        Serial.print(i + 1);
        Serial.print(")...");
        sdInitialized = initSD();
        if(!sdInitialized) {
            delay(1000);  // Wait before retry
        }
    }
    
    // Print column headers to serial for plotting applications
    Serial.println("Time Stamp,ID,Extended,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8");
}

void loop() {
    // Check for button press with debouncing
    int reading = digitalRead(button_center);
    
    if (reading != lastButtonState) {
        lastDebounceTime = millis();
    }
    
    if ((millis() - lastDebounceTime) > debounceDelay) {
        if (reading == LOW && !buttonPressed) {
            logAction();
            buttonPressed = true;
        }
        else if (reading == HIGH) {
            buttonPressed = false;
        }
    }
    
    lastButtonState = reading;
    
    // Check for CAN messages
    if(!digitalRead(CAN0_INT)) {
        CAN0.readMsgBuf(&rxId, &len, rxBuf);
        
        // Determine if extended or standard ID
        bool isExtended = (rxId & 0x80000000) == 0x80000000;
        String idString;
        
        if(isExtended) {
            idString = String((rxId & 0x1FFFFFFF), HEX);
        } else {
            idString = String(rxId, HEX);
        }
        
        // Format data bytes individually
        String d1 = len >= 1 ? String(rxBuf[0], HEX) : "0";
        String d2 = len >= 2 ? String(rxBuf[1], HEX) : "0";
        String d3 = len >= 3 ? String(rxBuf[2], HEX) : "0";
        String d4 = len >= 4 ? String(rxBuf[3], HEX) : "0";
        String d5 = len >= 5 ? String(rxBuf[4], HEX) : "0";
        String d6 = len >= 6 ? String(rxBuf[5], HEX) : "0";
        String d7 = len >= 7 ? String(rxBuf[6], HEX) : "0";
        String d8 = len >= 8 ? String(rxBuf[7], HEX) : "0";
        
        // Pad each byte with leading zero if needed
        if(d1.length() == 1) d1 = "0" + d1;
        if(d2.length() == 1) d2 = "0" + d2;
        if(d3.length() == 1) d3 = "0" + d3;
        if(d4.length() == 1) d4 = "0" + d4;
        if(d5.length() == 1) d5 = "0" + d5;
        if(d6.length() == 1) d6 = "0" + d6;
        if(d7.length() == 1) d7 = "0" + d7;
        if(d8.length() == 1) d8 = "0" + d8;
        
        // Create log string in new format: Time Stamp,ID,Extended,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8
        String logString = String(millis()) + "," +
                          "0x" + idString + "," +
                          (isExtended ? "1" : "0") + "," +
                          String(BUS_ID) + "," +
                          String(len) + "," +
                          d1 + "," + d2 + "," + d3 + "," + d4 + "," +
                          d5 + "," + d6 + "," + d7 + "," + d8;
        
        // For console output (formatted for readability)
        String displayString = "ID: 0x" + idString + 
                             " Ext: " + (isExtended ? "1" : "0") +
                             " Bus: " + String(BUS_ID) +
                             " Len: " + String(len) +
                             " Data: " + d1 + " " + d2 + " " + d3 + " " + d4 + 
                             " " + d5 + " " + d6 + " " + d7 + " " + d8;
        
        Serial.println("----------------------------------------");
        Serial.println(displayString);
        Serial.println("----------------------------------------");
        
        // Also output in CSV format for serial plotting
        Serial.println(logString);
        
        // Write to SD card
        writeToSD(logString);
    }
}
