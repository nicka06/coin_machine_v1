/*
 * Automated Coin Machine Firmware
 * ESP32 Arduino Code
 * 
 * This firmware controls an automated coin recognition system with:
 * - Optical sensor for coin detection
 * - Servo-controlled trapdoor for rejecting multiple coins
 * - Servo-controlled flipper for coin positioning
 * - OV2640 camera for coin photography
 * - WS2812B LED lighting for photography
 * - RGB status indicator
 */

#include "config.h"
#include "hardware_functions.h"

// ==================== GLOBAL VARIABLES ====================
// Hardware objects
Servo trapdoorServo;
Servo flipperServo;
CRGB cameraLEDs[NUM_CAMERA_LEDS];

// State machine variables
CoinMachineState currentState = STATE_INIT;
CoinMachineState previousState = STATE_INIT;
unsigned long stateStartTime = 0;
unsigned long lastStateTime = 0;

// Timing variables
unsigned long trapdoorOpenTime = 0;
unsigned long flipperMoveTime = 0;
unsigned long photographSequenceTime = 0;

// Processing variables
bool processingCoin = false;
int currentPhotoStep = 0;
String currentImageFilename1 = "";
String currentImageFilename2 = "";

// Error handling
StatusCode lastError = STATUS_OK;
int errorCount = 0;

// ==================== SETUP FUNCTION ====================
void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(1000);
    
    DEBUG_PRINTLN("=== Automated Coin Machine Starting ===");
    DEBUG_PRINTLN("Initializing hardware...");
    
    // Initialize all hardware components
    bool initSuccess = true;
    
    if (!initializeStorage()) {
        DEBUG_PRINTLN("ERROR: Storage initialization failed");
        initSuccess = false;
    }
    
    if (!initializeCamera()) {
        DEBUG_PRINTLN("ERROR: Camera initialization failed");
        initSuccess = false;
    }
    
    if (!initializeServos()) {
        DEBUG_PRINTLN("ERROR: Servo initialization failed");
        initSuccess = false;
    }
    
    if (!initializeLEDs()) {
        DEBUG_PRINTLN("ERROR: LED initialization failed");
        initSuccess = false;
    }
    
    if (!initializeSensor()) {
        DEBUG_PRINTLN("ERROR: Sensor initialization failed");
        initSuccess = false;
    }
    
    // Check initialization results
    if (!initSuccess) {
        currentState = STATE_ERROR;
        setStatusLED(LED_ERROR);
        DEBUG_PRINTLN("FATAL: Hardware initialization failed");
        return;
    }
    
    // Perform system test
    DEBUG_PRINTLN("Performing system test...");
    performSystemTest();
    
    // Clean up old images
    cleanupOldImages();
    
    // Initialize state machine
    changeState(STATE_WAITING_FOR_COIN);
    
    DEBUG_PRINTLN("=== System Ready ===");
    DEBUG_PRINTLN("Waiting for coins...");
}

// ==================== MAIN LOOP ====================
void loop() {
    unsigned long currentTime = millis();
    
    // Update state timing
    unsigned long timeInState = currentTime - stateStartTime;
    
    // Handle timeout protection
    if (timeInState > RESET_TIMEOUT) {
        DEBUG_PRINTLN("WARNING: State timeout, performing reset");
        handleError(STATUS_TIMEOUT_ERROR);
        return;
    }
    
    // State machine dispatcher
    switch (currentState) {
        case STATE_INIT:
            // Should not reach here after setup
            changeState(STATE_WAITING_FOR_COIN);
            break;
            
        case STATE_WAITING_FOR_COIN:
            handleWaitingForCoin();
            break;
            
        case STATE_COIN_DETECTED:
            handleCoinDetected();
            break;
            
        case STATE_PROCESSING:
            handleProcessing();
            break;
            
        case STATE_PHOTOGRAPHING:
            handlePhotographing();
            break;
            
        case STATE_REJECTING:
            handleRejecting();
            break;
            
        case STATE_ERROR:
            handleError(lastError);
            break;
            
        default:
            DEBUG_PRINTLN("ERROR: Unknown state");
            changeState(STATE_ERROR);
            break;
    }
    
    // Process serial commands for debugging
    processSerialCommands();
    
    // Small delay to prevent excessive CPU usage
    delay(10);
}

// ==================== STATE HANDLERS ====================

void handleWaitingForCoin() {
    // Set ready status
    setStatusLED(LED_READY);
    
    // Check for sensor trigger
    if (isSensorTriggered()) {
        DEBUG_PRINTLN("Coin detected!");
        changeState(STATE_COIN_DETECTED);
        clearSensorTrigger();
    }
}

