/**
 * ADPD1080 + XIAO SAMD21 DAC出力
 * Analog計測器用の心拍データ出力
 * 
 * 機能:
 * - オフセット自動取得（起動時にベースライン測定）
 * - 自動スケーリング（最大・最小値を追跡してDAC範囲に自動マッピング）
 * - DAC出力（A0ピン、10ビット分解能、0～1023）
 * 
 * 対象ボード: Seeed Studio XIAO SAMD21
 * DAC出力: A0ピン（10ビット、0～3.3V）
 * 
 * 接続:
 * ADPD1080 → XIAO
 * ----------------
 * SDA → D4 (I2C SDA)
 * SCL → D5 (I2C SCL)
 * INT → D2
 * VDD → 3.3V
 * GND → GND
 * 
 * DAC出力:
 * A0 → Analog計測器の入力
 */

#include <Wire.h>

// ADPD1080 I2Cアドレス
#define ADPD1080_ADDR  0x64

// ピン定義
#define INT_PIN  2
#define DAC_PIN  A0  // XIAO SAMD21のDAC出力ピン

// DAC設定
#define DAC_RESOLUTION  10  // 10ビット（0～1023）
#define DAC_MAX  1023

// オフセット・スケーリング設定
#define BASELINE_SAMPLES  100  // ベースライン測定のサンプル数
#define AUTOSCALE_WINDOW  500  // 自動スケーリングのウィンドウサイズ

// グローバル変数
volatile bool dataReady = false;

// オフセット・スケーリング変数
uint16_t baselineOffset = 0;  // ベースラインオフセット
uint16_t dataMin = 65535;     // 最小値
uint16_t dataMax = 0;         // 最大値
uint16_t sampleCount = 0;     // サンプルカウンター

// 移動平均フィルタ用
#define FILTER_SIZE 5
uint16_t filterBuffer[FILTER_SIZE] = {0};
uint8_t filterIndex = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  pinMode(INT_PIN, INPUT_PULLUP);
  
  // DAC設定
  analogWriteResolution(DAC_RESOLUTION);
  pinMode(DAC_PIN, OUTPUT);
  analogWrite(DAC_PIN, 0);
  
  Serial.println("=== ADPD1080 + XIAO DAC Output ===");
  Serial.println("Analog Measurement System");
  Serial.println();
  
  delay(100);
  
  // ADPD1080の初期化
  Serial.println("Initializing ADPD1080...");
  if (!initADPD1080()) {
    Serial.println("ERROR: ADPD1080 initialization failed!");
    while(1) {
      // エラーLED点滅
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      delay(200);
    }
  }
  Serial.println("ADPD1080 initialized OK");
  
  // 割り込み設定
  attachInterrupt(digitalPinToInterrupt(INT_PIN), []{ dataReady = true; }, FALLING);
  
  // ベースラインオフセットの自動取得
  Serial.println();
  Serial.println("Measuring baseline offset...");
  Serial.println("Please keep sensor away from skin.");
  delay(2000);
  
  baselineOffset = measureBaseline();
  Serial.print("Baseline offset: ");
  Serial.println(baselineOffset);
  
  // 緑LED（LED1）のGain設定
  setLED1Gain(0x1030);  // 標準的な電流値
  
  Serial.println();
  Serial.println("=== Ready! ===");
  Serial.println("Place sensor on skin.");
  Serial.println("DAC output: A0 pin (0-3.3V)");
  Serial.println();
  Serial.println("Format: Raw | Offset-Corrected | DAC | Min | Max");
  Serial.println("-----------------------------------------------");
  
  delay(1000);
}

