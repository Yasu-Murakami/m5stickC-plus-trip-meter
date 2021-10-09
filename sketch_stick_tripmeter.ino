
//#define debug

/* ************************************** // *************************
 * sketch_stick_tripmeter
 * ODD        : 1~999999km
 * Trip1      : 0.0~999.9km     * D_MAX_TRIP  で定義
 * Trip2      : 0.0~999.9km     * D_MAX_TRIP  で定義
 * Count down : 999.9~0km       * D_DEF_CDOWN で定義
 * Max        : 999km/h         * D_MAX_SPEED で定義
 * M5StickC Plus
 * Ver. 1.0 2021-10-07
 **************************************** // ********************** */
#include <M5StickCPlus.h>
#include <EEPROM.h>

// 定数宣言
#define       D_MODE_ODD      0           // ODD
#define       D_MODE_TRIP1    1           // TRIP 1
#define       D_MODE_TRIP2    2           // TRIP 2
#define       D_MODE_CDOWN    3           // カウントダウントリップ
#define       D_MODE_MAXSPD   4           // 最高速度
#define       D_MODE_BIGSPD   5           // デカ文字速度表示
#define       D_MODE_END      6           // 最後（ダミー）
#define       D_DEF_CDOWN     6000        // カウントダウントリップ初期値
#define       D_MAX_TRIP      99999       // トリップ最大値 9999.9
#define       D_MAX_SPEED     180         // ノイズ対策

#define       D_FONT_A8PXASC  1           // Adafruit 8ピクセルASCIIフォント
#define       D_FONT_16PXASC  2           // 16ピクセルASCIIフォント
#define       D_FONT_26PXASC  4           // 26ピクセルASCIIフォント
#define       D_FONT_26PXNUM  6           // 26ピクセル数字フォント
#define       D_FONT_48PX7SG  7           // 48ピクセル7セグ風フォント
#define       D_FONT_75PXNUM  8           // 75ピクセル数字フォント

const int     C_PIN_NEUTRAL = 32;         // ニュートラル
const int     C_PIN_INPUT   = 33;         // パルス入力
const int     C_PIN_LED     = 10;         // 内蔵LED
const int     C_ADDR_KEYNO  = 123;        // チェックキー(変更で全メモリクリア）
const int     C_ADDR_ODD    = 0;          // オドメーター
const int     C_ADDR_CHECK  = 4;          // 初期チェック用 KEYNO なら初期設定済み
const int     C_ADDR_TRIP1  = 8;          // TRIP1 2BYTE
const int     C_ADDR_TRIP2  = 12;         // TRIP2 2BYTE
const int     C_ADDR_CDOWN  = 16;         // カウントダウントリップの初期値 2BYTE
const int     C_ADDR_FSCALE = 20;         // 最高時速表示（フルスケール）
const int     C_ADDR_BRIGHT = 24;         // 画面の輝度
const int     C_ADDR_MODE   = 28;         // 洗濯中の表示モード

const double  C_PULSE_DIST = 1.0;         // 入力１パルスの距離m
const int     C_PULSE_TIMEOUT = 1;        // 停止チェック秒数(指定秒パルスがなければメーターを０にする）

// 割り込み処理で使用する変数は誤作動防止のため最適化をしない
volatile unsigned long lngPulseCnt = 0;   // パルスカウンタ
volatile long lngPeriod = 0;              // パルスの間隔μsec（周期）
volatile int  intSpeed  = 0;              // 現在の速度
volatile long lngOdd    = 0;              // ODDメーター
volatile int  intTrip1  = 0;              // トリップ１
volatile int  intTrip2  = 0;              // トリップ２
volatile int  intCdown  = 0;              // カウントダウントリップ
volatile int  intFullScale = 0;           // 最高速度
volatile int  intMode   = D_MODE_ODD;     // 初期モード
volatile int  intRequestSave = false;     // EEPROM書き込み必要
volatile int  intMinPulse;                // 最小パルス値
volatile int  intNeutral;                 // ニュートラル true=on, false=off

int intBatteryMode = false;               // バッテリーモード時は自動電源OFFしない
int intFgcolor;                           // 前景色
int intBgcolor;                           // 背景色
int lastVolTime = 0;                      // 最後に電圧確認した時間
int intBright = 9;                        // 画面の明るさ 7-12