void handleCoinDetected() {
    setStatusLED(LED_PROCESSING);
    
    unsigned long timeInState = millis() - stateStartTime;
    
    // Wait for the detection window to complete
    if (timeInState >= MULTI_COIN_TIMEOUT) {
        // Check if multiple coins were detected
        if (isMultipleCoinDetected()) {
            DEBUG_PRINTLN("Multiple coins detected - rejecting");
            changeState(STATE_REJECTING);
            lastError = STATUS_MULTIPLE_COINS;
        } else {
            DEBUG_PRINTLN("Single coin detected - processing");
            changeState(STATE_PROCESSING);
        }
        resetSensorCount();
    }
}

void handleProcessing() {
    setStatusLED(LED_PROCESSING);
    
    // Check for additional coins during processing
    if (isSensorTriggered()) {
        DEBUG_PRINTLN("Additional coin detected during processing - rejecting");
        clearSensorTrigger();
        resetSensorCount();
        changeState(STATE_REJECTING);
        lastError = STATUS_COIN_DURING_PROCESSING;
        return;
    }
    
    unsigned long timeInState = millis() - stateStartTime;
    
    // Brief settling time before starting photography
    if (timeInState >= 500) {
        DEBUG_PRINTLN("Starting photography sequence");
        changeState(STATE_PHOTOGRAPHING);
        currentPhotoStep = 0;
    }
}

void handlePhotographing() {
    setStatusLED(LED_BUSY);
    
    unsigned long timeInState = millis() - stateStartTime;
    
    switch (currentPhotoStep) {
        case 0: // Move flipper to first position (90 degrees)
            DEBUG_PRINTLN("Moving flipper to first position");
            moveFlipperToSide1();
            flipperMoveTime = millis();
            currentPhotoStep = 1;
            break;
            
        case 1: // Wait for flipper to reach position, then take first photo
            if (millis() - flipperMoveTime >= SERVO_MOVE_DELAY) {
                DEBUG_PRINTLN("Taking first photo");
                currentImageFilename1 = generateImageFilename();
                
                if (captureAndSaveImage(currentImageFilename1.c_str())) {
                    DEBUG_PRINTLN("First photo captured successfully");
                    currentPhotoStep = 2;
                    flipperMoveTime = millis();
                } else {
                    DEBUG_PRINTLN("ERROR: First photo capture failed");
                    handleError(STATUS_CAMERA_ERROR);
                    return;
                }
            }
            break;
            
        case 2: // Move flipper to second position (180 degrees)
            if (millis() - flipperMoveTime >= FLIPPER_PHOTO_DELAY) {
                DEBUG_PRINTLN("Moving flipper to second position");
                moveFlipperToSide2();
                flipperMoveTime = millis();
                currentPhotoStep = 3;
            }
            break;
            
        case 3: // Wait for flipper to reach position, then take second photo
            if (millis() - flipperMoveTime >= SERVO_MOVE_DELAY) {
                DEBUG_PRINTLN("Taking second photo");
                currentImageFilename2 = generateImageFilename();
                
                if (captureAndSaveImage(currentImageFilename2.c_str())) {
                    DEBUG_PRINTLN("Second photo captured successfully");
                    currentPhotoStep = 4;
                    flipperMoveTime = millis();
                } else {
                    DEBUG_PRINTLN("ERROR: Second photo capture failed");
                    handleError(STATUS_CAMERA_ERROR);
                    return;
                }
            }
            break;
            
        case 4: // Return flipper to home position
            if (millis() - flipperMoveTime >= FLIPPER_PHOTO_DELAY) {
                DEBUG_PRINTLN("Returning flipper to home position");
                moveFlipperHome();
                flipperMoveTime = millis();
                currentPhotoStep = 5;
            }
            break;
            
        case 5: // Wait for flipper to reach home, then complete
            if (millis() - flipperMoveTime >= SERVO_MOVE_DELAY) {
                DEBUG_PRINTLN("Photography sequence complete");
                DEBUG_PRINT("Images saved: ");
                DEBUG_PRINT(currentImageFilename1);
                DEBUG_PRINT(", ");
                DEBUG_PRINTLN(currentImageFilename2);
                
                // Reset for next coin
                currentPhotoStep = 0;
                changeState(STATE_WAITING_FOR_COIN);
            }
            break;
    }
    
    // Timeout protection for photography sequence
    if (timeInState > PROCESSING_TIMEOUT) {
        DEBUG_PRINTLN("ERROR: Photography sequence timeout");
        handleError(STATUS_TIMEOUT_ERROR);
    }
}

