/*
 * Arduino SLCAN CAN Bus Listener
 * 
 * This sketch listens on a 500kbps CAN bus and forwards all received messages
 * to the serial port using the SLCAN protocol.
 * 
 * Hardware:
 * - Arduino Uno R3
 * - CAN Shield (e.g., MCP2515)
 * 
 * Protocol:
 * - SLCAN (Serial Line CAN) protocol at 115200 baud
 * - Waits for config commands before starting reception: C, S6, O
 */

#include <SPI.h>
#include <mcp_can.h>

// CAN Shield SPI CS Pin
const int SPI_CS_PIN = 10;

// Extended frame flag - if your library doesn't define this
#define EXTENDED_FRAME_FLAG 0x80000000

// Create CAN interface instance
MCP_CAN CAN(SPI_CS_PIN);

// State variables
bool receptionEnabled = false;

// Buffer for SLCAN messages
char slcanBuffer[32];

// Buffer for receiving CAN frames
unsigned long rxId;
byte rxLen = 0;
byte rxBuf[8];

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  
  // Initialize CAN bus at 500kbps
  while (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) != CAN_OK) {
    delay(100);
  }
  
  // Set to normal mode
  CAN.setMode(MCP_NORMAL);
}

void loop() {
  // Process incoming serial commands
  if (Serial.available() > 0) {
    processCommand();
  }
  
  // Check for received CAN messages and forward them if reception is enabled
  if (receptionEnabled && CAN_MSGAVAIL == CAN.checkReceive()) {
    receiveAndForwardCanMessage();
  }
}

void processCommand() {
  char cmd = Serial.read();
  
  switch (cmd) {
    case 'C':
      // Close command
      receptionEnabled = false;
      Serial.write('\r'); // Acknowledge with CR only
      break;
      
    case 'S':
      // Speed command (only S6 is implemented - 500kbps)
      if (Serial.available() > 0) {
        char speedByte = Serial.read();
        if (speedByte == '6') {
          // Set to 500kbps (already initialized in setup)
          Serial.write('\r'); // Acknowledge with CR only
        }
      }
      break;
      
    case 'O':
      // Open command
      receptionEnabled = true;
      Serial.write('\r'); // Acknowledge with CR only
      break;
      
    default:
      // Unknown command - no response
      break;
  }
}

void receiveAndForwardCanMessage() {
  // Read the message from CAN bus
  byte ext = 0;
  
  // Using the readMsgBuf method with the correct parameters
  // based on your library's API
  CAN.readMsgBuf(&rxId, &rxLen, rxBuf);  // This version takes 3 parameters
  
  // Format as SLCAN message and send
  formatSlcanMessage(rxId, rxBuf, rxLen);
  
  // Send the formatted message over serial
  Serial.print(slcanBuffer);
  Serial.write('\r'); // Standard SLCAN termination
}

void formatSlcanMessage(unsigned long id, byte* data, byte length) {
  // Standard format: tiiildd..
  // t: type (t for 11-bit, T for 29-bit)
  // iii: hex ID (3 bytes for 11-bit, 8 bytes for 29-bit)
  // l: data length (1-8)
  // dd: data bytes in hex
  
  int index = 0;
  
  // Check if this is an extended frame
  // Most implementations use bit 31 as flag for extended frames
  if (id & EXTENDED_FRAME_FLAG) {
    // Extended frame (29-bit ID)
    slcanBuffer[index++] = 'T';
    
    // ID in hex (8 bytes for 29-bit ID)
    char idStr[9];
    sprintf(idStr, "%08lX", id & 0x1FFFFFFF); // Mask off the flag bit, keep only the 29-bit ID
    for (int i = 0; i < 8; i++) {
      slcanBuffer[index++] = idStr[i];
    }
  } else {
    // Standard frame (11-bit ID)
    slcanBuffer[index++] = 't';
    
    // ID in hex (3 bytes for 11-bit ID)
    char idStr[4];
    sprintf(idStr, "%03lX", id & 0x7FF); // Limit to 11 bits
    slcanBuffer[index++] = idStr[0];
    slcanBuffer[index++] = idStr[1];
    slcanBuffer[index++] = idStr[2];
  }
  
  // Data length
  slcanBuffer[index++] = '0' + length;
  
  // Data bytes
  for (int i = 0; i < length; i++) {
    char byteStr[3];
    sprintf(byteStr, "%02X", data[i]);
    slcanBuffer[index++] = byteStr[0];
    slcanBuffer[index++] = byteStr[1];
  }
  
  // Null terminator
  slcanBuffer[index] = '\0';
}