void loop() {
  if (dataReady) {
    dataReady = false;
    
    // 心拍の生データを読み取る
    uint16_t rawData = readHeartRateChannel1();
    
    // オフセット補正
    int32_t correctedData = (int32_t)rawData - (int32_t)baselineOffset;
    if (correctedData < 0) correctedData = 0;
    
    // 移動平均フィルタを適用（ノイズ低減）
    uint16_t filteredData = applyMovingAverage((uint16_t)correctedData);
    
    // 最大・最小値の更新（自動スケーリング）
    updateMinMax(filteredData);
    
    // DAC範囲（0～1023）にスケーリング
    uint16_t dacValue = scaleToDAC(filteredData);
    
    // DAC出力
    analogWrite(DAC_PIN, dacValue);
    
    // シリアル出力（デバッグ用）
    Serial.print(rawData);
    Serial.print(" | ");
    Serial.print(filteredData);
    Serial.print(" | ");
    Serial.print(dacValue);
    Serial.print(" | ");
    Serial.print(dataMin);
    Serial.print(" | ");
    Serial.println(dataMax);
    
    // 一定サンプル数ごとにスケーリング範囲をリセット
    sampleCount++;
    if (sampleCount >= AUTOSCALE_WINDOW) {
      // 範囲を少し縮小（次の測定に備える）
      uint16_t range = dataMax - dataMin;
      dataMin += range / 10;
      dataMax -= range / 10;
      if (dataMin > dataMax) {
        dataMin = 0;
        dataMax = 65535;
      }
      sampleCount = 0;
      
      Serial.println("--- Autoscale range updated ---");
    }
  }
}

/**
 * ベースラインオフセットの測定
 * センサーを皮膚から離した状態で測定
 */
uint16_t measureBaseline() {
  uint32_t sum = 0;
  uint16_t validSamples = 0;
  
  Serial.print("Sampling");
  
  for (uint16_t i = 0; i < BASELINE_SAMPLES * 2; i++) {
    while (!dataReady) {
      delay(1);
    }
    dataReady = false;
    
    uint16_t data = readHeartRateChannel1();
    
    // 異常値を除外（センサーエラー対策）
    if (data > 100 && data < 60000) {
      sum += data;
      validSamples++;
    }
    
    if (i % 10 == 0) Serial.print(".");
    
    if (validSamples >= BASELINE_SAMPLES) break;
  }
  
  Serial.println(" Done!");
  
  if (validSamples > 0) {
    return (uint16_t)(sum / validSamples);
  } else {
    Serial.println("WARNING: Could not measure baseline. Using default.");
    return 1000;  // デフォルト値
  }
}

/**
 * 最大・最小値の更新
 */
void updateMinMax(uint16_t value) {
  if (value < dataMin) dataMin = value;
  if (value > dataMax) dataMax = value;
  
  // 最小値と最大値が近すぎる場合、範囲を広げる
  if (dataMax - dataMin < 100) {
    dataMin = (value > 50) ? value - 50 : 0;
    dataMax = value + 50;
  }
}

/**
 * DAC範囲（0～1023）にスケーリング
 */
uint16_t scaleToDAC(uint16_t value) {
  // 範囲が有効かチェック
  if (dataMax <= dataMin) {
    return DAC_MAX / 2;  // 中央値を返す
  }
  
  // リニアスケーリング
  int32_t scaled = ((int32_t)value - (int32_t)dataMin) * DAC_MAX / (dataMax - dataMin);
  
  // 範囲制限
  if (scaled < 0) scaled = 0;
  if (scaled > DAC_MAX) scaled = DAC_MAX;
  
  return (uint16_t)scaled;
}

/**
 * 移動平均フィルタ
 */
uint16_t applyMovingAverage(uint16_t newValue) {
  filterBuffer[filterIndex] = newValue;
  filterIndex = (filterIndex + 1) % FILTER_SIZE;
  
  uint32_t sum = 0;
  for (uint8_t i = 0; i < FILTER_SIZE; i++) {
    sum += filterBuffer[i];
  }
  
  return (uint16_t)(sum / FILTER_SIZE);
}

/**
 * ADPD1080の初期化
 */
