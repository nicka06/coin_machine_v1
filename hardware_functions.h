#ifndef HARDWARE_FUNCTIONS_H
#define HARDWARE_FUNCTIONS_H

#include "config.h"
#include "esp_camera.h"
#include "FS.h"
#include "SPIFFS.h"
#include <ESP32Servo.h>
#include <FastLED.h>

// ==================== GLOBAL HARDWARE OBJECTS ====================
extern Servo trapdoorServo;
extern Servo flipperServo;
extern CRGB cameraLEDs[NUM_CAMERA_LEDS];

// ==================== CAMERA FUNCTIONS ====================
bool initializeCamera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = CAMERA_Y2_PIN;
    config.pin_d1 = CAMERA_Y3_PIN;
    config.pin_d2 = CAMERA_Y4_PIN;
    config.pin_d3 = CAMERA_Y5_PIN;
    config.pin_d4 = CAMERA_Y6_PIN;
    config.pin_d5 = CAMERA_Y7_PIN;
    config.pin_d6 = CAMERA_Y8_PIN;
    config.pin_d7 = CAMERA_Y9_PIN;
    config.pin_xclk = CAMERA_XCLK_PIN;
    config.pin_pclk = CAMERA_PCLK_PIN;
    config.pin_vsync = CAMERA_VSYNC_PIN;
    config.pin_href = CAMERA_HREF_PIN;
    config.pin_sscb_sda = CAMERA_SIOD_PIN;
    config.pin_sscb_scl = CAMERA_SIOC_PIN;
    config.pin_pwdn = -1;
    config.pin_reset = CAMERA_RESET_PIN;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    
    // Frame size and quality settings
    config.frame_size = CAMERA_FRAME_SIZE;
    config.jpeg_quality = CAMERA_JPEG_QUALITY;
    config.fb_count = 2;
    
    // Initialize camera
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        DEBUG_PRINTLN("Camera init failed");
        return false;
    }
    
    // Set camera parameters
    sensor_t * s = esp_camera_sensor_get();
    s->set_brightness(s, CAMERA_BRIGHTNESS);
    s->set_contrast(s, CAMERA_CONTRAST);
    
    DEBUG_PRINTLN("Camera initialized successfully");
    return true;
}

bool captureAndSaveImage(const char* filename) {
    // Turn on camera lights
    setCameraLights(true);
    delay(CAMERA_WARMUP_TIME);
    
    // Capture image
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
        DEBUG_PRINTLN("Camera capture failed");
        setCameraLights(false);
        return false;
    }
    
    // Save to SPIFFS
    File file = SPIFFS.open(filename, FILE_WRITE);
    if (!file) {
        DEBUG_PRINTLN("Failed to open file for writing");
        esp_camera_fb_return(fb);
        setCameraLights(false);
        return false;
    }
    
    file.write(fb->buf, fb->len);
    file.close();
    esp_camera_fb_return(fb);
    
    // Turn off camera lights
    setCameraLights(false);
    
    DEBUG_PRINT("Image saved: ");
    DEBUG_PRINTLN(filename);
    return true;
}

// ==================== SERVO FUNCTIONS ====================
bool initializeServos() {
    trapdoorServo.attach(TRAPDOOR_SERVO_PIN);
    flipperServo.attach(FLIPPER_SERVO_PIN);
    
    // Move to home positions
    trapdoorServo.write(TRAPDOOR_CLOSED);
    flipperServo.write(FLIPPER_HOME);
    
    delay(1000); // Allow servos to reach position
    
    DEBUG_PRINTLN("Servos initialized");
    return true;
}

void setTrapdoorPosition(int angle) {
    trapdoorServo.write(angle);
    DEBUG_PRINT("Trapdoor moved to: ");
    DEBUG_PRINTLN(angle);
}

void setFlipperPosition(int angle) {
    flipperServo.write(angle);
    DEBUG_PRINT("Flipper moved to: ");
    DEBUG_PRINTLN(angle);
}

void openTrapdoor() {
    setTrapdoorPosition(TRAPDOOR_OPEN);
}

void closeTrapdoor() {
    setTrapdoorPosition(TRAPDOOR_CLOSED);
}

void moveFlipperHome() {
    setFlipperPosition(FLIPPER_HOME);
}

void moveFlipperToSide1() {
    setFlipperPosition(FLIPPER_SIDE_1);
}

void moveFlipperToSide2() {
    setFlipperPosition(FLIPPER_SIDE_2);
}

// ==================== LED FUNCTIONS ====================
bool initializeLEDs() {
    // Initialize status LED pins
    pinMode(STATUS_LED_R_PIN, OUTPUT);
    pinMode(STATUS_LED_G_PIN, OUTPUT);
    pinMode(STATUS_LED_B_PIN, OUTPUT);
    
    // Initialize camera LEDs (WS2812B)
    FastLED.addLeds<WS2812B, CAMERA_LIGHTS_PIN, GRB>(cameraLEDs, NUM_CAMERA_LEDS);
    FastLED.setBrightness(CAMERA_LED_BRIGHTNESS);
    
    // Turn off all LEDs initially
    setStatusLED(LED_OFF);
    setCameraLights(false);
    
    DEBUG_PRINTLN("LEDs initialized");
    return true;
}

void setStatusLED(RGBColor color) {
    analogWrite(STATUS_LED_R_PIN, map(color.r, 0, 255, 0, STATUS_LED_BRIGHTNESS));
    analogWrite(STATUS_LED_G_PIN, map(color.g, 0, 255, 0, STATUS_LED_BRIGHTNESS));
    analogWrite(STATUS_LED_B_PIN, map(color.b, 0, 255, 0, STATUS_LED_BRIGHTNESS));
}