// スプライト
TFT_eSprite tftSp1 = TFT_eSprite(&M5.Lcd);// ガイダンス
TFT_eSprite tftSp2 = TFT_eSprite(&M5.Lcd);// 速度表示 
TFT_eSprite tftSp3 = TFT_eSprite(&M5.Lcd);// トリップ表示
TFT_eSprite tftSp4 = TFT_eSprite(&M5.Lcd);// ODDメーター表示
TFT_eSprite tftSp5 = TFT_eSprite(&M5.Lcd);// 最高速度表示 
TFT_eSprite tftSp6 = TFT_eSprite(&M5.Lcd);// デカ文字速度表示 
TFT_eSprite tftCfg = TFT_eSprite(&M5.Lcd);// 設定画面

/*
 * 初期設定
 */
void setup() {

  M5.begin();
  pinMode(C_PIN_NEUTRAL, INPUT_PULLUP);   // ニュートラル時GND
  pinMode(C_PIN_INPUT,   INPUT);
  pinMode(C_PIN_LED,     OUTPUT);
 
  // EEPROM 初期化
  Serial.println("\nTesting EEPROM Library\n");
  if (!EEPROM.begin(1000)) {
    Serial.println("Failed to initialise EEPROM");
    Serial.println("Restarting...");
    delay(1000);
    ESP.restart();
  }

  readEEPROM();                           // EEPROMの読み込み
 
  // 共通初期化
  M5.begin();
  M5.Lcd.setRotation(1);                  // 横画面 スイッチ右
  M5.Axp.ScreenBreath(intBright);         // 画面の明るさ 7-12
  M5.Lcd.fillScreen(NAVY);

  digitalWrite(C_PIN_LED, LOW);           // LED点灯
  delay(50);
  digitalWrite(C_PIN_LED, HIGH);          // LED消灯

  // diagnostic
  M5.Lcd.fillScreen(MAGENTA);
  delay(200);
  M5.Lcd.fillScreen(BLUE);
  delay(200);
  M5.Lcd.fillScreen(GREEN);
  delay(200);
  M5.Lcd.fillScreen(ORANGE);
  delay(200);
  M5.Lcd.fillScreen(RED);
  delay(200);
  M5.Lcd.fillScreen(BLACK);
  delay(200);

  // Buttonクラスを利用するときには必ずUpdateを呼んで状態を更新する
  M5.update();

  // ホームボタンを押しながら電源ON
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.setTextFont(D_FONT_26PXASC);
  if ( M5.BtnA.isPressed()) {
    intBatteryMode = true;
    M5.Lcd.print("Manual Power Mode");
    delay(2000);
  } else {
    intBatteryMode = false;
    //M5.Lcd.print("Auto Power Mode");
  }

  // ホームボタンを離したか？（1度だけ取得可能）
  if ( M5.BtnA.wasReleased() ) {
  }

  // 変化した時
  if (digitalRead(C_PIN_NEUTRAL) == LOW) {
    intNeutral = true;
    intFgcolor = BLACK;
    intBgcolor = GREEN;
  } else {
    intNeutral = false;
    intFgcolor = WHITE;
    intBgcolor = BLACK;
  }

  // ノイズ対策用　最小パルスの算出
  intMinPulse = (int)(1.0/((((double)D_MAX_SPEED*1000.0)/3600.0)/C_PULSE_DIST)*1000.0*1000.0);

  printGuidance();                         // ガイダンスの表示

  // 割り込み処理開始
  attachInterrupt(C_PIN_INPUT,   func_int1, FALLING);
  attachInterrupt(C_PIN_NEUTRAL, func_int2, CHANGE);
}

/*
 * メインループ
 */
