/*
 #include <Servo.h>

#define   PIN_BASE    9
#define   PIN_HEAD    10
#define   ANGLE_BASEPOINT   90

Servo base,head;

void setup() {
  base.attach(PIN_BASE);
  head.attach(PIN_HEAD);
}

void loop() {
  base.write(ANGLE_BASEPOINT);
  head.write(ANGLE_BASEPOINT);
  delay(50);
  
}
*/

/*
 * 제품명 : 태양 추적기
 * 제작 : EL-Science
 * 
 * [사용하는 전자부품]
 *  - 서보모터 ( SG-90 )
 *  - 적외선 센서 ( 발광부 / 수광부 포함 )
 *  - 아두이노 나노
 *  - 아두이노 나노 쉴드
 *  - CDS
 *  
 * [서보를 조립하시기 전에...]
 *  - 본 제품에 사용된 서보모터는 좌측 90도, 우측 90도 회전하도록 설계된 코드가 쓰입니다.
 *    따라서 서보모터의 축을돌려 기준을 잡아주는 작업을 우선 실시하신 후 조립을 실시하셔야합니다.
 *    서보모터의 기준을 잡아주는 방법은 set_base.ino 소스코드를 업로드하시면 되오며, 
 *    서보모터가 연결된 상태에서 
 *    
 * [ 사용되는 소스코드 ]
 *  - SunDetector.ino : 태양 추적 소스코드
 *  - set_base.ino : 사용되는 서보모터의 기준을 초기화시키는 소스코드 ( 업로드 하신 후 조립을 실시하세요. )
 */
#include <Servo.h>
#include <Wire.h>
#define     uchar               unsigned char
#define     ushort              unsigned short
#define     SENSING_LEN         10
#define     SENSING_HISTERESIS  100
#define     CALC_AVG(target)    ((float)target/SENSING_LEN)
#define     MAX_ULONG            4294967295UL

enum TYPE_APORT{PORT_TR=A0,PORT_BR=A1,PORT_TL=A2,PORT_BL=A3};
enum TYPE_BASE{LEFT=1,RIGHT=2};
enum TYPE_HEAD{TOP=1,BOTTOM=2};

const TYPE_APORT aportTable[] = {PORT_TR,PORT_BR,PORT_TL,PORT_BL};

typedef struct
{
  short  data[SENSING_LEN];
  long  total_value;
  TYPE_BASE base;
  TYPE_HEAD head;
} SENSOR;

SENSOR cds_value[4];
uchar index_sensing;

Servo base,head;

unsigned long old_time;

void setup() {
  pinMode(PORT_TR,INPUT);
  pinMode(PORT_BR,INPUT);
  pinMode(PORT_TL,INPUT);
  pinMode(PORT_BL,INPUT);

  cds_value[0].base = cds_value[1].base = TYPE_BASE::RIGHT;
  cds_value[2].base = cds_value[3].base = TYPE_BASE::LEFT;
  
  cds_value[0].head = cds_value[2].head = TYPE_HEAD::TOP;
  cds_value[1].head = cds_value[3].head = TYPE_HEAD::BOTTOM;
  
  Serial.begin(9600);

  base.attach(9);
  head.attach(10);

  base.write(90);
  head.write(90);
  delay(500);

  Wire.begin();
  Wire.setClock(400000UL); // 400KHz.
  
  old_time = millis();
}

void loop() { 
  unsigned long int totalSensorValue = 0;
  
  //센싱.
  for(char i=0;i<4;i++)
  {
    cds_value[i].total_value = cds_value[i].total_value - cds_value[i].data[index_sensing];
    cds_value[i].data[index_sensing] = analogRead(aportTable[i]);
    cds_value[i].total_value = cds_value[i].total_value + cds_value[i].data[index_sensing];
    totalSensorValue += cds_value[i].total_value;
  }
  index_sensing = (index_sensing + 1) % SENSING_LEN;


  unsigned long cur_time = millis();

  if((MAX_ULONG-old_time+cur_time) % MAX_ULONG >= 10)
  {
    //1위 2위값 추출
    char index_rank[4];
  
    for(char i=0;i<=2;i+=2)
    {
      if(cds_value[i].total_value > cds_value[i+1].total_value)
      {
        index_rank[i] = i;
        index_rank[i+1] = i+1;
      }
      else
      {
        index_rank[i] = i+1;
        index_rank[i+1] = i;
      }
    }
    
    if(cds_value[index_rank[0]].total_value > cds_value[index_rank[2]].total_value)
    {
      if(cds_value[index_rank[1]].total_value < cds_value[index_rank[2]].total_value)
      {
        index_rank[1] = index_rank[2];
      }
    }
    else
    {
      if(cds_value[index_rank[0]].total_value < cds_value[index_rank[3]].total_value)
      {
        index_rank[1] = index_rank[3];
      }
      else
      {
        index_rank[1] = index_rank[0];
      }
      index_rank[0] = index_rank[2];
    }


    Serial.print("1 : "); Serial.print(index_rank[0],DEC); Serial.print("  2 : "); Serial.println(index_rank[1],DEC);
  
    
    //동작값 설정.
    char dir_base=0;
    char dir_head=0;
    
    if(cds_value[index_rank[0]].base == cds_value[index_rank[1]].base)
    {
      if(cds_value[index_rank[0]].base == TYPE_BASE::RIGHT)
      {
        dir_base = 1;
      }
      else
      {
        dir_base = -1;
      }
    }
    else if(cds_value[index_rank[0]].head == cds_value[index_rank[1]].head)
    {
      if(cds_value[index_rank[0]].head == TYPE_HEAD::TOP)
      {
        dir_head = -1;
      }
      else
      {
        dir_head = 1;
      }
    }
    else
    {
      unsigned long total_A = (cds_value[0].total_value/10.0f) + (cds_value[3].total_value/10.0f);
      unsigned long total_B = (cds_value[1].total_value/10.0f) + (cds_value[2].total_value/10.0f);
  
      if(abs(total_A - total_B) > 100)  // 불안정한 상태
      {
        if(cds_value[index_rank[0]].base == TYPE_BASE::RIGHT) dir_base = 1;
        else dir_base =-1;
  
        if(cds_value[index_rank[0]].head == TYPE_HEAD::TOP) dir_head = -1;
        else dir_base = 1;
      }
    }
  
    //동작.
    int angle_base = base.read() + dir_base;
    int angle_head = head.read() + dir_head;
  
    if(angle_base < 0) angle_base = 0;
    else if(180 < angle_base) angle_base = 180;
  
    if(angle_head < 0) angle_head = 0;
    else if(180 < angle_head) angle_head = 180;
  
    base.write(angle_base);
    head.write(angle_head);

    old_time = millis();
  }
}
