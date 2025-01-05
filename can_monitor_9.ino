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
        logFile.println("Timestamp,ID Type,ID,DLC,Data,Event");
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
    String actionString = String(millis()) + 
                         ",ACTION,,,," + 
                         "Action " + String(actionCounter);
    
    Serial.println("----------------------------------------");
    Serial.println("ACTION " + String(actionCounter) + " LOGGED");
    Serial.println("----------------------------------------");
    
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
        
        String idType;
        String idString;
        
        if((rxId & 0x80000000) == 0x80000000) {
            idType = "Extended";
            idString = String((rxId & 0x1FFFFFFF), HEX);
        } else {
            idType = "Standard";
            idString = String(rxId, HEX);
        }
        
        String dataBytes = "";
        for(byte i = 0; i < len; i++) {
            if(rxBuf[i] < 0x10) dataBytes += "0";
            dataBytes += String(rxBuf[i], HEX);
            if(i < (len - 1)) dataBytes += " ";
        }
        
        String displayString = idType + " ID: 0x" + idString + 
                             "  DLC: " + String(len) + 
                             "  Data: " + dataBytes;
                             
        String logString = String(millis()) + "," +
                          idType + "," +
                          "0x" + idString + "," +
                          String(len) + "," +
                          dataBytes + ",";
        
        Serial.println("----------------------------------------");
        Serial.println(displayString);
        Serial.println("----------------------------------------");
        
        writeToSD(logString);
    }
}