void loop() {

  static unsigned long tim = 0;           // EEPROMへの書き込みタイミング
  static long pre_counter = 0;            // 前回のパルスカウンタ値
  static int  loop_cnt = 0;               // ループ回数
  static int  flg_reject = false;         // trueの時ボタンを無視
  static int  prev_spd;                   // 前回の速度
  double pulse_dist = C_PULSE_DIST;       // 1パルスで進む距離m

  // Buttonクラスを利用するときには必ずUpdateを呼んで状態を更新する
  M5.update();
  
  // 設定モードのチェック
  if ( M5.BtnB.wasReleased() ) {
    cnfBrightness();                      // 輝度設定
    printGuidance();
    lastVolTime = millis();               // 最終電圧確認時間更新
  }

  // 表示処理
  intSpeed = calcSpeed(lngPeriod);

  // 前回（100ms前）との速度差が異常値であれば無視する
  if (prev_spd == 0 ) {
    prev_spd = intSpeed;
  } else if (abs(intSpeed-prev_spd) >= 10) {
    prev_spd = intSpeed;
    return; 
  } else {
    prev_spd = intSpeed;
  }

  if (intSpeed > intFullScale)            // 最高速度更新 
    intFullScale = intSpeed;

  // 背景色の設定
  int bg, fg;
  if (intSpeed < 60) {
    fg = WHITE;
    bg = BLACK;
  } else if (intSpeed < 80) {             // 速度警告（ネイビー）
    fg = WHITE;
    bg = NAVY;      
  } else if (intSpeed < 100) {            // 速度警告（ブルー）
    fg = WHITE;
    bg = BLUE;      
  } else if (intSpeed >= 100) {           // 速度警告（レッド）
    fg = WHITE;
    bg = RED;      
  }

  if (intNeutral) {                       // ニュートラル優先
    fg = BLACK;
    bg = GREEN;
  }    

  // 色が変わったらガイダンスを再表示
  if (bg != intBgcolor) {
    intFgcolor = fg;
    intBgcolor = bg;
    printGuidance();
  }

  // 速度表示
  switch (intMode) {
    case  D_MODE_ODD:
      printSpeed();                       // 速度を表示 
      printOdd();                         // ODDメーターを表示      
      break;
    case  D_MODE_TRIP1: 
    case  D_MODE_TRIP2:
    case  D_MODE_CDOWN:
      printSpeed();                       // 速度を表示 
      printTrip();                        // トリップを表示
      break;
    case  D_MODE_MAXSPD:
      printSpeed();                       // 速度を表示 
      printFullScale();                   // 最高速度を表示
      break;
    case  D_MODE_BIGSPD:
      printBigSpeed();                    // デカ文字速度
      break;
    default:
      break;
  }    

  // ホームボタンを離したか？（1度だけ取得可能）
  if ( M5.BtnA.wasReleased() ) {
    lastVolTime = millis();               // 最終電圧確認時間更新
    if (!flg_reject) {                    // ボタン長押し後のリリースは無視
      intMode += 1;
      if (intMode == D_MODE_END) {
        intMode = D_MODE_ODD;
      }
      printGuidance();
    }
    flg_reject = false;
  }

  // データクリア　（ホームボタン長押し）
  if ( M5.BtnA.pressedFor(3000) ) {
    lastVolTime = millis();               // 最終電圧確認時間更新
    switch (intMode) {
      case  D_MODE_ODD:
        lngOdd = 0;                       // ODDクリア
        break;
      case  D_MODE_TRIP1: 
        intTrip1 = 0;                     // Trip1クリア                
        break;
      case  D_MODE_TRIP2:
        intTrip2 = 0;                     // Trip2クリア
        break;
      case  D_MODE_CDOWN:
        intCdown = D_DEF_CDOWN;           // カウンドダウントリップ初期値
        break;
      case  D_MODE_MAXSPD:
        intFullScale = 0;                 // 最高速度クリア
        break;
      default:
        break;
    }
    writeEEPROM();

    flg_reject = true;                    // 次回のボタンを離すイベントを無視するため
  }
    
  // 前輪停止時にカウンタをリセット
  if (pre_counter == lngPulseCnt) {       // 前回のカウンタと同じなら（動きがないなら）
    loop_cnt += 1;  
  } else {
    loop_cnt = 0;
    pre_counter = lngPulseCnt;
  }
  if (loop_cnt == 10) {                   // 1000ms
    lngPeriod = 0;                        // 周期をゼロにする（停止）
  }
  
  // EEPROMへの書き込み処理
  if (intRequestSave) {
    intRequestSave = false;
    writeEEPROM();
  }

#ifdef debug
#else
  // 外部電源がオフになれば電源を切る
  if ((M5.Axp.GetVinVoltage() < 3.0) && (intBatteryMode == false)) {
    if(lastVolTime + 10000 < millis()){   // 3.0V以下が10秒以上で電源オフ
      writeEEPROM();                      // データ保存
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setCursor(0, 0);
      M5.Lcd.print("Auto Power Off");
      delay(1000);
      M5.Axp.PowerOff();
    }
  } else {
    lastVolTime = millis();               // 最終電圧確認時間更新
  }
#endif

  delay(100);

}

