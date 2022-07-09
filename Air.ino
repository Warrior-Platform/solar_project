#include <Wire.h>
#include<SoftPWM.h>

/*CLCD 사용을 위한 라이브러리 추가*/
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2); //또는 LiquidCrystal_I2C lcd(0x3F, 16, 2);

#define FAN_POWER A2   //팬 전원 핀
#define FAN_PWM A3     //팬 속도 조절 핀
#define RED 7            //팬의 Red핀(D9)
#define BLUE 9           //팬의 Blue핀(D7)
#define GREEN 8         //팬의 Green핀(D6)

SOFTPWM_DEFINE_CHANNEL(FAN_PWM); //SoftPWM으로 사용할 핀 설정

/*IoT 사용을 위한 라이브러리 추가*/
#include <VitconBrokerComm.h>
using namespace vitcon;

float pm10_grimm_ave; //미세먼지 평균값
uint32_t timer = 0; //시간 저장 변수
int count = 0; //미세먼지 측정 카운트
uint16_t pm10_grimm = 0; //미세먼지값 저장 변수
int prestate = -1; //먼지 레벨의 이전 레벨
int state = -1; //현재 먼지 레벨

/* A set of definition for IOT items */
#define ITEM_COUNT 5

IOTItemFlo WLdustQuan;
IOTItemBin WLED_level1;
IOTItemBin WLED_level2;
IOTItemBin WLED_level3;
IOTItemBin WLED_level4;
IOTItem *items[ITEM_COUNT] = { &WLdustQuan, &WLED_level1, &WLED_level2, &WLED_level3, &WLED_level4};


/* IOT server communication manager */
const char device_id[] = "df65372c3f3c9c6bd35ab1f244eccea1"; // Change device_id to yours
BrokerComm comm(&Serial, device_id, items, ITEM_COUNT);

/*********************웃는 얼굴**********************/
byte smile_eye[8] = { //웃는 얼굴 눈
  B00000,
  B00000,
  B01110,
  B11011,
  B10001,
  B10001,
  B10001,
  B00000
};
byte smile_mouth_L[8] = {//웃는 얼굴의 왼쪽 입
  B00111,
  B00100,
  B00100,
  B00010,
  B00001,
  B00000,
  B00000,
  B00000
};
byte smile_mouth_M[8] = {//웃는 얼굴의 가운데 입
  B11111,
  B00000,
  B00000,
  B00000,
  B11111,
  B00000,
  B00000,
  B00000
};
byte smile_mouth_R[8] = {//웃는 얼굴의 오른쪽 입
  B11100,
  B00100,
  B00100,
  B01000,
  B10000,
  B00000,
  B00000,
  B00000
};

/*******************살짝 웃는 얼굴*******************/
byte soso_eye[8] = {//살짝 웃는 얼굴의 눈
  B00000,
  B00000,
  B00000,
  B01110,
  B10001,
  B00000,
  B00000,
  B00000
};
byte soso_mouth_L[8] = {//살짝 웃는 얼굴의 왼쪽 입
  B00000,
  B00010,
  B00001,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000
};
byte soso_mouth_M[8] = {//살짝 웃는 얼굴의 가운데 입
  B00000,
  B00000,
  B00000,
  B11111,
  B00000,
  B00000,
  B00000,
  B00000
};
byte soso_mouth_R[8] = {//살짝 웃는 얼굴의 오른쪽 입
  B00000,
  B01000,
  B10000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000
};

/*********************정색 얼굴********************/
byte straight_eye[8] = {//정색 얼굴의 눈
  B00000,
  B00000,
  B00000,
  B11111,
  B00000,
  B00000,
  B00000,
  B00000
};

/********************우는 얼굴********************/
byte crying_eye[8] = {// 우는 얼굴의 눈
  B00000,
  B00000,
  B00000,
  B11111,
  B00100,
  B00100,
  B00100,
  B00000
};
byte crying_mouth[8] = {//우는 얼굴의 입
  B01110,
  B10001,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000
};

