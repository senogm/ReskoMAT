/*
 * 06:45:49.086 -> R_Code = 13
 * 06:45:49.086 -> +CMTI: "SM",6
 * Работа этой функции регулируется при помощи простой AT-
  команды:
  AT+DDET=1 // Включить функцию DTMF декодирования
  OK
  ATDXXXXXXXXXXX; // Установить голосовое соединение
  OK
  +DTMF:2 // Удаленная сторона жмет на клавиши мобильного телефона
  +DTMF:8
  +DTMF:8
  +DTMF:4
  +DTMF:5
  +DTMF:2
  NO CARRIER // Голосовое соединение завершено
 */
// отладка
//#define DEBUG_ENABLE
#ifdef DEBUG_ENABLE 
#define P_DEBUG(x) Serial.println(x);
#else
#define P_DEBUG(x)
#endif

#define MASTER_PHONE1 "+380503008444"
#define MASTER_PHONE2 "+380939880888"
#define MASTER_PHONE3 "+380661026086"
#define MASTER_PHONE4 "+380504077212"
#define MASTER_PHONE5 "+380000000000"   // НОМЕР ПОД ПРИВАТБАНК 

#include "avr/sleep.h"
#include "avr/eeprom.h"
#include "Servo.h"
#include <SoftwareSerial.h>

#define SIZE_OF_BUFFER 99               // буффер для чтения из модем 
#define TIMEOUT 5000                    // очередной TIME OUT
#define wait_ok 1000                    // сколько времени ждать ОК от модема на AT команду - например 1 секунда, больше - нету ответа
#define wait_dtmf 30000                 // сколько времени ждать DTMF от %USER% - например 30 секунда, больше - нах

// Pin's for software serial for SIM800L (Used interupt 0)
#define pin_rx 2
#define pin_tx 4

// Pin`s for 2 SR501 united logial AND. In test mode used TTP223 sensor
#define nearbird_pin 3

// PWM pin for X servo
#define x0servo_pin   5
#define x1servo_pin   5
#define x2servo_pin   5
#define x3servo_pin   5
#define xmax_angle   180    // максимальный угол поворота Х сервы - зависит от модели
#define xservo_wait  1000   // задержка Х сервы
// PWM pin`s for Z servo
#define z0servo_pin  6
#define z1servo_pin  9
#define z2servo_pin  10
#define z3servo_pin  11
#define zservo_wait  1000   // задержка Z серва
// TTP223 - A0 = D14, A1 = D15, A2 = D16, A3 = D17, A4 = D18, A5 = D19, A6 = D20, A7 = D21
#define onikey1 A7
// #define onikey2 A1

#define period 60000        // 10 секунд крутим щарманку и если все тихо, то спать!

// определим количество типов в Рескомате товаров для ЗеМата и ЛикиМата
#define num_types   1       // количество типо - максимум 4,в Рескомате 1, максимум 2
#define num_clip    4       // количество типо - максимум 4,в Рескомате 1, максимум 2
#define depth_clip 11       // сколько "заряжено" в одину "обойму"

SoftwareSerial SIM800(pin_rx, pin_tx);        // 8 - RX Arduino (TX SIM800L), 9 - TX Arduino (RX SIM800L)

// AT+CWHITELIST={0 - off,1 - call,2 - sms,3 - call&sms},{1-30},tel - не работает пока
// +IPR: (),(0,0b00,2400,4800,9600,19200,38400,57600,115200,230400,460800
// AT+CSCLK=0 - спящий режим выкл, 1 - ? (для выкл + low to DTR) , 2 - используем этот. После RX int - at, 100ms, at+csclk=0
// AT+CBC ответ R_Code = 15;+CBC: 0,98,4186;R_Code = 2;OK - вольтаж 4186 = 4,186в