/*
 * 輝度設定画面
 */
void cnfBrightness() {

  char str[4];

  // ガイダンス表示
  tftCfg.createSprite(240, 135);          // ガイダンス
  tftCfg.fillScreen(BLACK);               // バックカラー
  tftCfg.setTextColor(WHITE, BLACK);      // 文字色・背景色 

  // fillscreenのバグ対策
  tftCfg.setTextFont(D_FONT_26PXNUM);     // バグ対策 完全にfillできないため
  tftCfg.setCursor(0, 0);                 // 26ピクセルASCIIフォントで埋める
  for (int ii=0; ii <= 60; ii++) {
    tftCfg.print(" ");
  }  

  tftCfg.setTextSize(1);                  // 倍率
  tftCfg.setTextFont(D_FONT_26PXASC);     // 26ピクセルASCIIフォント

  tftCfg.setCursor(4, 4);
  tftCfg.print("Brightness (1-6)");

  // Bボタンが押されるまでループ
  while(true) {

    sprintf(str, "%3d", intBright-6);

    int x = 48;
    for (int ii=0; ii<3; ii++) {
      tftCfg.setCursor(x, 32);
      if (str[ii] != ' ') {               // 7セグフォントはスペースが表示できない
        tftCfg.setTextFont(D_FONT_48PX7SG); // 48pix 7セグ風フォント
        tftCfg.setTextSize(2);            // 倍率
        tftCfg.print(str[ii]);
      } else {
        tftCfg.setTextFont(D_FONT_16PXASC); // 16pix ASCIIフォント
        tftCfg.setTextSize(6);            // 倍率
        tftCfg.print("  ");
      }
      x += 64;                            // フォントの幅
    }
    tftCfg.pushSprite(0, 0);              // 画面を描画する

    // 設定モードのチェック
    M5.update();    
    if (M5.BtnB.wasReleased()) {
      break;
    } if (M5.BtnA.wasReleased()) {
      intBright += 1;
      if (intBright > 12)
        intBright = 7;
    }
    M5.Axp.ScreenBreath(intBright);       // 画面の明るさ 7-12
    tftCfg.pushSprite(0, 0);              // ガイダンスを描画する
    delay(50);
  }
  
  // 保存
  writeEEPROM();                          // 設定を保存

}


/*
 * 速度計算
 * period = 1周期μsec
 */
int calcSpeed(long period) {
  int ret = 0;
  double pulse_dist = C_PULSE_DIST;
  if (period > 0) {
    // 時速 距離km/時間h
    // pulse_dist/1,000 /period * 1,000,000 * 3,600
    // pulse_dist /  period * 3,600,000
    ret = (int)(pulse_dist / (double)period * 3600000);
  }  
  return ret;
}


/*
 * ガイダンスの表示
 */