void handleRejecting() {
    setStatusLED(LED_ERROR);
    
    unsigned long timeInState = millis() - stateStartTime;
    
    // Open trapdoor immediately when entering rejection state
    if (timeInState < 100) {
        DEBUG_PRINTLN("Opening rejection trapdoor");
        openTrapdoor();
        trapdoorOpenTime = millis();
    }
    
    // Close trapdoor after specified time
    if (timeInState >= TRAPDOOR_OPEN_TIME) {
        DEBUG_PRINTLN("Closing rejection trapdoor");
        closeTrapdoor();
        
        // Wait a bit more for trapdoor to close, then return to waiting
        if (timeInState >= TRAPDOOR_OPEN_TIME + 1000) {
            DEBUG_PRINTLN("Rejection complete, ready for next coin");
            changeState(STATE_WAITING_FOR_COIN);
        }
    }
}

void handleError(StatusCode error) {
    setStatusLED(LED_ERROR);
    
    static unsigned long lastErrorReport = 0;
    unsigned long currentTime = millis();
    
    // Report error periodically
    if (currentTime - lastErrorReport > 5000) {
        DEBUG_PRINT("ERROR STATE: ");
        switch (error) {
            case STATUS_MULTIPLE_COINS:
                DEBUG_PRINTLN("Multiple coins detected");
                break;
            case STATUS_COIN_DURING_PROCESSING:
                DEBUG_PRINTLN("Coin detected during processing");
                break;
            case STATUS_CAMERA_ERROR:
                DEBUG_PRINTLN("Camera error");
                break;
            case STATUS_STORAGE_ERROR:
                DEBUG_PRINTLN("Storage error");
                break;
            case STATUS_TIMEOUT_ERROR:
                DEBUG_PRINTLN("Timeout error");
                break;
            default:
                DEBUG_PRINTLN("Unknown error");
                break;
        }
        lastErrorReport = currentTime;
        errorCount++;
    }
    
    // Auto-recovery after multiple error reports
    if (errorCount >= 3) {
        DEBUG_PRINTLN("Attempting system recovery...");
        systemReset();
        errorCount = 0;
        changeState(STATE_WAITING_FOR_COIN);
    }
}

// ==================== UTILITY FUNCTIONS ====================

void changeState(CoinMachineState newState) {
    if (newState != currentState) {
        previousState = currentState;
        currentState = newState;
        stateStartTime = millis();
        
        DEBUG_PRINT("State change: ");
        DEBUG_PRINT(getStateName(previousState));
        DEBUG_PRINT(" -> ");
        DEBUG_PRINTLN(getStateName(currentState));
        
        // Reset variables when changing states
        if (newState == STATE_WAITING_FOR_COIN) {
            processingCoin = false;
            resetSensorCount();
            lastError = STATUS_OK;
        }
    }
}

const char* getStateName(CoinMachineState state) {
    switch (state) {
        case STATE_INIT: return "INIT";
        case STATE_WAITING_FOR_COIN: return "WAITING_FOR_COIN";
        case STATE_COIN_DETECTED: return "COIN_DETECTED";
        case STATE_PROCESSING: return "PROCESSING";
        case STATE_PHOTOGRAPHING: return "PHOTOGRAPHING";
        case STATE_REJECTING: return "REJECTING";
        case STATE_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

// ==================== INTERRUPT HANDLERS ====================
// Sensor interrupt handler is defined in hardware_functions.h

// ==================== DEBUG AND MONITORING ====================
void printSystemStatus() {
    DEBUG_PRINTLN("=== System Status ===");
    DEBUG_PRINT("Current State: ");
    DEBUG_PRINTLN(getStateName(currentState));
    DEBUG_PRINT("Time in State: ");
    DEBUG_PRINT(millis() - stateStartTime);
    DEBUG_PRINTLN("ms");
    DEBUG_PRINT("Sensor Trigger Count: ");
    DEBUG_PRINTLN(getSensorTriggerCount());
    DEBUG_PRINT("Error Count: ");
    DEBUG_PRINTLN(errorCount);
    DEBUG_PRINT("Free Heap: ");
    DEBUG_PRINTLN(ESP.getFreeHeap());
    DEBUG_PRINTLN("==================");
}

// Optional: Add serial command processing for debugging
void processSerialCommands() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        
        if (command == "status") {
            printSystemStatus();
        } else if (command == "test") {
            performSystemTest();
        } else if (command == "reset") {
            systemReset();
            changeState(STATE_WAITING_FOR_COIN);
        } else if (command == "photos") {
            // List stored photos
            File root = SPIFFS.open("/");
            File file = root.openNextFile();
            DEBUG_PRINTLN("=== Stored Images ===");
            while (file) {
                if (String(file.name()).startsWith(IMAGE_FILENAME_PREFIX)) {
                    DEBUG_PRINT("- ");
                    DEBUG_PRINT(file.name());
                    DEBUG_PRINT(" (");
                    DEBUG_PRINT(file.size());
                    DEBUG_PRINTLN(" bytes)");
                }
                file = root.openNextFile();
            }
            DEBUG_PRINTLN("==================");
        } else {
            DEBUG_PRINTLN("Available commands: status, test, reset, photos");
        }
    }
} 