#define _AT_ F("AT")
#define _AT_DDET_ F("AT+DDET=");    // 1 - включить определение DTMF после снятия трубки, 0 - выключить
#define _AT_INIT_ F("ATE0V1+CMEE=0;+CLIP=1;+CMGF=1;+CSCLK=0;&W"); // echo off, ответы текстовые, ошибки не раскрывать, номер определять, смс - текст, ждущий режим отключить - записать состояние
#define _AT_CSCLK_ F("AT+CSCLK=");  // 0 - обычный режим работы, 2 - ждущий режим
#define _AT_DELSMS_ F("AT+CMGDA=\"DEL ALL\"")

volatile unsigned long sleep_time = 0;
volatile unsigned int type_int = 2;           // = 0 - прерывание по модему, 1 = датчик движения сработал
volatile unsigned int xpos_angle = 30;        // XServa по умолчанию уходит на 30"

volatile boolean TOP, BOTTOM, RESKIOUT = true;  // состояние механизвом true - можно запускать, false = нельзя

// состояние кнопок и результат выдачи рескинола
boolean key1_on, key2_on, rc_RO, sr501 = false;

volatile int stock[num_clip]={depth_clip, depth_clip, depth_clip, depth_clip};

const int xservo_pin[num_clip]={x0servo_pin, x1servo_pin, x2servo_pin, x3servo_pin};
const int zservo_pin[num_clip]={z0servo_pin, z1servo_pin, z2servo_pin, z3servo_pin};

Servo xservo, zservo;

String reply="";

int rcode;

void setup() {
  Serial.begin(9600);               // Скорость обмена данными с компьютером - если нужен для отладки
  SIM800.begin(9600);               // Скорость обмена данными с модемом
  
  boolean init_rc = initResko();
  
  // Датчик наличия движения рядом с Рескоматом 
  pinMode(nearbird_pin, INPUT);
  
  //
  Serial.println("Start!");
  sleep_time = millis() + period;
}

boolean initResko() {
  // модем и переменные с ним связанные присваиваем 
  rcode = 0;
  reply = "";
  // скорось пусть по порту модем наш вырявняет
  SIM800.println(_AT_);
  delay(100);
  
  // вычитаем хлам из модема
  while (SIM800.available()) {rcode = SIM800.read(); delay(1);}
  
  // настроим с начала и правильно 
  reply = _AT_INIT_;
  do {
    rcode=sim800rw(reply);
  } while (!(reply[0] == 'O' && reply[1] == 'K') && rcode > 0);
  if (rcode < 0) {
    sim800error(rcode);
    return false;    
  }
  
  // перед началом работы, грохним все sms-ки
  reply = _AT_DELSMS_;
  do {
    rcode=sim800rw(reply);
  } while (!(reply[0] == 'O' && reply[1] == 'K') && rcode > 0);
  if (rcode < 0) {
    sim800error(rcode);
    return false;    
  }
  
  // теперь разберемся с сервами = первая Х

  for (int i=0;i<=3;i++) {
    pinMode(xservo_pin[i], OUTPUT);
    pinMode(zservo_pin[i], OUTPUT);
    xservo.attach(xservo_pin[i]);
    zservo.attach(zservo_pin[i]);
    xservo.write(0);
    delay(1000);
    zservo.write(0);
    delay(1000);
    xservo.detach();
    zservo.detach();
  }
  
  return true;
}

void sim800error(int &rc) {
  Serial.println("timeout = -1, buffer = -2; After init error RC =" + rc);
  Serial.print(reply);
}