void printGuidance() {

  // ガイダンス表示
  tftSp1.createSprite(240, 135);          // ガイダンス
  tftSp1.fillScreen(intBgcolor);          // バックカラー
  tftSp1.setTextColor(intFgcolor, intBgcolor);  // 文字色・背景色 

  // fillscreenのバグ対策
  tftSp1.setTextFont(D_FONT_26PXNUM);     // バグ対策 完全にfillできないため
  tftSp1.setCursor(0, 0);                 // 26ピクセルASCIIフォントで埋める
  for (int ii=0; ii <= 60; ii++) {
    tftSp1.print(" ");
  }  

  tftSp1.setTextSize(1);                  // 倍率
  tftSp1.setTextFont(D_FONT_26PXASC);     // 26ピクセルASCIIフォント

  // デカ文字以外は２段表示
  if (intMode != D_MODE_BIGSPD) {
    tftSp1.setCursor(4, 4);
    tftSp1.print("Speed");
    tftSp1.setCursor(180, 30);
    tftSp1.print("km/h");
    tftSp1.setCursor(11, 53);

    // 下段のタイトル
    if (intMode == D_MODE_ODD) {
      tftSp1.print("ODD");
    } else if (intMode == D_MODE_TRIP1) {
      tftSp1.print("Trip-1");
    } else if (intMode == D_MODE_TRIP2) {
      tftSp1.print("Trip-2");
    } else if (intMode == D_MODE_CDOWN) {
      tftSp1.print("Count down");
    } else if (intMode == D_MODE_MAXSPD) {
      tftSp1.print("Max");
    }
  
    // 最高速度表示の時
    if (intMode == D_MODE_MAXSPD) {
      tftSp1.setCursor(180, 105);
      tftSp1.print("km/h");
    } else {                                // それ以外
      if (intMode == D_MODE_ODD) {          // ODDメータ
        tftSp1.setCursor(205, 105);
      } else {                              // それ以外
        tftSp1.setCursor(190, 105);
      }
      tftSp1.print("km");
    }
  } else {
    tftSp1.setCursor(4, 4);
    tftSp1.print("km/h");
    
  }
  tftSp1.pushSprite(0, 0);                // ガイダンスを描画する

}

/*
 * 速度の表示
 */
void printSpeed() {

  char str[4];
  static int spd = -1;                    // 初期表示のために-1
  
  tftSp2.createSprite(96, 48);

  // 速度表示
  tftSp2.fillScreen(intBgcolor);          // バックカラー
  tftSp2.setTextColor(intFgcolor, intBgcolor);  // 文字色・背景色
  sprintf(str, "%3d", intSpeed);

  int x = 0;
  for (int ii=0; ii<3; ii++) {
    tftSp2.setCursor(x, 0);
    if (str[ii] != ' ') {                 // 7セグフォントはスペースが表示できない
      tftSp2.setTextFont(D_FONT_48PX7SG); // 48pix 7セグ風フォント
      tftSp2.setTextSize(1);              // 倍率
      tftSp2.print(str[ii]);
    } else {
      tftSp2.setTextFont(D_FONT_16PXASC); // 16pix ASCIIフォント
      tftSp2.setTextSize(3);              // 倍率
      tftSp2.print("  ");
    }
    x += 32;                              // フォントの幅
  }
  tftSp2.pushSprite(84, 4);
}

/*
 * ODDの表示
 */
void printOdd() {

  char buf[7];
  long odd = lngOdd;

  tftSp4.createSprite(192,  48);          // ODD表示(数字）
  
  // トリップ表示
  tftSp4.fillScreen(intBgcolor);          // バックカラー
  tftSp4.setTextColor(intFgcolor, intBgcolor);  // 文字色・背景色

  // 整数部の表示
  sprintf(buf, "%06d", long(odd/10));
  tftSp4.setTextFont(D_FONT_48PX7SG);     // 48pix 7セグ風フォント
  tftSp4.setTextSize(1);                  // 倍率
  tftSp4.setCursor(0, 0);
  tftSp4.print(buf);
  tftSp4.pushSprite(8, 80);               // ODDを描画する

 }

/*
 * トリップの表示
 */
