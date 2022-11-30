#include <ESP8266WiFi.h>
#include <Ultrasonic.h>
#include <OneWire.h>
#include <DallasTemperature.h>

/* DC motor A (left) */
#define in1 D0 
#define in2 D1
#define enA D2

/* DC motor B (right) */
#define in3 D3
#define in4 D4
#define enB D2 // use one MCU pin for both enA and enB, since both motors always rotate at the same speed

#define buzzer D5

/* Temperature sensor */
#define ONE_WIRE_BUS D6

/* Ultrasonic sensor */
#define echo D7
#define trig D8

/* Battery voltage sensing */
#define batt A0

/* Declarations */
bool moving_front = false, block_front = false; // used for obstacle detection
int dc_speed = 200, crash_dist = 30;
const double Vref = 3.3, div_ratio = 2; // used for voltage measurement
String Request = "";
const char ssid[] = "WiFi_Rover";
const char passwd[] = "123456789";

/* Initialize server at port 80 */
WiFiServer server(80);

/* Setup ultrasonic sensor */
Ultrasonic dist_sensor(trig, echo);

/* Setup an oneWire instance to communicate with any OneWire devices 
 * and pass the oneWire reference to Dallas Temperature 
 */
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature temp_sensor(&oneWire);

void setup() {
  /* Pin definitions */
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  pinMode(enA, OUTPUT);
  pinMode(in3, OUTPUT);
  pinMode(in4, OUTPUT);
  pinMode(enB, OUTPUT);

  /* Establish serial communication; used for debugging */
  Serial.begin(9600);
  /* Establish WiFi access point */
  WiFi.softAP(ssid, passwd);
  delay(3000);

  /* Acquire the IP of the access point; used by the app */ 
  /* Serial.print("Server IP: ");
   * Serial.println((WiFi.softAPIP()));
   */
   
  temp_sensor.begin(); 
  server.begin();
}

void loop() {
  WiFiClient client = server.available();
  /* The app sends motion requests when a button is pressed or released (stop).
   * While a motion button is touched down no requests are sent but the rover is moving.
   * During that interval the rover has to check for obstacles in front of them.
   */
  while (!client) {
    check_distance(); 
    if(moving_front and block_front) { stop(); beep(); } 
    delay(100); 
    return;
  }
  while(!client.available()) delay(1); 
  /* FIXME: Do we really need to wait for it? */

  /* Read the first line of the request */
  Request = client.readStringUntil('\r');

  /* Respond to the client */
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("");
  client.print(get_temperature()); client.print(" oC        "); 
  client.print(get_voltage()); client.println(" V");
  

  /* The request sent has the form "GET /command HTTP/1.1".
   * From the above string we keep only the command. 
   */
  Request.remove(0, 5); // discards "GET /"
  Request.remove(Request.length()-9, 9); // discards " HTTP/1.1"
  
  if (Request == "Refresh") delay(1);
  else if (Request == "Front") front(dc_speed);
  else if (Request == "Rear") rear(dc_speed);
  else if (Request == "Left") left(dc_speed);
  else if (Request == "Right") right(dc_speed);
  else if (Request == "Stop") stop();
  else if (Request == "Beep") beep();
  else if (Request == "Close") crash_dist = 15;
  else if (Request == "Medium") crash_dist = 30;
  else if (Request == "Far") crash_dist = 50; 
  else if (Request != "") dc_speed = Request.toInt();
  
  client.flush();
}

void stop(){
  moving_front = false;
  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
  digitalWrite(in3, LOW);
  digitalWrite(in4, LOW);
  analogWrite(enA, 0);
  analogWrite(enB, 0);  
}

void front(int vel){
  moving_front = true;
  digitalWrite(in1, HIGH);
  digitalWrite(in2, LOW);
  digitalWrite(in3, HIGH);
  digitalWrite(in4, LOW);
  analogWrite(enA, vel);
  analogWrite(enB, vel);
}

void rear(int vel){
  moving_front = false;
  digitalWrite(in1, LOW);
  digitalWrite(in2, HIGH);
  digitalWrite(in3, LOW);
  digitalWrite(in4, HIGH);
  analogWrite(enA, vel);
  analogWrite(enB, vel);  
}

void left(int vel){
  moving_front = false;
  digitalWrite(in1, HIGH);
  digitalWrite(in2, LOW);
  digitalWrite(in3, LOW);
  digitalWrite(in4, HIGH);
  analogWrite(enA, vel);
  analogWrite(enB, vel);    
}

void right(int vel){
  moving_front = false;
  digitalWrite(in1, LOW);
  digitalWrite(in2, HIGH);
  digitalWrite(in3, HIGH);
  digitalWrite(in4, LOW);
  analogWrite(enA, vel);
  analogWrite(enB, vel);  
}

void beep(){
  tone(buzzer,2000);
  delay(500);
  noTone(buzzer);
}

void check_distance(){
  /* Check if there is an obstacle in crash_dist range from the rover */
  if (dist_sensor.read() <= crash_dist) block_front = true;
  else block_front = false;
}

float get_temperature(){
  /* Issue a global temperature request to all devices on the bus */
  temp_sensor.requestTemperatures();
  return temp_sensor.getTempCByIndex(0);
  /* Why "byIndex"?  
   * You can have more than one DS18B20 on the same bus.  
   * 0 refers to the first IC on the wire. 
   */
}

double get_voltage(){
  int adc = analogRead(batt);
  /* The input voltage range of the MCU is Vref  
   * To assure that the pin voltage never exceeds that limit, 
   * a voltage divider is used on the input (ratio: R_ground/R_tot).
   * The ADC of the MCU has 10-bit precision, 
   * so returns an integer value in the range [0, 1023].
   * To calculate the battery voltage, the adc value must be mapped to
   * a float value in [0, Vref] and then find the battery voltage 
   * using the (inverse) voltage division ratio of the input.
   */
  double format = div_ratio*Vref/1024;
  return adc * format;
}
