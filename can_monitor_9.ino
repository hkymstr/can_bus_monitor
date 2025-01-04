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

void setup()
{
  // Initialize pins
  pinMode(CAN0_CS_PIN, OUTPUT);
  pinMode(SD_CS_PIN, OUTPUT);
  pinMode(CAN0_INT, INPUT);
  
  // Set both CS pins high initially
  digitalWrite(CAN0_CS_PIN, HIGH);
  digitalWrite(SD_CS_PIN, HIGH);
  
  Serial.begin(115200);
  delay(1000);
  Serial.println("CAN Bus Logger with SD Card Storage");
  
  // Initialize SPI
  SPI.begin();
  
  // Initialize CAN first
  Serial.print("Initializing CAN controller...");
  digitalWrite(CAN0_CS_PIN, LOW);  // Select CAN
  digitalWrite(SD_CS_PIN, HIGH);   // Deselect SD
  
  if(CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_16MHZ) == CAN_OK) {
    Serial.println("OK!");
  } else {
    Serial.println("Failed!");
    while(1); // CAN failure is critical
  }
  
  CAN0.setMode(MCP_NORMAL);   // Set operation mode to normal
  digitalWrite(CAN0_CS_PIN, HIGH);  // Deselect CAN
  
  // Try to initialize SD Card
  Serial.print("Initializing SD card...");
  digitalWrite(SD_CS_PIN, LOW);   // Select SD
  if (SD.begin(SD_CS_PIN)) {
    Serial.println("OK!");
    sdCardPresent = true;
    
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
      logFile.println("Time,ID,Length,Data,Type");
      logFile.close();
    }
  } else {
    Serial.println("Failed! Operating in Serial-only mode");
    sdCardPresent = false;
  }
  digitalWrite(SD_CS_PIN, HIGH);  // Deselect SD
  
  // Re-select CAN for normal operation
  digitalWrite(CAN0_CS_PIN, LOW);
  
  // Final status message
  if (!sdCardPresent) {
    Serial.println("WARNING: Operating without SD card logging");
    Serial.println("Data will only be output to Serial port");
  }
}

void logToSD(unsigned long timestamp, String msgType) {
  if (!sdCardPresent) return;  // Skip if SD card is not available
  
  // Switch from CAN to SD card
  digitalWrite(CAN0_CS_PIN, HIGH);  // Deselect CAN
  digitalWrite(SD_CS_PIN, LOW);     // Select SD
  
  File logFile = SD.open(activeLogFile.c_str(), FILE_WRITE);
  if (logFile) {
    logFile.print(timestamp);
    logFile.print(",0x");
    logFile.print(rxId & 0x1FFFFFFF, HEX); // Remove frame type bits
    logFile.print(",");
    logFile.print(len);
    logFile.print(",");
    
    // Write data bytes
    if((rxId & 0x40000000) == 0x40000000) {
      logFile.print("REMOTE");
    } else {
      for(byte i = 0; i<len; i++){
        if(rxBuf[i] < 0x10) logFile.print("0");
        logFile.print(rxBuf[i], HEX);
        if(i < len-1) logFile.print(" ");
      }
    }
    logFile.print(",");
    logFile.println(msgType);
    logFile.close();
  } else if (sdCardPresent) {
    // If we suddenly can't write to the SD card, mark it as unavailable
    Serial.println("WARNING: SD card write failed! Switching to Serial-only mode");
    sdCardPresent = false;
  }
  
  // Switch back to CAN
  digitalWrite(SD_CS_PIN, HIGH);    // Deselect SD
  digitalWrite(CAN0_CS_PIN, LOW);   // Select CAN
}

void loop()
{
  if(!digitalRead(CAN0_INT))  // If CAN0_INT pin is low, read receive buffer
  {
    CAN0.readMsgBuf(&rxId, &len, rxBuf);      // Read data: len = data length, buf = data byte(s)
    unsigned long timestamp = millis();
    
    // Determine message type and format message string
    String msgType;
    if((rxId & 0x80000000) == 0x80000000) {
      sprintf(msgString, "Extended ID: 0x%.8lX  DLC: %1d  Data:", (rxId & 0x1FFFFFFF), len);
      msgType = "Extended";
    } else {
      sprintf(msgString, "Standard ID: 0x%.3lX       DLC: %1d  Data:", rxId, len);
      msgType = "Standard";
    }
    
    Serial.print(msgString);
    
    // Handle remote request frames and regular frames
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
    
    // Try to log to SD card if available
    logToSD(timestamp, msgType);
  }
  
  // Optional: Check if SD card was reinserted
  static unsigned long lastSDCheck = 0;
  if (!sdCardPresent && (millis() - lastSDCheck > 5000)) {  // Check every 5 seconds
    lastSDCheck = millis();
    
    // Switch to SD card
    digitalWrite(CAN0_CS_PIN, HIGH);  // Deselect CAN
    digitalWrite(SD_CS_PIN, LOW);     // Select SD
    
    if (SD.begin(SD_CS_PIN)) {
      Serial.println("SD card detected! Resuming logging...");
      sdCardPresent = true;
      
      // Create new log file
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
        logFile.println("Time,ID,Length,Data,Type");
        logFile.close();
      }
    }
    
    // Switch back to CAN
    digitalWrite(SD_CS_PIN, HIGH);    // Deselect SD
    digitalWrite(CAN0_CS_PIN, LOW);   // Select CAN
  }
}
