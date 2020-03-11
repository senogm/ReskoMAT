#include <avr/sleep.h>
#include <Servo.h>

#define button_pin   2    // пин кнопки под прерывание 
#define xservo_pin   3    // пин X сервы
#define pot_pin      7    // потенциометр = на nano - A7

#define zstep_en  5         // пин Z шаг
#define zstep_ms1 6         // пин Z шаг
#define zstep_ms2 7         // пин Z шаг
#define zstep_ms3 8         // пин Z шаг
#define zstep_rst 9         // пин Z шаг
#define zstep_sleep   10    // пин Z шаг
#define zstep_steppin 11    // пин Z шаг
#define zstep_dirpin  12    // пин Z шаг

#define led_pin      13     // пин подключения LED
#define xmax_angle   180    // максимальный угол поворота Х сервы
#define xservo_wait  1000   // задержка Х сервы
#define zservo_wait  1000   // задержка Z сервы
// количество шагов на 1 оборот
#define step_round 200
// скорость двигателя Z
#define SPEED 10

int pot, new_pot = 0;     // показания подстроечника для угла поворода Х сервы
int xpos_angle = 0;       // и угол ее поворота для прижима второго ряда Рескинола

Servo xservo; 

volatile int state = LOW;
 
void setup()
{
  Serial.begin(9600);
  Serial.println("Начали с ожиданием");
  // фонарик 
  pinMode(led_pin, OUTPUT);
  // режим  для ZStepper
  pinMode(zstep_en, OUTPUT);
  pinMode(zstep_ms1, OUTPUT);
  pinMode(zstep_ms2, OUTPUT);
  pinMode(zstep_ms3, OUTPUT);
  pinMode(zstep_rst, OUTPUT);
  pinMode(zstep_sleep, OUTPUT);
  pinMode(zstep_steppin, OUTPUT);
  pinMode(zstep_dirpin, OUTPUT);
  // начальные значения для ZStepper
  digitalWrite(zstep_en, LOW);
  digitalWrite(zstep_ms1, LOW);
  digitalWrite(zstep_ms2, LOW);
  digitalWrite(zstep_ms3, LOW);
  digitalWrite(zstep_rst, HIGH);  
  digitalWrite(zstep_sleep, HIGH);
  digitalWrite(zstep_steppin, 1);
  digitalWrite(zstep_dirpin, 0);
  // прерывание по кнопке 
  attachInterrupt(0, blink, RISING);
  new_pot = analogRead(pot_pin); // прочитаем, чего накрутили инженеры
  xpos_angle = map(new_pot,0,1023,0,xmax_angle);
  xservo.attach(xservo_pin);
  xservo.write(0);               // разожмем второй ряд Рескинола
  blinking(5,200);                // мигнем 5 раз дня идентификации начала работы
  //delay(xservo_wait);
}
 
void loop()
{
  //Serial.print("Sleep! Angle = ");
  //Serial.println(xpos_angle);
  //delay(100);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_mode();
  // выйти из режима сна мы можем только по нажатию кнопки 
  //Serial.println("Изменили значения LED");
  //digitalWrite(led_pin, state);
  //delay(100);
  //ReskiOut();
  //Serial.println("ReskiOut...");
  new_pot = analogRead(pot_pin); // прочитаем, чего накрутили инженеры
  Serial.println(new_pot);
  if (abs(pot-new_pot)>10) {         // подкрутили потенциометр
      pot = new_pot;
      xpos_angle = map(new_pot,0,1023,0,xmax_angle);
      blinking(3,300);
      //Serial.print("Угол поворота Х-сервы:");
      //Serial.println(xpos_angle);
      //delay(300);
    }
   
  ReskiOut();
   
}
 
void blink()
{
  state = !state;
}

void blinking(int y, int t)
{
  for(int x = 0; x < y; x++)
      {
        digitalWrite(led_pin, HIGH);
        delay(t);
        digitalWrite(led_pin, LOW);
        delay(t);
      }
}

void ReskiOut()
{
  
  xservo.write(xpos_angle);             // зажмем второй ряд Рескинола
  blinking(3,300);
  //delay(WaitXServo);                  // подолждем, что бы точно зажала
  digitalWrite(zstep_dirpin, HIGH);
  // тут идет код работы со вторым мотором - или одним 
  // повернули от статического состояния на 45"
  for(int j = 0; j < step_round; j++) {
    digitalWrite(zstep_steppin, HIGH);
    delay(SPEED);
    digitalWrite(zstep_steppin, LOW);
    delay(SPEED);
    }
   Serial.print("ReskiOut...");
  //Serial.println(j);
  //blinking(10,100);
  delay(zservo_wait);                  // подолждем, что бы точно выпада
  // повернули еще на 45"
  //for(int j = 0; j < step_round; j++) {
  //  digitalWrite(zstep_steppin, HIGH);
  //  delay(SPEED);
  //  digitalWrite(zstep_steppin, LOW);
  //  delay(SPEED);
  //  }
  xservo.write(0);
  delay(xservo_wait);
}