void printTrip() {

  char buf[7];
  static int trip = -1;                   // 初期表示のために-1

  if (intMode == D_MODE_TRIP1) {          // TRIP1の時
    trip = intTrip1;                      // 値を更新
  } else if (intMode == D_MODE_TRIP2) {   // TRIP2の時
    trip = intTrip2;
  } else if (intMode == D_MODE_CDOWN) {   // カウンドダウントリップの時
    trip = intCdown;
  }

  tftSp3.createSprite(174,  48);          // トリップ表示(数字）
  
  // トリップ表示
  tftSp3.fillScreen(intBgcolor);          // バックカラー
  tftSp3.setTextColor(intFgcolor, intBgcolor);  // 文字色・背景色
  //dtostrf((float)intTrip1/10,5,1,buf);  // 1234.5

  // 整数部の表示
  sprintf(buf, "%04d", int(trip/10));
  tftSp3.setTextFont(D_FONT_48PX7SG);     // 48pix 7セグ風フォント
  tftSp3.setTextSize(1);                  // 倍率
  tftSp3.setCursor(0, 0);
  tftSp3.print(buf);

  // 小数点の表示
  sprintf(buf, "%05d", trip);

  tftSp3.setTextFont(D_FONT_16PXASC);     // 16pix ASCIIフォント
  tftSp3.setTextSize(3);                  // 倍率
  tftSp3.setCursor(32*4+4, 0);
  tftSp3.print(" ");
  tftSp3.setCursor(32*4+4, 8);
  tftSp3.print(".");
  tftSp3.setTextFont(D_FONT_48PX7SG);     // 48pix 7セグ風フォント
  tftSp3.setTextSize(1);                  // 倍率
  tftSp3.setCursor(32*4+14, 0);
  tftSp3.print(buf[4]);                   // 小数部

  tftSp3.pushSprite(8, 80);               // トリップを描画する

 }

/*
 * 最高速度の表示
 */
void printFullScale() {

  char str[4];
  static int spd = -1;                    // 初期表示のために-1
  
  tftSp5.createSprite(96, 80);

  // 速度表示
  tftSp5.fillScreen(intBgcolor);          // バックカラー
  tftSp5.setTextColor(intFgcolor, intBgcolor);  // 文字色・背景色
  sprintf(str, "%3d", intFullScale);

  int x = 0;
  for (int ii=0; ii<3; ii++) {
    tftSp5.setCursor(x, 0);
    if (str[ii] != ' ') {                 // 7セグフォントはスペースが表示できない
      tftSp5.setTextFont(D_FONT_48PX7SG); // 48pix 7セグ風フォント
      tftSp5.setTextSize(1);              // 倍率
      tftSp5.print(str[ii]);
    } else {
      tftSp5.setTextFont(D_FONT_16PXASC); // 16pix ASCIIフォント
      tftSp5.setTextSize(3);              // 倍率
      tftSp5.print("  ");
    }
    x += 32;                              // フォントの幅
  }
  tftSp5.pushSprite(84, 80);              // 最高速度を表示する
}

/*
 * 速度の表示
 */
void printBigSpeed() {

  char str[4];
  static int spd = -1;                    // 初期表示のために-1
  
  tftSp6.createSprite(192, 96);

  // 速度表示
  tftSp6.fillScreen(intBgcolor);          // バックカラー
  tftSp6.setTextColor(intFgcolor, intBgcolor);  // 文字色・背景色
  sprintf(str, "%3d", intSpeed);

  int x = 0;
  for (int ii=0; ii<3; ii++) {
    tftSp6.setCursor(x, 0);
    if (str[ii] != ' ') {                 // 7セグフォントはスペースが表示できない
      tftSp6.setTextFont(D_FONT_48PX7SG); // 48pix 7セグ風フォント
      tftSp6.setTextSize(2);              // 倍率
      tftSp6.print(str[ii]);
    } else {
      tftSp6.setTextFont(D_FONT_16PXASC); // 16pix ASCIIフォント
      tftSp6.setTextSize(6);              // 倍率
      tftSp6.print("  ");
    }
    x += 64;                              // フォントの幅
  }
  tftSp6.pushSprite(48, 32);
}

/*
 * EEPROMの読み出し
 */
