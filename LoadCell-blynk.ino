// Blynk credentials
#define BLYNK_TEMPLATE_ID "TMPL69MBJJDgC"
#define BLYNK_TEMPLATE_NAME "LoadCell"
#define BLYNK_AUTH_TOKEN "4btrlp9fX3bmriFlNhmiZewCokUuSliP"

#include <HX711_ADC.h>

#include <ESP8266WiFi.h>

#include <EEPROM.h>

#include <BlynkSimpleEsp8266.h>

// Pins
const int HX711_dout = D2;
const int HX711_sck = D1;

// HX711 constructor
HX711_ADC LoadCell(HX711_dout, HX711_sck);

// WiFi credentials
const char * ssid = "PISANGIN";
const char * password = "12345678";

// EEPROM address for calibration value
const int calVal_eepromAdress = 0;

void setup() {
    Serial.begin(115200);
    delay(10);

    // Connect to WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    // Initialize EEPROM
    EEPROM.begin(512);

    // Initialize LoadCell
    LoadCell.begin();
    unsigned long stabilizingtime = 2000;
    boolean _tare = true;
    LoadCell.start(stabilizingtime, _tare);
    if (LoadCell.getTareTimeoutFlag()) {
        Serial.println("Tare timeout, check wiring and pin configuration");
    } else {
        float calValue = 1.0;
        EEPROM.get(calVal_eepromAdress, calValue);
        LoadCell.setCalFactor(calValue);
    }

    // Initialize Blynk
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, password);
}

void loop() {
    static boolean newDataReady = false;
    const int serialPrintInterval = 0;
    const float threshold = 10.0;

    // Check for new data/start next conversion
    if (LoadCell.update()) newDataReady = true;

    // Get smoothed value from the dataset
    if (newDataReady) {
        if (millis() > serialPrintInterval) {
            float weight = LoadCell.getData();
            if (weight < threshold) {
                weight = 0;
            }
            newDataReady = false;
            sendToBlynk(weight);
        }
    }

    // Receive command from serial terminal
    if (Serial.available() > 0) {
        char inByte = Serial.read();
        if (inByte == 't') LoadCell.tareNoDelay();
        else if (inByte == 'r') calibrate();
        else if (inByte == 'c') changeSavedCalFactor();
    }

    // Check if last tare operation is complete
    if (LoadCell.getTareStatus()) {
        Serial.println("Tare complete");
    }

    Blynk.run();
}

void sendToBlynk(float data) {
    if (data >= 0) {
        Blynk.virtualWrite(V1, data);
        Serial.println("Data sent to Blynk successfully!");
    } else {
        Serial.println("Nilai data negatif, tidak dikirim ke Blynk.");
    }
}

void calibrate() {
    Serial.println("Start calibration:");
    Serial.println("Place the load cell on a level stable surface.");
    Serial.println("Remove any load applied to the load cell.");
    Serial.println("Send 't' from serial monitor to set the tare offset.");

    boolean _resume = false;
    while (!_resume) {
        LoadCell.update();
        if (Serial.available() > 0) {
            char inByte = Serial.read();
            if (inByte == 't') LoadCell.tareNoDelay();
        }
        if (LoadCell.getTareStatus()) {
            Serial.println("Tare complete");
            _resume = true;
        }
    }

    Serial.println("Now, place your known mass on the loadcell.");
    Serial.println("Then send the weight of this mass (i.e. 100.0) from serial monitor.");

    float known_mass = 0;
    _resume = false;
    while (!_resume) {
        LoadCell.update();
        if (Serial.available() > 0) {
            known_mass = Serial.parseFloat();
            if (known_mass != 0) {
                Serial.print("Known mass is: ");
                Serial.println(known_mass);
                _resume = true;
            }
        }
    }

    LoadCell.refreshDataSet();
    float newCalibrationValue = LoadCell.getNewCalibration(known_mass);

    Serial.print("New calibration value has been set to: ");
    Serial.print(newCalibrationValue);
    Serial.println(", use this as calibration value (calFactor) in your project sketch.");

    EEPROM.put(calVal_eepromAdress, newCalibrationValue);
    #if defined(ESP8266) || defined(ESP32)
    EEPROM.commit();
    #endif
    Serial.println("Calibration value saved to EEPROM.");
    Serial.println("End calibration");
    Serial.println("To re-calibrate, send 'r' from serial monitor.");
    Serial.println("For manual edit of the calibration value, send 'c' from serial monitor.");
}

void changeSavedCalFactor() {
    float oldCalibrationValue = LoadCell.getCalFactor();
    boolean _resume = false;
    Serial.print("Current value is: ");
    Serial.println(oldCalibrationValue);
    Serial.println("Now, send the new value from serial monitor, i.e. 696.0");
    float newCalibrationValue;
    while (!_resume) {
        if (Serial.available() > 0) {
            newCalibrationValue = Serial.parseFloat();
            if (newCalibrationValue != 0) {
                Serial.print("New calibration value is: ");
                Serial.println(newCalibrationValue);
                LoadCell.setCalFactor(newCalibrationValue);

                EEPROM.put(calVal_eepromAdress, newCalibrationValue);
                #if defined(ESP8266) || defined(ESP32)
                EEPROM.commit();
                #endif
                _resume = true;
            }
        }
    }
    Serial.println("End change calibration value");
}
