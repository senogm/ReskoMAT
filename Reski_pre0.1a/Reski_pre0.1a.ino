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
#define MASTER_PHONE1 "+380503008444"
#define MASTER_PHONE2 "+380939880888"
#define MASTER_PHONE3 "+380661026086"
#define MASTER_PHONE4 "+380504077212"
#define MASTER_PHONE5 "+380000000000"   // НОМЕР ПОД ПРИВАТБАНК 

#include "avr/sleep.h"
#include <SoftwareSerial.h>

#define SIZE_OF_BUFFER 99
#define TIMEOUT 5000

#define pin_rx 2
#define pin_tx 4

#define period 50000         // 10 секунд крутим щарманку и если все тихо, то спать!
#define wait_ok 1000         // сколько времени ждать ОК от модема на AT команду - например 1 секунда, больше - нету ответа

SoftwareSerial SIM800(pin_rx, pin_tx);        // 8 - RX Arduino (TX SIM800L), 9 - TX Arduino (RX SIM800L)

// sendATCommand("AT+DDET=1,0,0", true);     // Включить DTMF
// AT+CWHITELIST={0 - off,1 - call,2 - sms,3 - call&sms},{1-30},tel - не работает пока
// +IPR: (),(0,0b00,2400,4800,9600,19200,38400,57600,115200,230400,460800
// AT+CSCLK=0 - спящий режим выкл, 1 - ? (для выкл + low to DTR) , 2 - используем этот. После RX int - at, 100ms, at+csclk=0

#define _AT_ F("AT")
#define _AT_DDET_ F("AT+DDET=");
#define _AT_INIT_ F("ATE0V1+CMEE=0;+CLIP=1;+CMGF=1;+CSCLK=0;&W");
#define _AT_CSCLK_ F("AT+CSCLK=");
#define _AT_DELSMS_ F("AT+CMGDA=\"DEL ALL\"")

volatile long sleep_time = 0;

volatile boolean TOP, BOTTOM, RESKIOUT = true; // состояние механизвом true - можно запускать, false = нельзя

String reply="";

int rcode;

void setup() {
  Serial.begin(9600);               // Скорость обмена данными с компьютером
  SIM800.begin(9600);               // Скорость обмена данными с модемом
  // скорось пусть по порту модем наш вырявняет
  SIM800.println(_AT_);
  delay(100);
  // настроим с начала и правильно 
  reply = _AT_INIT_;
  do {
    rcode=sim800rw(reply);
  } while (!(reply[0] == 'O' && reply[1] == 'K') && rcode > 0);
  if (rcode < 0) Serial.print("timeout = -1, buffer = -2; After init error RC =" + rcode);
  // перед началом работы, грохним все sms-ки
  reply = _AT_DELSMS_;
  do {
    rcode=sim800rw(reply);
  } while (!(reply[0] == 'O' && reply[1] == 'K') && rcode > 0);

  rcode = 0;
  reply = "";
  
  Serial.println("Start!");
  sleep_time = millis() + period;
}

void loop() {
 
  if (SIM800.available()) {
    // появилось что то почитать
    rcode=sim800rw(reply);
    if (rcode > 2) {
      if (reply.startsWith(F("RING"))) {
        // разбираем звоночек
          Serial.print("Звонят ");
          reply = "";
          // должен идти +CLIP
          rcode=sim800rw(reply);
          if (reply.startsWith(F("+CLIP"))) {
            // нам точно звонят и мы точно получили номер звонящего 
            if (check_phone(reply)) {
              Serial.println("Шеф звонит с номера " + reply);
              
              boolean rc = wwDTMF();
            } else {
              Serial.print("...лохи звонят...");
              reply = F("ATH");
              rcode = sim800rw(reply);
            } 
          }
      } else if (reply.startsWith("+CMTI")) {
        // разбираем код смски
          //Serial.print("SMSят..");
          boolean rc = wwSMS(reply);
      } else {
        Serial.print("\r\nR_Code = ");
        Serial.print(rcode);
        Serial.print(";");  
        Serial.println(reply);
        reply = "";
      }
    }    
    sleep_time = millis() + period;
  }

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
        Serial.print("R_Code = ");
        Serial.print(rcode);  
        Serial.print(";");
        Serial.println(reply);
        if (rcode != 2) reply = "";
      } while (!(reply[0] == 'O' && reply[1] == 'K') && rcode > 0);
    } else reply = "";
  }
  
  
  if (millis()>sleep_time){
    Serial.println("Sleep!");
    delay(100);
    // притушить модем
    // отключить силовые модули
    attachInterrupt(digitalPinToInterrupt(pin_rx), myISRrx, LOW);
    set_sleep_mode(SLEEP_MODE_STANDBY);
    sleep_mode();
    // если WAKEUP из за модема, сначала надо прочитать событие!
    // подключать модем будем, когда будем в него что то слать, а пока просто прочитаем, что он нам прислал
    // подключить силовые модули?
  }
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
  // в этой точке мы по любому тогда, когда модем нам что-то пихает или должен
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


void myISRrx() {
detachInterrupt(digitalPinToInterrupt(pin_rx));
sleep_time = millis() + period;
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
  reply = F("AT+DDET=1");
    
  if (sim800rw(reply)!=2) {
    Serial.print("DDET не ОК");
    return false;
  }
  reply = F("ATA");
  sleep_time = millis() + period;
  if (sim800rw(reply) == 2) {
    // сняли трубку и получили окей - крутим цикл
    while (millis() < sleep_time) {
      // вот тут человек должен нажимать кнопочки: как сделать? сразу заставить читать или подождать available?
      reply = "";
      //rcode = sim800rw(reply); 
      if (SIM800.available()) rcode = sim800rw(reply);
      if (rcode == 8 && reply.startsWith(F("+DTMF")) ) {
      // получили код DTMF = разберем, что это
      sleep_time = millis() + period; // обновим таймер
      reply = reply.substring(7,8);
      int dtmf = reply.toInt();
      if (dtmf == 0) break; // закончим цикл while
      switch (dtmf) {
        case 2:
          Serial.println("Open TOP");
          break;
        case 5:
          Serial.println("ReskiOut");
          break;
        case 8:
          Serial.println("Open BOTTOM");
          break;
        }
       } else {
        // а это нихрена не DTMF
        if (rcode == 10 && reply.startsWith(F("NO CARRIER"))) break;
       }
      } // end of while
      reply = F("AT+DDET=0");
      rcode=sim800rw(reply);
      reply = F("ATH");
      rcode=sim800rw(reply);
    }
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
       if (check_phone(reply)) {
              // в rpl пока хранится номер отправителя
              rpl = "";
              rcode=sim800rw(rpl); // тупо прочитаем, что там 
              rpl.toUpperCase();
              if (rpl.startsWith(F("TOP"))) {
                Serial.println("Open TOP");
              } else if (rpl.startsWith(F("BOTTOM"))) {
                Serial.println("Open BOTTOM");
              } else if (rpl.startsWith(F("RESKI"))) {
                Serial.println("ReskiOut");
              }
              rpl = "";
            } else {
              //Serial.println("Лохи прислали sms-ку");
              rpl = "";
              rcode=sim800rw(rpl);
              rpl = ""; // игнорируем 
            }  
    } 
  } while (!(rpl[0] == 'O' && rpl[1] == 'K'));
  // грохнем пока так или будем грохать по одной?
  rpl = "AT+CMGD="  + n_sms + ",0";
  do {
    rcode=sim800rw(rpl);
  } while (!(rpl[0] == 'O' && rpl[1] == 'K'));
}