void setup() {
  Wire.begin();
  Serial.begin(250000);
  pinMode(A0, OUTPUT);
  digitalWrite(A0, LOW);

  pinMode(FAN_POWER, OUTPUT); //D4를 출력 핀으로 설정
  pinMode(FAN_PWM, OUTPUT);   //D5를 출력 핀으로 설정
  pinMode(RED, OUTPUT);  //D9를 출력 핀으로 설정
  pinMode(BLUE, OUTPUT); //D7를 출력 핀으로 설정
  pinMode(GREEN, OUTPUT);//D6를 출력 핀으로 설정

  comm.SetInterval(200);

  digitalWrite(FAN_POWER, HIGH);//FAN 전원 ON으로 고정
  SoftPWM.begin(490); //PWM frequency설정
  SoftPWM.set(100); //초기 FAN 속도 0

  //초기 RGB 초록색 출력
  analogWrite(RED, 0);
  analogWrite(GREEN, 255);
  analogWrite(BLUE, 0);

  //Send Command Data (Set up continuously measurement(default mode))
  uint8_t cmd_buf[7] = {0x16, 0x7, 0x3, 0xff, 0xff, 0, 0};

  //Check code
  cmd_buf[6] = cmd_buf[0];
  for (int i = 1; i < 6; i++)
  {
    cmd_buf[6] ^= cmd_buf[i];
  }

  //0x28 address slave device 와의 통신 시작
  Wire.beginTransmission(0x28);
  Wire.write(cmd_buf, 7);
  Wire.endTransmission();

  lcd.init();//CLCD 초기화
  lcd.backlight(); //백라이트 ON (밝기는 CLCD 뒤쪽의 가변저항으로 조절합니다.)

  /*사용자 정의문자를 메모리에 지정, 최대 8개까지 정의 가능*/
  lcd.createChar(1, crying_eye);    //우는 얼굴의 눈
  lcd.createChar(2, crying_mouth); //우는 얼굴의 입
  lcd.createChar(0, straight_eye);  //정색 얼굴의 눈

  //LCD 초기화 및 "DUST"를 (0.0)좌표에 출력, "ug/m3"를(5,1)좌표에 출력
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("DUST");
  lcd.setCursor(6, 1);
  lcd.print("ug/m3");

  timer = millis();  //get the current time;
}

void loop() {
  if (millis() > timer + 1000) //1초 마다 실행
  {
    //LED위젯 초기화
    WLED_level1.Set(false);
    WLED_level2.Set(false);
    WLED_level3.Set(false);
    WLED_level4.Set(false);

    pm10_grimm = pm10_grimm + readPM2008M();
    count++;

    if (count > 4)
    {
      pm10_grimm_ave = (float)pm10_grimm / 5.0;
      count = 0; //카운트값 초기화
      pm10_grimm = 0; //미세먼지 측정값 초기화
    }

    //clcd 출력
    lcd.setCursor(0, 1);
    lcd.print("      ");

    lcd.setCursor(0, 1);
    lcd.print(pm10_grimm_ave);

    //pm10_grimm 범위에 따라 state설정
    if (pm10_grimm_ave <= 30) {
      state = 0;
      WLED_level1.Set(true);
    }
    else if (pm10_grimm_ave > 30 && pm10_grimm_ave <= 80) {
      state = 1;
      WLED_level2.Set(true);
    }
    else if (pm10_grimm_ave > 81 && pm10_grimm_ave <= 150) {
      state = 2;
      WLED_level3.Set(true);
    }
    else if (pm10_grimm_ave > 150) {
      state = 3;
      WLED_level4.Set(true);
    }

    timer = millis();
  }

  if (state != prestate) //이전 상태와 다를 때만 실행한다.
  {

    if (state == 0) {
      SoftPWM.set(60); //slow speed(duty 60%)

      // 초록색 출력
      analogWrite(RED, 0);
      analogWrite(GREEN, 255);
      analogWrite(BLUE, 0);

      //이모티콘1 출력
      face_init();
      smile_face();
    }

    else if (state == 1) {//middle speed1
      SoftPWM.set(40); //middle speed1(duty 40%)

      //파란색 출력
      analogWrite(RED, 0);
      analogWrite(GREEN, 0);
      analogWrite(BLUE, 255);

      //이모티콘2 출력
      face_init();
      soso_face();
    }

    else if (state == 2) {//middle speed2
      SoftPWM.set(40); //middle speed1(duty 40%)

      //주황색 출력
      analogWrite(RED, 255);
      analogWrite(GREEN, 60);
      analogWrite(BLUE, 0);

      //이모티콘3 출력
      face_init();
      straight_face();
    }

    else if (state == 3) {//fast speed
      SoftPWM.set(0); //fast speed(duty 0%)

      //빨강색 출력
      analogWrite(RED, 255);
      analogWrite(GREEN, 0);
      analogWrite(BLUE, 0);

      //이모티콘4 출력
      face_init();
      crying_face();
    }

    prestate = state;
  }

  //서버와 통신
  WLdustQuan.Set(pm10_grimm_ave);
  comm.Run();
}

