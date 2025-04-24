/*
 * Automatic Pill Dispenser
 * 
 * This program controls an automatic pill dispenser with the following features:
 * - Rotates a 14-compartment wheel using a stepper motor every 12 hours
 * - Uses a distance sensor to detect if pills are present after rotation
 * - LED behavior:
 *   - If pills present: LED turns on after 5 minutes if pills not taken, turns off when taken
 *   - If no pills: LED flashes for 2 hours, then turns off until next rotation
 * 
 * Components:
 * - 28BYJ-48 Stepper Motor with ULN2003 Driver
 * - Sparkfun VL53L4CD Distance Sensor
 * - LED indicator
 */

#include <Stepper.h>
#include <Wire.h>
#include "SparkFun_VL53L1X.h" // Using existing library for VL53L1X which is compatible

// Define pins
#define LED_PIN 13
#define SHUTDOWN_PIN 2
#define INTERRUPT_PIN 3

// Define steps per rotation (manufacturer specs for 28BYJ-48)
const int STEPS_PER_REVOLUTION = 2048; // 32 steps per revolution * 64:1 gear ratio
const int STEPS_PER_COMPARTMENT = STEPS_PER_REVOLUTION / 14; // For 14 compartments

// Define timing
const unsigned long TWELVE_HOURS_MS = 30000; // 12 hours in milliseconds: 43200000
const unsigned long PILL_REMINDER_TIMEOUT = 15000; // 5 minutes in milliseconds: 300000
const unsigned long NO_PILLS_FLASH_DURATION = 20000; // 2 hours in milliseconds: 7200000
const unsigned long PILL_CHECK_INTERVAL = 3000; // Check for pills every 5 seconds
const unsigned long LED_FLASH_INTERVAL = 500; // LED flash interval for "no pills" warning

// Thresholds for distance sensor
const int PILLS_PRESENT_THRESHOLD = 100; // Approximate distance in mm when pills are present
const int PILLS_TAKEN_THRESHOLD = 150;   // Approximate distance in mm when pills are taken

// Initialize the stepper motor
Stepper pillStepper(STEPS_PER_REVOLUTION, 9, 10, 11, 12);

// Initialize the distance sensor
SFEVL53L1X distanceSensor;

// Enum for system state
enum SystemState {
  IDLE,               // Waiting for next rotation
  PILLS_PRESENT,      // Pills present in compartment
  PILLS_REMINDER_ON,  // Pills present, reminder LED on
  NO_PILLS_WARNING,   // No pills detected, LED flashing
  NO_PILLS_IDLE       // No pills, after flash duration
};

// System variables
SystemState currentState = IDLE;
bool ledState = false;
bool ledFlashing = false;
unsigned long lastLedToggle = 0;
unsigned long lastRotationTime = 0;
unsigned long lastPillCheckTime = 0;
unsigned long stateEntryTime = 0;   // Time when current state was entered

void setup() {
  // Initialize serial communication
  Serial.begin(9600);
  Serial.println("Automatic Pill Dispenser Initializing...");
  
  // Initialize stepper motor speed
  pillStepper.setSpeed(8); // Set lower speed for stability

  // Initialize LED pin
  pinMode(LED_PIN, OUTPUT);

  // Initialize I2C communication
  Wire.begin();
  
  // Initialize distance sensor
  if (distanceSensor.begin() != 0) {
    Serial.println("Distance sensor failed to initialize. Check wiring.");
  } else {
    Serial.println("Distance sensor initialized.");
    // Configure the sensor
    distanceSensor.setDistanceModeLong();
    distanceSensor.setTimingBudgetInMs(50);
    distanceSensor.setIntermeasurementPeriod(100);
  }

  // Start with LED off
  digitalWrite(LED_PIN, LOW);
  
  // Record current time for rotation timing
  lastRotationTime = millis();
  lastPillCheckTime = millis();
  stateEntryTime = millis();
  
  Serial.println("Initialization complete. Pill dispenser ready.");
}

