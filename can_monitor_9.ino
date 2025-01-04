#include <SPI.h>
#include <mcp_can.h>
#include <SD.h>

// Pin definitions
const int CAN0_CS_PIN = 10;    // Set CS to pin 10
const int SD_CS_PIN = 9;       // SD card CS pin
const int CAN0_INT = 2;        // Set INT to pin 2

// Global variables
long unsigned int rxId;
unsigned char len = 0;
unsigned char rxBuf[8];
char msgString[128];           // Array to store serial string

// Initialize CAN Bus object
MCP_CAN CAN0(CAN0_CS_PIN);

// Active log filename and SD card status
String activeLogFile;
bool sdCardPresent = false;
File logFile;  // Keep file object global

void initializeSPI() {
    digitalWrite(CAN0_CS_PIN, HIGH);  // Deselect CAN
    digitalWrite(SD_CS_PIN, HIGH);    // Deselect SD
    SPI.begin();
    delay(100);
}

void initializeSD() {
    digitalWrite(CAN0_CS_PIN, HIGH);  // Ensure CAN is deselected
    if (SD.begin(SD_CS_PIN)) {
        sdCardPresent = true;
        Serial.println("SD card initialized successfully!");
        
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
        
        // Create and leave file open for writing
        logFile = SD.open(activeLogFile.c_str(), FILE_WRITE);
        if (logFile) {
            logFile.println("Time,ID,Length,Data,Type");
            logFile.flush();  // Ensure header is written
        } else {
            Serial.println("Failed to open log file!");
            sdCardPresent = false;
        }
    } else {
        Serial.println("SD card initialization failed!");
        sdCardPresent = false;
    }
}

void setup() {
    // Initialize pins
    pinMode(CAN0_CS_PIN, OUTPUT);
    pinMode(SD_CS_PIN, OUTPUT);
    pinMode(CAN0_INT, INPUT);
    
    Serial.begin(115200);
    delay(1000);
    Serial.println("CAN Bus Logger with SD Card Storage");
    
    // Initialize SPI bus
    initializeSPI();
    
    // Initialize CAN
    Serial.print("Initializing CAN controller...");
    if(CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_16MHZ) == CAN_OK) {
        Serial.println("OK!");
    } else {
        Serial.println("Failed!");
        while(1);
    }
    CAN0.setMode(MCP_NORMAL);
    
    // Initialize SD Card
    Serial.print("Initializing SD card...");
    initializeSD();
    
    if (!sdCardPresent) {
        Serial.println("WARNING: Operating without SD card logging");
    }
}

void logCANMessage(unsigned long timestamp, String msgType) {
    if (!sdCardPresent || !logFile) return;
    
    // Format the log entry first
    String logEntry = String(timestamp) + ",0x" +
                     String(rxId & 0x1FFFFFFF, HEX) + "," +
                     String(len) + ",";
    
    if((rxId & 0x40000000) == 0x40000000) {
        logEntry += "REMOTE";
    } else {
        for(byte i = 0; i < len; i++) {
            if(rxBuf[i] < 0x10) logEntry += "0";
            logEntry += String(rxBuf[i], HEX);
            if(i < len-1) logEntry += " ";
        }
    }
    logEntry += "," + msgType + "\n";
    
    // Write to SD
    logFile.print(logEntry);
    logFile.flush();  // Ensure data is written
}

void loop() {
    if(!digitalRead(CAN0_INT)) {  // If CAN0_INT pin is low, read receive buffer
        CAN0.readMsgBuf(&rxId, &len, rxBuf);  // Read data
        unsigned long timestamp = millis();
        
        // Process and display the message
        String msgType;
        if((rxId & 0x80000000) == 0x80000000) {
            sprintf(msgString, "Extended ID: 0x%.8lX  DLC: %1d  Data:", (rxId & 0x1FFFFFFF), len);
            msgType = "Extended";
        } else {
            sprintf(msgString, "Standard ID: 0x%.3lX       DLC: %1d  Data:", rxId, len);
            msgType = "Standard";
        }
        
        Serial.print(msgString);
        
        if((rxId & 0x40000000) == 0x40000000) {
            sprintf(msgString, " REMOTE REQUEST FRAME");
            Serial.print(msgString);
            msgType = "Remote";
        } else {
            for(byte i = 0; i<len; i++){
                sprintf(msgString, " 0x%.2X", rxBuf[i]);
                Serial.print(msgString);
            }
        }
        Serial.println();
        
        // Log to SD if available
        if (sdCardPresent) {
            logCANMessage(timestamp, msgType);
        }
    }
    
    // Check for SD card much less frequently (every 30 seconds)
    static unsigned long lastSDCheck = 0;
    if (!sdCardPresent && (millis() - lastSDCheck > 30000)) {
        lastSDCheck = millis();
        initializeSD();
    }
}