uint16_t readPM2008M()
{
  uint8_t buf[32];
  uint16_t pm10_grimm = 0;
  uint8_t idx = 0;

  //마스터가 특정 slave 장치의 입력한 수량 만큼의 바이트를 요청
  Wire.requestFrom(0x28, 32);
  while (Wire.available())
  {
    buf[idx++] = (uint8_t)Wire.read();
    if (idx == 32)break;
  }

  //buf[0] : frame header
  //buf[1] : frame length
  if (buf[0] != 0x16 || buf[1] != 32) //
  {
    return 0; //read fail
  }

  pm10_grimm = (buf[11] << 8) + buf[10]; //미세먼지
  return pm10_grimm;
}

void face_init() { //이모티콘 초기화 함수
  lcd.setCursor(12, 0);
  lcd.print("   ");
  lcd.setCursor(12, 1);
  lcd.print("   ");
}

void smile_face() { //이모티콘1 함수
  lcd.createChar(4, smile_eye);
  lcd.createChar(5, smile_mouth_L);
  lcd.createChar(6, smile_mouth_M);
  lcd.createChar(7, smile_mouth_R);
  delay(10);

  lcd.setCursor(12, 0);
  lcd.write(4);
  lcd.setCursor(14, 0);
  lcd.write(4);
  lcd.setCursor(12, 1);
  lcd.write(5);
  lcd.setCursor(13, 1);
  lcd.write(6);
  lcd.setCursor(14, 1);
  lcd.write(7);
}

void soso_face() { //이모티콘2 함수
  lcd.createChar(4, soso_eye);
  lcd.createChar(5, soso_mouth_L);
  lcd.createChar(6, soso_mouth_M);
  lcd.createChar(7, soso_mouth_R);
  delay(10);

  lcd.setCursor(12, 0);
  lcd.write(4);
  lcd.setCursor(14, 0);
  lcd.write(4);
  lcd.setCursor(12, 1);
  lcd.write(5);
  lcd.setCursor(13, 1);
  lcd.write(6);
  lcd.setCursor(14, 1);
  lcd.write(7);
}
void straight_face() { //이모티콘3 함수
  lcd.setCursor(12, 0);
  lcd.write(0);
  lcd.setCursor(14, 0);
  lcd.write(0);
  lcd.setCursor(13, 1);
  lcd.write(2);
}

void crying_face() { //이모티콘4 함수
  lcd.setCursor(12, 0);
  lcd.write(1);
  lcd.setCursor(14, 0);
  lcd.write(1);
  lcd.setCursor(13, 1);
  lcd.write(2);
}
