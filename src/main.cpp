#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>

// --- Konfigurasi Pin RFID PN532 (Mode I2C) ---
// Karena menggunakan I2C, pin IRQ dan RESET tidak kita pakai secara fisik.
// Kita definisikan ke pin bebas (dummy) di ESP32.
#define PN532_IRQ   (32) 
#define PN532_RESET (33) 
Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

// --- Inisialisasi Layar TFT ---
TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);

  // ==========================================
  // 1. SETUP LAYAR LCD TFT
  // ==========================================
  tft.init();
  tft.setRotation(1); // Mode Landscape
  tft.fillScreen(TFT_BLACK);
  
  tft.setTextFont(4);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextDatum(MC_DATUM); // Teks rata tengah layar
  tft.drawString("Memulai Sistem...", tft.width()/2, tft.height()/2);

  // ==========================================
  // 2. SETUP MODUL RFID PN532
  // ==========================================
  Serial.println("\n--- Memulai I2C untuk RFID ---");
  
  // Kunci pin SDA=21 dan SCL=22 secara eksplisit
  Wire.begin(21, 22); 
  
  // Beri waktu 1 detik agar chip PN532 & LCD menyala stabil
  delay(1000); 

  nfc.begin();
  uint32_t versiondata = 0;
  
  // Sistem Retry: Coba hubungi PN532 sampai 3 kali
  for (int i = 1; i <= 3; i++) {
    versiondata = nfc.getFirmwareVersion();
    if (versiondata) {
      Serial.println("Berhasil terhubung ke PN532!");
      break; // Jika berhasil, keluar dari loop retry
    }
    Serial.print("Percobaan ke-"); Serial.print(i); Serial.println(" gagal. Mencoba lagi...");
    delay(500);
  }
  
  // Jika setelah 3 kali tetap gagal
  if (!versiondata) {
    Serial.println("ERROR KRITIS: Modul PN532 Tidak Merespon!");
    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.drawString("Error: RFID Tidak Terdeteksi!", tft.width()/2, tft.height()/2);
    while (1); // Program dihentikan di sini. Cek kabel & daya.
  } 
  
  // Jika berhasil
  Serial.print("Ditemukan chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX);
  
  // Setting penting: Batasi waktu tunggu (retries) agar ESP32 tidak macet
  nfc.setPassiveActivationRetries(0x01);
  nfc.SAMConfig();
  Serial.println("RFID Siap Digunakan!");

  // ==========================================
  // 3. TAMPILAN STANDBY (SIAP MEMBACA)
  // ==========================================
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Silakan Tempelkan Kartu", tft.width()/2, tft.height()/2);
}

void loop() {
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 }; // Tempat menyimpan ID kartu
  uint8_t uidLength;                       // Panjang ID kartu
  
  // Coba baca kartu dengan batas waktu tunggu (timeout) 50 milidetik
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 50);
  
  // Jika kartu terdeteksi
  if (success) { 
    Serial.println("KARTU BERHASIL DIBACA!");
    
    // Tampilkan ID Kartu di Serial Monitor (opsional)
    Serial.print("UID Kartu: ");
    for (uint8_t i = 0; i < uidLength; i++) {
      Serial.print(uid[i], HEX);
      Serial.print(" ");
    }
    Serial.println("");

    // 1. Ubah layar jadi Biru dan beri pesan
    tft.fillScreen(TFT_BLUE); 
    tft.setTextColor(TFT_WHITE, TFT_BLUE); // Teks putih, background biru
    tft.drawString("KARTU DITEMUKAN!", tft.width()/2, tft.height()/2);
    
    // 2. Tahan selama 2 detik agar pesan bisa dibaca oleh user
    delay(2000); 
    
    // 3. Kembalikan layar ke mode Standby
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Silakan Tempelkan Kartu", tft.width()/2, tft.height()/2);
  }
}