void loop() {
  unsigned long currentTime = millis();
  
  // Handle LED flashing if active
  if (ledFlashing && (currentTime - lastLedToggle >= LED_FLASH_INTERVAL)) {
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    lastLedToggle = currentTime;
  }
  
  // Check if it's time to rotate the dispenser (every 12 hours)
  if (currentTime - lastRotationTime >= TWELVE_HOURS_MS) {
    Serial.println("12 hours elapsed. Rotating pill dispenser...");
    
    // Rotate the wheel by one compartment
    rotateOneCompartment();
    
    // Update the rotation time
    lastRotationTime = currentTime;
    
    // Check if pills are present after rotation
    delay(5000); // Wait for things to settle
    checkPillsAfterRotation();
  }
  
  // State machine for different scenarios
  switch (currentState) {
    case IDLE:
      // Just waiting for next rotation
      break;
      
    case PILLS_PRESENT:
      // Pills are present, check if they've been taken
      if (currentTime - lastPillCheckTime >= PILL_CHECK_INTERVAL) {
        lastPillCheckTime = currentTime;
        
        // Check if pills are still present
        if (!arePillsPresent()) {
          // Pills have been taken
          Serial.println("Pills have been taken!");
          transitionToState(IDLE);
          break;
        }
        
        // Check if reminder timeout has elapsed
        if (currentTime - stateEntryTime >= PILL_REMINDER_TIMEOUT) {
          Serial.println("Pills not taken within 5 minutes! Turning on reminder LED.");
          transitionToState(PILLS_REMINDER_ON);
        }
      }
      break;
      
    case PILLS_REMINDER_ON:
      // Reminder is on, check if pills have been taken
      if (currentTime - lastPillCheckTime >= PILL_CHECK_INTERVAL) {
        lastPillCheckTime = currentTime;
        
        // Check if pills are still present
        if (!arePillsPresent()) {
          // Pills have been taken, turn off reminder
          Serial.println("Pills have been taken! Turning off reminder.");
          transitionToState(IDLE);
        }
      }
      break;
      
    case NO_PILLS_WARNING:
      // No pills, LED flashing
      if (currentTime - lastPillCheckTime >= PILL_CHECK_INTERVAL) {
        lastPillCheckTime = currentTime;
        
        // Check if pills have appeared
        if (arePillsPresent()) {
          Serial.println("Pills now detected in the compartment!");
          transitionToState(PILLS_PRESENT);
          break;
        }
        
        // Check if we've been flashing for 2 hours
        if (currentTime - stateEntryTime >= NO_PILLS_FLASH_DURATION) {
          Serial.println("No-pills warning timeout reached. Stopping flash until next rotation.");
          transitionToState(NO_PILLS_IDLE);
        }
      }
      break;
      
    case NO_PILLS_IDLE:
      // After flashing period, check if pills have appeared
      if (currentTime - lastPillCheckTime >= PILL_CHECK_INTERVAL) {
        lastPillCheckTime = currentTime;
        
        // Check if pills have appeared
        if (arePillsPresent()) {
          Serial.println("Pills now detected in the compartment!");
          transitionToState(PILLS_PRESENT);
        }
      }
      break;
  }
  
  // Small delay to prevent excessive CPU usage
  delay(100);
}

// Function to rotate the wheel by one compartment
void rotateOneCompartment() {
  Serial.println("Rotating to next compartment...");
  pillStepper.step(STEPS_PER_COMPARTMENT);
  delay(500); // Give motor time to settle
}

// Function to check pill status after rotation
void checkPillsAfterRotation() {
  if (arePillsPresent()) {
    Serial.println("Pills detected in compartment after rotation.");
    transitionToState(PILLS_PRESENT);
  } else {
    Serial.println("No pills detected in compartment after rotation!");
    transitionToState(NO_PILLS_WARNING);
  }
}

// Function to check if pills are present
bool arePillsPresent() {
  int distance = getDistance();
  
  Serial.print("Current distance: ");
  Serial.print(distance);
  Serial.println(" mm");
  
  // Pills are present if distance is small
  return (distance < PILLS_PRESENT_THRESHOLD);
}

// Function to check if pills have been taken
bool havePillsBeenTaken() {
  int distance = getDistance();
  
  // Pills have been taken if distance is medium (greater than threshold)
  return (distance > PILLS_TAKEN_THRESHOLD);
}

// Function to transition to a new state
void transitionToState(SystemState newState) {
  // Exit actions for current state
  switch (currentState) {
    case PILLS_REMINDER_ON:
      // Turn off steady LED
      digitalWrite(LED_PIN, LOW);
      break;
      
    case NO_PILLS_WARNING:
      // Stop flashing
      ledFlashing = false;
      digitalWrite(LED_PIN, LOW);
      break;
  }
  
  // Enter actions for new state
  switch (newState) {
    case IDLE:
      Serial.println("Entering IDLE state");
      // Make sure LED is off
      digitalWrite(LED_PIN, LOW);
      ledFlashing = false;
      break;
      
    case PILLS_PRESENT:
      Serial.println("Entering PILLS_PRESENT state");
      // Make sure LED is off
      digitalWrite(LED_PIN, LOW);
      ledFlashing = false;
      break;
      
    case PILLS_REMINDER_ON:
      Serial.println("Entering PILLS_REMINDER_ON state");
      // Turn on steady LED
      digitalWrite(LED_PIN, HIGH);
      ledFlashing = false;
      break;
      
    case NO_PILLS_WARNING:
      Serial.println("Entering NO_PILLS_WARNING state");
      // Start flashing LED
      ledFlashing = true;
      lastLedToggle = millis();
      break;
      
    case NO_PILLS_IDLE:
      Serial.println("Entering NO_PILLS_IDLE state");
      // Make sure LED is off
      digitalWrite(LED_PIN, LOW);
      ledFlashing = false;
      break;
  }
  
  // Update state and record entry time
  currentState = newState;
  stateEntryTime = millis();
}

// Function to get distance reading from sensor
int getDistance() {
  distanceSensor.startRanging();
  
  int timeout = 0;
  while (!distanceSensor.checkForDataReady() && timeout < 100) {
    delay(1);
    timeout++;
  }
  
  if (timeout >= 100) {
    Serial.println("Sensor timeout!");
    return -1; // Error value
  }
  
  int distance = distanceSensor.getDistance(); // Get distance in mm
  
  distanceSensor.clearInterrupt();
  distanceSensor.stopRanging();
  
  return distance;
}