void readEEPROM() {

  int key = EEPROM.readInt(C_ADDR_CHECK);
  if (key != C_ADDR_KEYNO) {
    EEPROM.writeInt(C_ADDR_CHECK, C_ADDR_KEYNO);  // KEYNO書き込み
    EEPROM.commit();
    lngOdd    = 0;
    intTrip1  = 0;
    intTrip2  = 0;
    intCdown  = D_DEF_CDOWN;
    intFullScale = 0;
    intBright = 9;
    writeEEPROM();
    return;  
  }

  lngOdd    = EEPROM.readLong(C_ADDR_ODD);
  intTrip1  = EEPROM.readInt(C_ADDR_TRIP1);
  intTrip2  = EEPROM.readInt(C_ADDR_TRIP2);
  intCdown  = EEPROM.readInt(C_ADDR_CDOWN);
  intFullScale = EEPROM.readInt(C_ADDR_FSCALE);
  intBright = EEPROM.readInt(C_ADDR_BRIGHT);
  intMode   = EEPROM.readInt(C_ADDR_MODE);

  Serial.print("Odd=");
  Serial.print(lngOdd);
  Serial.print(",Trip1=");
  Serial.print(intTrip1);
  Serial.print(",Trip2=");
  Serial.print(intTrip2);
  Serial.print(",CountDown=");
  Serial.print(intCdown);
  Serial.print(",MaxSpeed=");
  Serial.print(intFullScale);
  Serial.print(",Bright=");
  Serial.print(intBright);
  Serial.print(",Mode=");
  Serial.print(intMode);
  Serial.println("");

}

/*
 * EEPROMの書き込み
 */
void writeEEPROM() {

  digitalWrite(C_PIN_LED, LOW);           // LED点灯
  detachInterrupt(C_PIN_INPUT);           // 割り込みを止めないと落ちる
  EEPROM.writeLong(C_ADDR_ODD,   lngOdd);
  EEPROM.writeInt(C_ADDR_TRIP1,  intTrip1);
  EEPROM.writeInt(C_ADDR_TRIP2,  intTrip2);
  EEPROM.writeInt(C_ADDR_CDOWN,  intCdown);
  EEPROM.writeInt(C_ADDR_FSCALE, intFullScale);
  EEPROM.writeInt(C_ADDR_BRIGHT, intBright);
  EEPROM.writeInt(C_ADDR_MODE,   intMode);
  EEPROM.commit(); 
  attachInterrupt(C_PIN_INPUT, func_int1, FALLING); // 再開
  digitalWrite(C_PIN_LED, HIGH);          // LED消灯
}


/*
 * 前輪の回転パルスでの割り込み
 */
void func_int1() {
  
  static long t_bef = 0;                  // 前回の時間
  long t_now = micros();                  // 今回の時間（電源ONからの経過）
  static long cnt = 0;                    // 前回のカウンタ値
  noInterrupts();                         // 割り込み禁止
  
  // 立ち下りの時
  if (digitalRead(C_PIN_INPUT) == LOW) {

    // 外れ値の除外
    long diff = (t_now - t_bef);          // μsec
    if (diff <= intMinPulse){             // 前回との時間差μsが最小値以下なら無視
      interrupts();                       // 割り込み許可
      return;
    }

    lngPulseCnt += 1;                     // パルスカウンタインクリメント

    // TRIPの更新
    long m100 = (long)(100.0/C_PULSE_DIST);  // 100Mごと m100 = 100 Pulse
    if ((lngPulseCnt - cnt) > m100) {
      if (cnt != 0) {
        lngOdd   += 1;
        intTrip1 += 1;
        if (intTrip1 > D_MAX_TRIP) {
          intTrip1 = 0;
        }
        intTrip2 += 1;
        if (intTrip2 > D_MAX_TRIP) {
          intTrip2 = 0;
        }
        if (intCdown > 0) {
          intCdown -= 1;
        }

        int x = lngOdd % 10;
        if (x == 0) {                     // 1kmごとに保存要求
          intRequestSave = true;
        }
      }
      cnt = lngPulseCnt;
    }
    
    if (t_bef == 0) {                     // 初回
      t_bef = t_now;                      // 前回に今回の値をセット
    } else if (t_bef > t_now) {           // 70分に１回のオーバーフロー
      t_bef = t_now;                      // 前回に今回の値をセット
    } else {
      lngPeriod = t_now - t_bef;          // 前回との時間差μs
      t_bef = t_now;                      // 前回値の更新
    }
    
  }
  interrupts();                           // 割り込み許可
}

/*
 * ニュートラル時
 */
 void func_int2() {
  
  noInterrupts();                         // 割り込み禁止
  
  // 変化した時
  if (digitalRead(C_PIN_NEUTRAL) == LOW) {
    intNeutral = true;
  } else {
    intNeutral = false;
  }
  interrupts();                           // 割り込み許可
}