void loop() {
 
  if (SIM800.available()) {
    // появилось что то почитать, разберем - что прислал модем
    reply = "";
    rcode=sim800rw(reply);
    if (rcode > 2) {
      // вот тут надо более глобально разбиратся с событием от модема
      if (reply.startsWith(F("RING"))) {
        // разбираем звоночек
          reply = "";
          // должен идти +CLIP
          rcode=sim800rw(reply);
          if (reply.startsWith(F("+CLIP"))) {
            // нам точно звонят и мы точно получили номер звонящего 
            if (check_phone(reply)) {
              // звонок с MASTER PHONE = рескомат переходит под управление звонящего
              boolean rc = wwDTMF();
            } else {
              // звонок случайный - отобьем
              reply = F("ATH");
              rcode = sim800rw(reply);
            } 
          }
      } else if (reply.startsWith("+CMTI")) {
        // разбираем код смски
          boolean rc = wwSMS(reply);
      } else {
        // тут будем разбирать другие события из жизни Рескомата 
        Serial.print("\r\nR_Code = ");
        Serial.print(rcode);
        Serial.print(";");  
        Serial.println(reply);
        reply = "";
      }
    }    
    sleep_time = millis() + period;
  }

  reply = "";

  while (Serial.available()) {        // Ожидаем команды по Serial...
    //SIM800.write(Serial.read());    // ...и отправляем полученную команду модему
    reply += (char)Serial.read();
    sleep_time = millis() + period;
    delay(1);
  }
  
  if (reply != "") {
    reply.toUpperCase();
    if (reply[0] == 'A' && reply[1]== 'T' ) {
      do {
        rcode=sim800rw(reply);
        Serial.print("\r\nR_Code = ");
        Serial.print(rcode);
        Serial.print(";");  
        Serial.println(reply);
        reply = "";
        if (rcode != 2) reply = "";
      } while (!(reply[0] == 'O' && reply[1] == 'K') && rcode > 0);
    } else reply = "";
  }

  // Проверим, если ли живая душу 
  if (digitalRead(nearbird_pin)) {
    // кто-то или что то живое трется рядом, при этом тянуть время в SR501 не будем - 5 сек и нах 
    sleep_time = millis() + period;
    sr501 = true;
  } else {
    sr501 = false;
  }

  // проверим нажатие кнопки 1
  if (analogRead(onikey1)>200) {
    sleep_time = millis() + period;
    if (!key1_on) {
      // кнопку только нажали
      key1_on = true;
      rc_RO = ReskiOut(4);  // выдадим из первой, самой "забитой" обоймы код = 4
      // а вообще тут мы будем звонить оператору или в 911
    }
  } else key1_on = false;

  
  if (millis()>sleep_time){
    // притушить модем
    // отключить силовые модули
    type_int = 2; // "сбросим тип прерывания
    attachInterrupt(digitalPinToInterrupt(pin_rx), myISRrx, LOW);
    attachInterrupt(digitalPinToInterrupt(nearbird_pin), mySR501, RISING);
    set_sleep_mode(SLEEP_MODE_STANDBY);
    sleep_mode();
    // если WAKEUP из за модема, сначала надо прочитать событие!
    // подключать модем будем, когда будем в него что то слать, а пока просто прочитаем, что он нам прислал
    // подключить силовые модули?
  }
}

void myISRrx() {
  detachInterrupt(digitalPinToInterrupt(pin_rx));
  detachInterrupt(digitalPinToInterrupt(nearbird_pin));
  type_int = 0;
  sleep_time = millis() + period;
}

void mySR501() {
  detachInterrupt(digitalPinToInterrupt(pin_rx));
  detachInterrupt(digitalPinToInterrupt(nearbird_pin));
  type_int = 1;
  sleep_time = millis() + period;
}

int sim800rw(String &rpl) {
  // функция вызывается в двух случаях - надо что-то запихнуть в модем и прочитать, или модем нам уже что то отдает сам (звонок, смс, событие и т. д.)
  // меньше 3х байт читать не будем или ошибка и выход по TIMEOUT
  unsigned long int t = 0;
  int rb, i = 0;
  //rpl.toUpperCase();
  if (rpl[0] == 'A' && rpl[1]== 'T' ) {
    // и теперь отработаем команду = АТ  
    SIM800.println(rpl); // в модем
    delay(10);
    t = millis() + TIMEOUT;
    while (!SIM800.available() && millis() < t) delay(1); // ждем канал, как соловей анал...
    if (millis() > t) return -1;
    rpl = "";
  }
  // в этой точке мы по любому тогда, когда модем нам что-то пихает или должен пихать после ATxxxxxxxx
   t = millis() + TIMEOUT;
   do { 
    rb = SIM800.read();
    switch (rb){
      case -1:
        // ничего не дал модем - увеличим таймер
        delay(1);
      case 10:
        // закончим это чтение - символ "новая строка" получен
        if (i<3) {
          rpl = "";
          i = 0;
          continue;
        } else {
        rpl += "\0";      // заменим на конец строки и закончим
        return i-1;
        }
      case 13:
        // игнорируем возврат каретки \r
        ;
      default:
        // другие символы - их в строку чтения
        rpl += (char) rb; 
        if (i + 1 > SIZE_OF_BUFFER) return -2;                    
        i++;
    }
    delay(1); 
  } while (millis() < t);
  // мы сюда попасть не должны
  return -1;
}


