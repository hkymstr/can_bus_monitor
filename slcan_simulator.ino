/*
 * Arduino SLCAN Random CAN Simulator
 * 
 * This sketch simulates a CAN bus by randomly generating CAN frames with 
 * predefined IDs and transmitting them using the SLCAN protocol over serial.
 * 
 * Hardware:
 * - Arduino Uno R3
 * - CAN Shield (e.g., MCP2515)
 * 
 * Protocol:
 * - SLCAN (Serial Line CAN) protocol at 115200 baud
 * - Waits for config commands before starting transmission: C, S6, O
 */

#include <SPI.h>
#include <mcp_can.h>

// CAN Shield SPI CS Pin
const int SPI_CS_PIN = 10;

// Create CAN interface instance
MCP_CAN CAN(SPI_CS_PIN);

// Predefined CAN IDs
const uint32_t CAN_IDS[] = {0x100, 0x200, 0x230, 0x260, 0x500};
const int NUM_IDS = 5;

// State variables
bool transmissionEnabled = false;
unsigned long lastTransmitTime = 0;
const unsigned long TRANSMIT_INTERVAL = 100; // ms between frames

// Buffer for SLCAN messages
char slcanBuffer[32];

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  
  // Initialize CAN bus
  while (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) != CAN_OK) {
    delay(100);
  }
  
  // Set to normal mode
  CAN.setMode(MCP_NORMAL);
  
  // Do not print debug messages as they interfere with SLCAN protocol
}

void loop() {
  // Process incoming serial commands
  if (Serial.available() > 0) {
    processCommand();
  }
  
  // Transmit random CAN frames if enabled
  if (transmissionEnabled && (millis() - lastTransmitTime > TRANSMIT_INTERVAL)) {
    sendRandomCanFrame();
    lastTransmitTime = millis();
  }
}

void processCommand() {
  char cmd = Serial.read();
  
  switch (cmd) {
    case 'C':
      // Close command
      transmissionEnabled = false;
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
      transmissionEnabled = true;
      Serial.write('\r'); // Acknowledge with CR only
      break;
      
    default:
      // Unknown command - no response
      break;
  }
}

void sendRandomCanFrame() {
  // Select a random CAN ID from the predefined list
  uint32_t canId = CAN_IDS[random(NUM_IDS)];
  
  // Generate random data length (1-8 bytes)
  uint8_t dataLength = 8;
  
  // Generate random data
  uint8_t data[8];
  for (int i = 0; i < dataLength; i++) {
    data[i] = random(0, 256);
  }
  
  // Format as SLCAN message
  formatSlcanMessage(canId, data, dataLength);
  
  // Send the formatted message over serial (without println)
  Serial.print(slcanBuffer);
  Serial.write('\r'); // Standard SLCAN termination
  
  // Also send to actual CAN bus if needed
  CAN.sendMsgBuf(canId, 0, dataLength, data);
}

void formatSlcanMessage(uint32_t id, uint8_t* data, uint8_t length) {
  // Standard format: tiiildd..
  // t: type (t for 11-bit, T for 29-bit)
  // iii: hex ID (3 bytes for 11-bit, 8 bytes for 29-bit)
  // l: data length (1-8)
  // dd: data bytes in hex
  
  int index = 0;
  
  // Frame type - using standard 11-bit IDs
  slcanBuffer[index++] = 't';
  
  // ID in hex (3 bytes for 11-bit ID)
  char idStr[4];
  sprintf(idStr, "%03X", id & 0x7FF); // Limit to 11 bits
  slcanBuffer[index++] = idStr[0];
  slcanBuffer[index++] = idStr[1];
  slcanBuffer[index++] = idStr[2];
  
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
