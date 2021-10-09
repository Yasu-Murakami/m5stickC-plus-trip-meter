# m5stickC-plus-trip-meter
M5StickC Plus Trip Meter for Motorcycles
Ver 1.0
Create date : 08-10-2021
Author : Y.Murakami

1.概要
M5StickC Plusを使用したバイク用のトリップメーターです。
トリップメーターがない方、トリップメーターを増設したい方向けです。
車速センサー（ホール素子）のパルス信号から速度と走行距離を算出しM5StickC Plusに表示します。
1km走行ごとまたは外部電源OFF時に走行距離をEEPROMに書き込む。

2.環境
Device : M5SticC Plus
IDE    : Arduino 1.8.15
Library: M5SticCPlus Ver 0.0.2

3.機能
速度/距離表示（Aボタンで切り替え）
(1)ODD        : 1~999999km
(2)Trip1      : 0.0~999.9km     * D_MAX_TRIP  で定義
(3)Trip2      : 0.0~999.9km     * D_MAX_TRIP  で定義
(4)Count down : 999.9~0km       * D_DEF_CDOWN で定義
(5)Max Speed  : 999km/h         * D_MAX_SPEED で定義

4.輝度設定
(1)Bボタンで設定画面
(2)Aボタンで変更
(3)Bボタンで戻る

5.背景色
(1)ニュートラル時  緑
(2)60km/h未満   ブラック
(3)80km/h未満   ネイビー
(4)100km/h未満  ブルー
(5)100km/h以上  レッド

6.配線
(1)5VIN  : 外部電源
(2)G33   : パルス入力　 ＊メーター側でプルアップしているのでプルアップ不要
(3)G32   : ニュートラル
(4)GND   : GND
