# XiaoPulsesensorExamples
XIAOでPulseSensorの例
## XIAO
https://wiki.seeedstudio.com/ja/Seeeduino-XIAO/

## pin
DAC　D0(10bit)　0から1024
SDA　D4
SCL D5
D1からD3はフリー
(UART) D6　TX、D7　RX
(SPI) D8　SCK、D9 MISO、D10 MOSI

### Arduino
DAC
analogWriteResolution(10)
analogWrite(A0, <value>)


## max30101only
https://www.switch-science.com/products/10598
https://github.com/sparkfun/SparkFun_MAX3010x_Sensor_Library


## max30101 & max32664
https://www.switch-science.com/products/5875
https://github.com/sparkfun/SparkFun_Bio_Sensor_Hub_Library
https://github.com/sparkfun/SparkFun_Pulse_Oximeter_Heart_Rate_Sensor

これは指用らしい

max32664には複数のバージョンがあり、測定箇所や機能が異なります。主に指での測定に対応しており、以下のバージョンがあります。

があります。詳細は以下の通りです。

- **バージョンA**: 基本的なパルス検出機能を備えたモデル
- **バージョンB**: 心拍変動(HRV)測定機能が追加されたモデル
- **バージョンC**: SpO2(酸素飽和度)測定に対応したモデル
- **バージョンD**: 最新版で、複数のセンサーが統合され、より高精度な測定が可能

