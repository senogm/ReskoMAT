/*
 Controlling a servo position using a potentiometer (variable resistor)
 by Michal Rinott <http://people.interaction-ivrea.it/m.rinott>

 modified on 8 Nov 2013
 by Scott Fitzgerald
 http://www.arduino.cc/en/Tutorial/Knob
*/
#include <avr/sleep.h>
#include <Servo.h>
#include <Stepper.h>
// Pin`s of A4988 controller - stepper Z
#define zstep_en   8        // пин Z Enable = должен быть №9
#define zstep_ms1 -1        // пин Z шаг - не используем 
#define zstep_ms2 -1        // пин Z шаг - не используем
#define zstep_ms3 -1        // пин Z шаг - не используем
#define zstep_rst -1        // пин Z шаг - замкнут пока с sleep
#define zstep_sleep  -1     // пин Z шаг - замкнут пока с rst - должен быть №8
#define zstep_steppin 7     // пин Z шаг
#define zstep_dirpin  6     // пин Z шаг
// количество шагов на 1 оборот
#define step_round 200
#define step_round4 50
#define step_round8 25
// скорость двигателя Z
#define SPEED 5

#define led_pin      13     // пин подключения LED

// pin`s for X-Servo
#define xservo_pin   5      // pin X servo 
#define potpin A7           // потенциометр для ручного регулирования угла поворота сервы X
#define xmax_angle   180    // максимальный угол поворота Х сервы
// waiting time
#define xservo_wait  1000   // задержка Х сервы
#define zservo_wait  1000   // задержка Z сервы

Servo myservo;  // create servo object to control a servo

//int potpin = 7;  // analog pin used to connect the potentiometer
int val;    // variable to read the value from the analog pin

boolean Xon = false;
volatile boolean state = false;

unsigned long TonX, TEvt; 

#define WaitXLock 1000
#define WaitX 3000
#define WaitEvent 10000 

void setup() {
  Serial.begin(9600);
  // initial X-servo
  myservo.attach(xservo_pin); // attaches the servo on pin 9 to the servo object
  myservo.write(0);           // unlock second row 
  // режим  для ZStepper
  pinMode(zstep_en, OUTPUT);
  pinMode(zstep_steppin, OUTPUT);
  pinMode(zstep_dirpin, OUTPUT);
  // начальные значения для ZStepper
  digitalWrite(zstep_en, LOW);
  
  //
  TEvt = millis();    // каждый раз, когда происходит внешнее событие, будем обновлять таймер
  attachInterrupt(0, blink, RISING);
}

void loop() {
  if ((millis()-TEvt) > WaitEvent) {      // если ничего не происходит более 5 секунд - впадем в спячку
    //attachInterrupt(0, blink, RISING);    // установим прерывание для выходы из сна по нажатию кнопки
    Serial.println("Sleepng...");
    delay(300);
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_mode();
    TEvt = millis();    // каждый раз, когда происходит внешнее событие, будем обновлять таймер
  }
  
  if (state) {
    noInterrupts();                        // отключим перерывания, что бы не мешали 
    state = false;
    //ReskiOut();
    //void ReskiOut() {
  val = analogRead(potpin);            // reads the value of the potentiometer (value between 0 and 1023)
  val = map(val, 0, 1023, 0, xmax_angle);     // scale it to use it with the servo (value between 0 and 180)  
  myservo.write(val);                  // sets the servo position according to the scaled value
  delay(WaitXLock);                    // waits for the servo to get there
  // теперь шаговый двигатель
  digitalWrite(zstep_dirpin, HIGH);
  // тут идет код работы со вторым мотором - или одним 
  // повернули от статического состояния на 45"
  for(int j = 0; j < step_round8; j++) {
   digitalWrite(zstep_steppin, HIGH);
   //delayMicroseconds(SPEED);
   delay(SPEED);
   digitalWrite(zstep_steppin, LOW);
   //delayMicroseconds(SPEED);
   delay(SPEED);     
  }
  delay(WaitX);                       // ждем падения коробки Рескинола
  for(int j = 0; j < step_round8; j++) {
   digitalWrite(zstep_steppin, HIGH);
   //delayMicroseconds(SPEED);
   delay(SPEED);
   digitalWrite(zstep_steppin, LOW);
   //delayMicroseconds(SPEED);
   delay(SPEED);
  }
  myservo.write(0);                    // sets the servo position according to the scaled value
  delay(WaitXLock);                    // waits for the servo to get there
//}

    TEvt = millis();
    interrupts();  
  }
}

void blink() {
  state = true;
}