bool initADPD1080() {
  // ソフトウェアリセット
  writeReg(0x00, 0x80FF);
  delay(10);
  
  // FIFOクリア
  writeReg(0x10, 0x0000);
  writeReg(0x5F, 0x0001);
  writeReg(0x00, 0x80FF);
  writeReg(0x5F, 0x0000);
  delay(10);
  
  // 基本設定
  writeReg(0x12, 0x0014);  // サンプリング周波数
  writeReg(0x15, 0x0330);  // PD選択
  writeReg(0x11, 0x30A0);  // スロット有効化
  writeReg(0x14, 0x0555);  // AFEトリム
  
  // LED1（緑LED）の設定
  writeReg(0x22, 0x1030);  // LED1ドライバー有効化
  writeReg(0x24, 0x1030);  // LED1電流設定
  writeReg(0x25, 0x630C);  // LED2電流設定
  
  // TIA設定
  writeReg(0x18, 0x1F00);
  writeReg(0x19, 0x3FFF);
  writeReg(0x1A, 0x3FFF);
  writeReg(0x1B, 0x3FFF);
  
  writeReg(0x1E, 0x1F00);
  writeReg(0x1F, 0x3FFF);
  writeReg(0x20, 0x3FFF);
  writeReg(0x21, 0x3FFF);
  
  // 積分器設定
  writeReg(0x35, 0x0220);
  writeReg(0x36, 0x020F);
  writeReg(0x39, 0x1AF8);
  writeReg(0x3B, 0x1AF8);
  writeReg(0x3C, 0x7006);
  
  // LEDパルス設定
  writeReg(0x42, 0x1C35);
  writeReg(0x43, 0xADA5);
  writeReg(0x44, 0x1C34);
  writeReg(0x45, 0xADA5);
  
  // その他の設定
  writeReg(0x34, 0x0000);
  writeReg(0x06, 0x0000);
  writeReg(0x4E, 0x7040);
  writeReg(0x54, 0x0AA0);
  writeReg(0x3F, 0x0320);
  writeReg(0x58, 0x0000);
  writeReg(0x59, 0x0808);
  writeReg(0x5A, 0x0010);
  
  // クロック設定
  writeReg(0x4B, 0x2695);
  writeReg(0x4D, 0x4272);
  
  // サンプリングモード開始
  writeReg(0x10, 0x0001);
  
  delay(100);
  
  // 初期化確認
  uint16_t modeReg = readReg(0x10);
  return (modeReg == 0x0001);
}

/**
 * LED1（緑LED）のGain設定
 */
void setLED1Gain(uint16_t gain) {
  writeReg(0x24, gain);
}

/**
 * 心拍データ（チャネル1）を読み取る
 */
uint16_t readHeartRateChannel1() {
  Wire.beginTransmission(ADPD1080_ADDR);
  Wire.write(0x60);  // FIFO_DATA
  Wire.endTransmission(false);
  
  Wire.requestFrom(ADPD1080_ADDR, (uint8_t)2);
  
  if (Wire.available() >= 2) {
    uint8_t msb = Wire.read();
    uint8_t lsb = Wire.read();
    return (msb << 8) | lsb;
  }
  
  return 0;
}

/**
 * レジスタに書き込む
 */
void writeReg(uint8_t addr, uint16_t val) {
  Wire.beginTransmission(ADPD1080_ADDR);
  Wire.write(addr);
  Wire.write(val >> 8);
  Wire.write(val & 0xFF);
  Wire.endTransmission();
  delayMicroseconds(100);
}

/**
 * レジスタから読み取る
 */
uint16_t readReg(uint8_t addr) {
  Wire.beginTransmission(ADPD1080_ADDR);
  Wire.write(addr);
  Wire.endTransmission(false);
  
  Wire.requestFrom(ADPD1080_ADDR, (uint8_t)2);
  
  if (Wire.available() >= 2) {
    return (Wire.read() << 8) | Wire.read();
  }
  return 0xFFFF;
}