void setCameraLights(bool on) {
    if (on) {
        fill_solid(cameraLEDs, NUM_CAMERA_LEDS, CRGB::White);
    } else {
        fill_solid(cameraLEDs, NUM_CAMERA_LEDS, CRGB::Black);
    }
    FastLED.show();
}

void flashCameraLights() {
    setCameraLights(true);
    delay(CAMERA_FLASH_DURATION);
    setCameraLights(false);
}

// ==================== SENSOR FUNCTIONS ====================
volatile bool sensorTriggered = false;
volatile unsigned long lastSensorTrigger = 0;
volatile int sensorTriggerCount = 0;
unsigned long sensorWindowStart = 0;

void IRAM_ATTR sensorInterrupt() {
    unsigned long currentTime = millis();
    
    // Debounce check
    if (currentTime - lastSensorTrigger < SENSOR_DEBOUNCE_TIME) {
        return;
    }
    
    lastSensorTrigger = currentTime;
    
    // Start new detection window if needed
    if (sensorTriggerCount == 0) {
        sensorWindowStart = currentTime;
    }
    
    sensorTriggerCount++;
    sensorTriggered = true;
    
    DEBUG_PRINT("Sensor triggered, count: ");
    DEBUG_PRINTLN(sensorTriggerCount);
}

bool initializeSensor() {
    pinMode(OPTICAL_SENSOR_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(OPTICAL_SENSOR_PIN), sensorInterrupt, FALLING);
    
    DEBUG_PRINTLN("Optical sensor initialized");
    return true;
}

bool isSensorTriggered() {
    return sensorTriggered;
}

void clearSensorTrigger() {
    sensorTriggered = false;
}

int getSensorTriggerCount() {
    return sensorTriggerCount;
}

void resetSensorCount() {
    sensorTriggerCount = 0;
    sensorWindowStart = 0;
}

bool isMultipleCoinDetected() {
    unsigned long currentTime = millis();
    
    // Check if we're within the detection window
    if (sensorTriggerCount > 0 && (currentTime - sensorWindowStart) > MULTI_COIN_TIMEOUT) {
        bool multipleCoins = (sensorTriggerCount >= MULTI_COIN_THRESHOLD);
        DEBUG_PRINT("Coin detection complete. Count: ");
        DEBUG_PRINT(sensorTriggerCount);
        DEBUG_PRINT(", Multiple: ");
        DEBUG_PRINTLN(multipleCoins ? "YES" : "NO");
        return multipleCoins;
    }
    
    return false;
}

// ==================== STORAGE FUNCTIONS ====================
bool initializeStorage() {
    if (!SPIFFS.begin(true)) {
        DEBUG_PRINTLN("SPIFFS initialization failed");
        return false;
    }
    
    DEBUG_PRINTLN("SPIFFS initialized");
    return true;
}

String generateImageFilename() {
    static int imageCounter = 0;
    imageCounter++;
    
    String filename = IMAGE_FILENAME_PREFIX;
    filename += String(millis());
    filename += "_";
    filename += String(imageCounter);
    filename += IMAGE_FILENAME_SUFFIX;
    
    return filename;
}

void cleanupOldImages() {
    // Simple cleanup - delete oldest files if we have too many
    // This is a basic implementation; could be enhanced
    File root = SPIFFS.open("/");
    int fileCount = 0;
    
    File file = root.openNextFile();
    while (file) {
        if (String(file.name()).startsWith(IMAGE_FILENAME_PREFIX)) {
            fileCount++;
        }
        file = root.openNextFile();
    }
    
    if (fileCount > MAX_IMAGES_STORED) {
        DEBUG_PRINTLN("Cleaning up old images...");
        // Simple cleanup - in a real implementation, you'd sort by date
        root.rewindDirectory();
        file = root.openNextFile();
        int deleted = 0;
        while (file && deleted < (fileCount - MAX_IMAGES_STORED)) {
            if (String(file.name()).startsWith(IMAGE_FILENAME_PREFIX)) {
                String filename = "/" + String(file.name());
                file.close();
                SPIFFS.remove(filename);
                deleted++;
                DEBUG_PRINT("Deleted: ");
                DEBUG_PRINTLN(filename);
            } else {
                file = root.openNextFile();
            }
        }
    }
}

// ==================== UTILITY FUNCTIONS ====================
void systemReset() {
    DEBUG_PRINTLN("Performing system reset...");
    
    // Reset all hardware to safe states
    closeTrapdoor();
    moveFlipperHome();
    setStatusLED(LED_OFF);
    setCameraLights(false);
    resetSensorCount();
    
    delay(1000);
    DEBUG_PRINTLN("System reset complete");
}

bool performSystemTest() {
    DEBUG_PRINTLN("Starting system test...");
    
    // Test status LED
    setStatusLED(LED_READY);
    delay(500);
    setStatusLED(LED_PROCESSING);
    delay(500);
    setStatusLED(LED_BUSY);
    delay(500);
    setStatusLED(LED_ERROR);
    delay(500);
    setStatusLED(LED_OFF);
    
    // Test servos
    DEBUG_PRINTLN("Testing servos...");
    setTrapdoorPosition(45);
    delay(1000);
    closeTrapdoor();
    delay(1000);
    
    setFlipperPosition(45);
    delay(1000);
    setFlipperPosition(90);
    delay(1000);
    moveFlipperHome();
    delay(1000);
    
    // Test camera lights
    DEBUG_PRINTLN("Testing camera lights...");
    setCameraLights(true);
    delay(1000);
    setCameraLights(false);
    
    DEBUG_PRINTLN("System test complete");
    return true;
}

#endif // HARDWARE_FUNCTIONS_H 