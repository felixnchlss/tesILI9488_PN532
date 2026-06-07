#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include "time.h"

// --- Include Library untuk PN532 I2C ---
#include <Wire.h>
#include <Adafruit_PN532.h>

// --- Konfigurasi WiFi & Waktu (WIB) ---
const char* ssid       = "CEIOT";     // Ganti dengan nama WiFi Anda
const char* password   = "CE-1OT@!";  // Ganti dengan password WiFi
const char* ntpServer  = "pool.ntp.org";
const long  gmtOffset_sec = 7 * 3600; // Zona Waktu WIB (UTC+7)
const int   daylightOffset_sec = 0;

// --- Konfigurasi Pin Rotary Encoder ---
#define PIN_ENCODER_A 32  // DT  (Aman untuk upload)
#define PIN_ENCODER_B 13  // CLK
#define PIN_TOMBOL 14     // SW

// --- Konfigurasi Pin Dummy PN532 (Wajib untuk Constructor) ---
// Penggunaan aslinya diatur oleh pin default I2C ESP32 (SDA=21, SCL=22)
#define PN532_IRQ   2 
#define PN532_RESET 3 
Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

TFT_eSPI tft = TFT_eSPI();

// --- Data Menu ---
const int JUMLAH_MENU = 6;
String menuItems[JUMLAH_MENU] = {
  "1. Prototyping & Development",
  "2. Educational Workshops",
  "3. Research & Experimentation",
  "4. Learning & Discussion",
  "5. Collaborative Projects",
  "6. Other"
};

// --- Variabel Global ---
volatile int indexMenu = 0;
volatile bool menuBerubah = true;
volatile unsigned long lastInterruptTime = 0;
int detikTerakhir = -1; 

// --- Fungsi Interrupt Rotary Encoder ---
void IRAM_ATTR bacaEncoder() {
  unsigned long interruptTime = millis();
  
  if (interruptTime - lastInterruptTime > 50) {
    if (digitalRead(PIN_ENCODER_B) != digitalRead(PIN_ENCODER_A)) {
      indexMenu++;
      if (indexMenu >= JUMLAH_MENU) indexMenu = 0; 
    } else {
      indexMenu--;
      if (indexMenu < 0) indexMenu = JUMLAH_MENU - 1; 
    }
    menuBerubah = true;
  }
  lastInterruptTime = interruptTime;
}

// --- Fungsi Menggambar Layar Jam ---
void tampilkanJam() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return; 
  }
  
  if (timeinfo.tm_sec != detikTerakhir) {
    char bufferJam[10];
    strftime(bufferJam, sizeof(bufferJam), "%H:%M:%S", &timeinfo);
    
    tft.setTextFont(4);
    tft.setTextDatum(TR_DATUM); 
    tft.setTextColor(TFT_WHITE, TFT_BLACK); 
    tft.drawString(bufferJam, tft.width() - 10, 10);
    
    detikTerakhir = timeinfo.tm_sec;
  }
}

// --- Fungsi Menggambar Menu ---
void gambarMenu() {
  int x = 20; 
  int yAwal = 60;
  int jarakY = 40;

  tft.setTextDatum(ML_DATUM); 
  tft.setTextFont(4); 

  for (int i = 0; i < JUMLAH_MENU; i++) {
    int yKotak = yAwal + (i * jarakY);
    int yTeks = yKotak + 15; 
    
    if (i == indexMenu) {
      tft.fillRect(10, yKotak, tft.width() - 20, 30, tft.color565(50, 50, 50));
      tft.setTextColor(TFT_YELLOW); 
    } else {
      tft.fillRect(10, yKotak, tft.width() - 20, 30, TFT_BLACK);
      tft.setTextColor(TFT_WHITE); 
    }
    tft.drawString(menuItems[i], x, yTeks);
  }
}

// --- Fungsi Recovery Tampilan (Untuk mengembalikan elemen UI atas) ---
void gambarHeader() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextFont(4);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Select Purpose:", 10, 10);
  tft.drawLine(10, 40, tft.width() - 10, 40, TFT_CYAN);
  detikTerakhir = -1; // Memaksa jam tergambar ulang di loop
  menuBerubah = true; // Memaksa menu tergambar ulang
}

void setup() {
  Serial.begin(115200);

    // --- Inisialisasi PN532 (I2C) ---
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("PN532 tidak ditemukan. Periksa kabel!");
  } else {
    Serial.print("Ditemukan chip PN5"); Serial.println((versiondata >> 24) & 0xFF, HEX);
    nfc.SAMConfig(); // Setup PN532 untuk membaca kartu
    Serial.println("Sistem PN532 Siap. Menunggu kartu...");
  }

  // --- Inisialisasi Layar ---
  tft.init();
  tft.setRotation(1);
  gambarHeader();

  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("WiFi..", tft.width() - 10, 10);

  // --- Mulai koneksi WiFi ---
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Terhubung!");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  } else {
    tft.fillRect(tft.width() - 100, 0, 100, 30, TFT_BLACK); 
  }

  // --- Konfigurasi Pin Encoder ---
  pinMode(PIN_ENCODER_A, INPUT_PULLUP);
  pinMode(PIN_ENCODER_B, INPUT_PULLUP);
  pinMode(PIN_TOMBOL, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_A), bacaEncoder, FALLING);
  Serial.println("Sistem Siap! Coba putar encoder.");
}

void loop() {
  // Update Jam dan Menu di Layar
  tampilkanJam();

  if (menuBerubah) {
    gambarMenu();
    menuBerubah = false;
  }

  // --- 1. Cek tombol Rotary ditekan ---
  if (digitalRead(PIN_TOMBOL) == LOW) {
    delay(50); 
    if (digitalRead(PIN_TOMBOL) == LOW) { 
      Serial.print("Purpose dipilih: ");
      Serial.println(menuItems[indexMenu]);
      
      tft.fillScreen(TFT_DARKGREEN);
      tft.setTextColor(TFT_WHITE);
      tft.setTextDatum(MC_DATUM); 
      tft.drawString("Purpose Selected!", tft.width()/2, tft.height()/2 - 20);
      tft.setTextColor(TFT_YELLOW);
      tft.drawString(menuItems[indexMenu], tft.width()/2, tft.height()/2 + 20);
      
      delay(1200); 
      gambarHeader();
      while(digitalRead(PIN_TOMBOL) == LOW); 
    }
  }

  // --- 2. Cek Deteksi Kartu RFID ---
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer penyimpan UID 
  uint8_t uidLength;                        // Panjang UID

  // Coba mendeteksi kartu (Pakai timeout 50ms agar looping jam & menu tidak macet)
  bool success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 50);
  
  if (success) {
    Serial.println("Kartu RFID Terdeteksi!");

    // Menampilkan tulisan "Kartu terdeteksi" di layar
    tft.fillScreen(TFT_BLUE); 
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(MC_DATUM); 
    tft.drawString("Kartu terdeteksi", tft.width()/2, tft.height()/2);
    
    delay(2000); // Tahan tampilan selama 2 detik sebelum kembali ke menu
    gambarHeader();
  }
}