#ifndef CONFIG_H
#define CONFIG_H

// ==================== PIN DEFINITIONS ====================
// Servo pins (avoiding camera pin conflicts)
#define TRAPDOOR_SERVO_PIN    16
#define FLIPPER_SERVO_PIN     17

// Sensor pins
#define OPTICAL_SENSOR_PIN    2   // Interrupt capable pin (changed from 21 to avoid I2C conflict)

// Camera pins (OV2640 - using default ESP32-CAM pins)
#define CAMERA_SDA_PIN        21
#define CAMERA_SCL_PIN        22
#define CAMERA_RESET_PIN      -1  // Not connected
#define CAMERA_XCLK_PIN       0
#define CAMERA_SIOD_PIN       26
#define CAMERA_SIOC_PIN       27
#define CAMERA_Y9_PIN         35
#define CAMERA_Y8_PIN         34
#define CAMERA_Y7_PIN         39
#define CAMERA_Y6_PIN         36
#define CAMERA_Y5_PIN         19
#define CAMERA_Y4_PIN         18
#define CAMERA_Y3_PIN         5
#define CAMERA_Y2_PIN         4
#define CAMERA_VSYNC_PIN      25
#define CAMERA_HREF_PIN       23
#define CAMERA_PCLK_PIN       22

// LED pins
#define STATUS_LED_R_PIN      12  // RGB status indicator
#define STATUS_LED_G_PIN      13
#define STATUS_LED_B_PIN      14
#define CAMERA_LIGHTS_PIN     15  // WS2812B strip data pin

// ==================== TIMING CONSTANTS ====================
// Servo timing (milliseconds)
#define TRAPDOOR_OPEN_TIME    2000    // How long trapdoor stays open
#define SERVO_MOVE_DELAY      500     // Delay between servo movements
#define FLIPPER_PHOTO_DELAY   300     // Delay after flipper moves before photo

// Sensor timing
#define SENSOR_DEBOUNCE_TIME  50      // Debounce delay for optical sensor
#define MULTI_COIN_TIMEOUT    1000    // Time window to detect multiple coins

// Camera timing
#define CAMERA_FLASH_DURATION 200     // LED flash duration during photo
#define CAMERA_WARMUP_TIME    100     // Camera stabilization time

// State timeouts
#define PROCESSING_TIMEOUT    10000   // Max time in processing state
#define RESET_TIMEOUT         30000   // Auto-reset if stuck

// ==================== SERVO POSITIONS ====================
// Trapdoor positions (degrees)
#define TRAPDOOR_CLOSED       0
#define TRAPDOOR_OPEN         90

// Flipper positions (degrees)
#define FLIPPER_HOME          0       // Starting position
#define FLIPPER_SIDE_1        90      // First photo position
#define FLIPPER_SIDE_2        180     // Second photo position (flipped)

// ==================== SENSOR THRESHOLDS ====================
#define MIN_COIN_BLOCK_TIME   10      // Minimum ms for valid coin detection
#define MAX_SINGLE_COIN_TIME  200     // Maximum ms for single coin passage
#define MULTI_COIN_THRESHOLD  2       // Number of interrupts indicating multiple coins

// ==================== CAMERA SETTINGS ====================
#define CAMERA_FRAME_SIZE     FRAMESIZE_VGA  // 640x480
#define CAMERA_JPEG_QUALITY   10             // JPEG quality (0-63, lower = better)
#define CAMERA_BRIGHTNESS     0              // -2 to 2
#define CAMERA_CONTRAST       0              // -2 to 2

// ==================== LED SETTINGS ====================
#define NUM_CAMERA_LEDS       8       // Number of WS2812B LEDs
#define CAMERA_LED_BRIGHTNESS 128     // 0-255
#define STATUS_LED_BRIGHTNESS 100     // PWM value for status LED

// ==================== SYSTEM STATES ====================
enum CoinMachineState {
    STATE_INIT,
    STATE_WAITING_FOR_COIN,
    STATE_COIN_DETECTED,
    STATE_PROCESSING,
    STATE_PHOTOGRAPHING,
    STATE_REJECTING,
    STATE_ERROR
};

// ==================== STATUS CODES ====================
enum StatusCode {
    STATUS_OK,
    STATUS_MULTIPLE_COINS,
    STATUS_COIN_DURING_PROCESSING,
    STATUS_CAMERA_ERROR,
    STATUS_STORAGE_ERROR,
    STATUS_TIMEOUT_ERROR
};

// ==================== LED COLORS ====================
struct RGBColor {
    uint8_t r, g, b;
};

// Status LED color definitions
const RGBColor LED_READY = {0, 255, 0};        // Green - ready for coin
const RGBColor LED_PROCESSING = {255, 255, 0}; // Yellow - processing coin
const RGBColor LED_BUSY = {255, 0, 0};         // Red - busy/photographing
const RGBColor LED_ERROR = {255, 0, 255};      // Magenta - error
const RGBColor LED_OFF = {0, 0, 0};            // Off

// ==================== FILE STORAGE ====================
#define MAX_IMAGES_STORED     100     // Maximum images before cleanup
#define IMAGE_FILENAME_PREFIX "/coin_"
#define IMAGE_FILENAME_SUFFIX ".jpg"

// ==================== DEBUG SETTINGS ====================
#define DEBUG_ENABLED         true
#define SERIAL_BAUD_RATE      115200

#if DEBUG_ENABLED
    #define DEBUG_PRINT(x)    Serial.print(x)
    #define DEBUG_PRINTLN(x)  Serial.println(x)
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
#endif

#endif // CONFIG_H 