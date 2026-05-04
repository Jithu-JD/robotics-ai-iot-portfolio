#include <WiFi.h>
#include <WebServer.h>

// ========== WiFi (Mobile Hotspot) ==========
const char* ssid     = "KIRAN";
const char* password = "Kiran@2025#";

// ========== Web server ==========
WebServer server(80);

// ========== Motor Pins ==========
#define IN1 14
#define IN2 27
#define IN3 26
#define IN4 25

#define ENA_PIN 33
#define ENB_PIN 32

// PWM
const int PWM_FREQ = 20000;
const int PWM_RES  = 8;
const int CH_A = 0;
const int CH_B = 1;

// Speed
uint8_t manualSpeed = 250;
uint8_t autoSpeed   = 250;

// Ultrasonic
#define TRIG_PIN 4
#define ECHO_PIN 5

// Control flags
bool forwardCmd = false;
bool backCmd    = false;
bool leftCmd    = false;
bool rightCmd   = false;

bool obstacleMode = false;

// Auto state
enum AutoState {
  AUTO_IDLE,
  AUTO_FORWARD,
  AUTO_BACKWARD,
  AUTO_TURN
};

AutoState autoState = AUTO_IDLE;
unsigned long autoStateStart = 0;
bool turnLeftNext = true;

const unsigned long BACK_TIME = 600;
const unsigned long TURN_TIME = 700;

const float OBSTACLE_DIST_CM = 20.0;

// ========== HTML ==========
const char index_html[] PROGMEM = R"=====(<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body { text-align:center; background:#111; color:white; font-family:Arial;}
button { padding:20px; margin:5px; font-size:18px; border-radius:10px;}
</style>
</head>
<body>
<h1>Mission Moon Walker</h1>

<button onmousedown="cmd('f',1)" onmouseup="cmd('f',0)">Forward</button><br>
<button onmousedown="cmd('l',1)" onmouseup="cmd('l',0)">Left</button>
<button onclick="cmd('s',1)">Stop</button>
<button onmousedown="cmd('r',1)" onmouseup="cmd('r',0)">Right</button><br>
<button onmousedown="cmd('b',1)" onmouseup="cmd('b',0)">Backward</button>

<h2>Obstacle Mode</h2>
<button onclick="mode(1)">ON</button>
<button onclick="mode(0)">OFF</button>

<script>
function cmd(d,s){ fetch(`/cmd?dir=${d}&state=${s}`); }
function mode(a){ fetch(`/mode?auto=${a}`); }
</script>

</body>
</html>)=====";

// ========== MOTOR ==========
void setMotorSpeed(uint8_t l, uint8_t r) {
  ledcWrite(CH_A, l);
  ledcWrite(CH_B, r);
}

void stopCar() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  setMotorSpeed(0,0);
}

void forward(uint8_t s){
  digitalWrite(IN1,HIGH); digitalWrite(IN2,LOW);
  digitalWrite(IN3,HIGH); digitalWrite(IN4,LOW);
  setMotorSpeed(s,s);
}

void backward(uint8_t s){
  digitalWrite(IN1,LOW); digitalWrite(IN2,HIGH);
  digitalWrite(IN3,LOW); digitalWrite(IN4,HIGH);
  setMotorSpeed(s,s);
}

void left(uint8_t s){
  digitalWrite(IN1,LOW); digitalWrite(IN2,HIGH);
  digitalWrite(IN3,HIGH); digitalWrite(IN4,LOW);
  setMotorSpeed(s,s);
}

void right(uint8_t s){
  digitalWrite(IN1,HIGH); digitalWrite(IN2,LOW);
  digitalWrite(IN3,LOW); digitalWrite(IN4,HIGH);
  setMotorSpeed(s,s);
}

// ========== ULTRASONIC ==========
float getDistance(){
  digitalWrite(TRIG_PIN,LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN,HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN,LOW);

  long d = pulseIn(ECHO_PIN,HIGH,30000);
  if(d==0) return -1;
  return d*0.034/2;
}

// ========== CONTROL ==========
void manualControl(){
  if(forwardCmd) forward(manualSpeed);
  else if(backCmd) backward(manualSpeed);
  else if(leftCmd) left(manualSpeed);
  else if(rightCmd) right(manualSpeed);
  else stopCar();
}

void autoControl(){
  unsigned long now = millis();

  switch(autoState){
    case AUTO_IDLE:
      autoState = AUTO_FORWARD;
      break;

    case AUTO_FORWARD:{
      float d = getDistance();
      if(d>0 && d<OBSTACLE_DIST_CM){
        autoState = AUTO_BACKWARD;
        autoStateStart = now;
      } else forward(autoSpeed);
      break;
    }

    case AUTO_BACKWARD:
      if(now-autoStateStart>BACK_TIME){
        autoState = AUTO_TURN;
        autoStateStart = now;
      } else backward(autoSpeed);
      break;

    case AUTO_TURN:
      if(turnLeftNext) left(autoSpeed);
      else right(autoSpeed);

      if(now-autoStateStart>TURN_TIME){
        turnLeftNext = !turnLeftNext;
        autoState = AUTO_FORWARD;
      }
      break;
  }
}

// ========== WEB ==========
void handleRoot(){ server.send_P(200,"text/html",index_html); }

void handleCmd(){
  String d = server.arg("dir");
  int s = server.arg("state").toInt();

  if(!obstacleMode){
    bool p = s!=0;
    if(d=="f"){ forwardCmd=p; backCmd=leftCmd=rightCmd=false;}
    else if(d=="b"){ backCmd=p; forwardCmd=leftCmd=rightCmd=false;}
    else if(d=="l"){ leftCmd=p; forwardCmd=backCmd=rightCmd=false;}
    else if(d=="r"){ rightCmd=p; forwardCmd=backCmd=leftCmd=false;}
    else if(d=="s"){ forwardCmd=backCmd=leftCmd=rightCmd=false; stopCar();}
  }
  server.send(200,"text/plain","OK");
}

void handleMode(){
  int m = server.arg("auto").toInt();
  if(m==1){
    obstacleMode=true;
    autoState=AUTO_FORWARD;
  } else{
    obstacleMode=false;
    stopCar();
  }
  server.send(200,"text/plain","OK");
}

// ========== SETUP ==========
void setup(){
  Serial.begin(115200);

  pinMode(IN1,OUTPUT); pinMode(IN2,OUTPUT);
  pinMode(IN3,OUTPUT); pinMode(IN4,OUTPUT);

  pinMode(ENA_PIN,OUTPUT);
  pinMode(ENB_PIN,OUTPUT);

  ledcSetup(CH_A,PWM_FREQ,PWM_RES);
  ledcAttachPin(ENA_PIN,CH_A);
  ledcSetup(CH_B,PWM_FREQ,PWM_RES);
  ledcAttachPin(ENB_PIN,CH_B);

  pinMode(TRIG_PIN,OUTPUT);
  pinMode(ECHO_PIN,INPUT);

  // ✅ CONNECT TO HOTSPOT
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid,password);

  Serial.print("Connecting");
  while(WiFi.status()!=WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  server.on("/",handleRoot);
  server.on("/cmd",handleCmd);
  server.on("/mode",handleMode);

  server.begin();
}

// ========== LOOP ==========
void loop(){
  server.handleClient();

  if(obstacleMode) autoControl();
  else manualControl();
}