boolean check_phone(String &rpl) {
  /*
  const char master_phone1[] PROGMEM = MASTER_PHONE1;
  const char master_phone2[] PROGMEM = MASTER_PHONE2;
  const char master_phone3[] PROGMEM = MASTER_PHONE3;
  const char master_phone4[] PROGMEM = MASTER_PHONE4;
  const char master_phone5[] PROGMEM = MASTER_PHONE5;
  const char* const master_phone[] PROGMEM = {MASTER_PHONE1, MASTER_PHONE2, MASTER_PHONE3, MASTER_PHONE4, MASTER_PHONE5};
  */
  const char* master_phone[] = {MASTER_PHONE1, MASTER_PHONE2, MASTER_PHONE3, MASTER_PHONE4, MASTER_PHONE5};
  for (int i=0; i<5; i++) {
    int j = rpl.indexOf(master_phone[i]);
    if ( j > -1) {
      // номер попался из списка )
      rpl = rpl.substring(j,j+13);
      return true;
    }
  }
  return false;
}

boolean wwDTMF() {
  
  int dtmf = -1;
  boolean rc = false;
  
  reply = F("AT+DDET=1");
  if (sim800rw(reply)!=2) {
    Serial.print("DDET не ОК");
    return false;
  }
  reply = F("ATA");
  if (sim800rw(reply)!=2) {
    Serial.print("не ATA");
    return false;
  }
  sleep_time = millis() + wait_dtmf;
  do {
    // вот тут человек должен нажимать кнопочки: как сделать? сразу заставить читать или подождать available?
    reply = "";
    dtmf = -1;
    //rcode = sim800rw(reply); 
    while (!SIM800.available() && millis() < sleep_time) delay(100); 
    if (millis() > sleep_time) break; // не дождались, сливаемся
    rcode = sim800rw(reply);
    //Serial.println(reply);
    if (rcode == 8 && reply.startsWith(F("+DTMF")) ) {      
      // получили код DTMF = разберем, что это
      sleep_time = millis() + wait_dtmf; // обновим таймер
      reply = reply.substring(7,8);
      dtmf = reply.toInt();
      //reply = "";
      if (dtmf == 0) break; // закончим цикл while
      switch (dtmf) {
        case 1:
          P_DEBUG("Open TOP");
          break;
        case 2:
          //P_DEBUG("Select 2 clip");
          rc = ReskiOut(2);
          break;
        case 3:
          P_DEBUG("Open TOP");
          break;
        case 4:
          //P_DEBUG("Select 1 clip");
          rc = ReskiOut(1);
          break;
        case 5:
          //P_DEBUG("ReskiOut"); 
          rc = ReskiOut(4);
          break;
        case 6:
          //P_DEBUG("Select 3 clip");
          rc = ReskiOut(3);
          break;
        case 7:
          P_DEBUG("Open BOTTOM");
          break;
        case 8:
          //P_DEBUG("Select 0 clip");
          rc = ReskiOut(0);
          break;
        case 9:
          P_DEBUG("Open BOTTOM");
          break;
        }
       } else {
        // а это нихрена не DTMF - а это может быть СМСка и еще какая-то хрен - потом разберемся
        if (rcode == 10 && reply.startsWith(F("NO CARRIER"))) break;
       }
    } while (millis() < sleep_time); 
  reply = F("ATH");
  rcode=sim800rw(reply);
  reply = F("AT+DDET=0");
  rcode=sim800rw(reply);
} // конец функци

boolean wwSMS(String &rpl) {
  // пришла смс-ка - прочитаем 
  String n_sms = rpl.substring(12) ;
  rpl = "AT+CMGR=" + n_sms; // получим номер смски и добавим к команде чтения 
  do {
    rcode=sim800rw(rpl);
    // после команды AT+CMGR=[Номер] прийдет 
    // 1е - R_Code = 61;+CMGR: "REC UNREAD","+380939880888","","20/04/08,15:52:26+12"
    // R_Code = 4; содержимое
    // R_Code = 2;OK - c OK делать ничего не будем, по нему свалим с while
    if (rpl.startsWith(F("+CMGR:"))) {
       if (check_phone(rpl)) {
              // в rpl пока хранится номер отправителя
              rpl = "";
              rcode=sim800rw(rpl); // тупо прочитаем, что там 
              rpl.toUpperCase();
              if (rpl.startsWith(F("TOP"))) {
                P_DEBUG("Open TOP");
              } else if (rpl.startsWith(F("BOTTOM"))) {
                P_DEBUG("Open BOTTOM");
              } else if (rpl.startsWith(F("RESKI"))) {
                P_DEBUG("ReskiOut");
                rc_RO = ReskiOut(4);
              } else if (rpl.startsWith(F("POT"))) {
                P_DEBUG("POT");
                rpl = rpl.substring(3,5); // только два знака актуальны
                xpos_angle = rpl.toInt();
                if (xpos_angle<25) xpos_angle=25;
                if (xpos_angle>90) xpos_angle=90;
              } 
              rpl = "";
            } else {
              rpl = "";
              rcode=sim800rw(rpl);
              rpl = ""; // игнорируем 
            }  
    } 
  } while (!(rpl[0] == 'O' && rpl[1] == 'K') && rcode>0);
  // грохнем пока так или будем грохать по одной?
  rpl = "AT+CMGD="  + n_sms + ",0";
  do {
    rcode=sim800rw(rpl);
  } while (!(rpl[0] == 'O' && rpl[1] == 'K'));
}

boolean ReskiOut(int clip) {
  
  boolean goods_out = false;          // фиксируем факт выбачи
  unsigned long int t = 0;            // переменная под время ожидания
  
  if (!empty_out()) {
    // в месте выдачи лежит какая-то хрень - не работаем 
    return false;
  }
  
  if (clip == 4) {
    // найдем самый "полный" клип
    clip = 0;
    for (int j=1; j<4; j++) if (stock[clip] < stock[j]) clip = j;
  }
  
  // 2. "зажимаем" второй ряд поворотом X servo
  // 3. ждем
  // 4. "открываем" выбранную обойму, если она не пустая
  // 5. ждем 
  // 6. выдача состоялась? "закрываем" обойму, ждем, "разжимаем" 2й ряд, уменьшаем остаток и возвращаем true
  // 7. выдача не состоялась? повторяем еще 2 раза и сообщаем об ошибке, если ничего не получилось
  
  if (stock[clip]>0) {
    xservo.attach(xservo_pin[clip]);
    zservo.attach(zservo_pin[clip]);
    xservo.write(xpos_angle);
    delay(1000);
    zservo.write(90);
    // делей надо заменить на ожидание сигнало от датчика(ов) выдачи товара о том, что ВЫДАНО
    delay(1000);
    stock[clip]--;
    zservo.write(0);
    delay(1000);
    xservo.write(0);
    delay(1000);
    xservo.detach();
    zservo.detach();
    return true;
  } else { // конец выдачи из выбранного клипа
    // clip пустой или какая-то хрень лежит в коробке для выдачи - то есть она не пустая 
    return false;
  }
  return goods_out;
}

boolean empty_out() {
  return true; 
}
