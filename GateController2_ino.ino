
#include <SoftwareSerial.h>

const int WAIT = 100;
const int GATE_WING_DELAY = 1000;
const unsigned long TRIGGER_CYCLE_COUNTER = 20000;
const int TRIGGER_STABILIZE_DELAY = 50;
const int SENSOR_STABILIZE_DELAY = 50;
const int MAX_GATE_RUNTIME_MS = 20000;
const int GATE_OPEN_MAX_TIME = 300;
const int GATE_LEFT = 1;
const int GATE_RIGHT = 2;
const int GATE_OPEN = 1;
const int GATE_CLOSE = 2;

const int TRIGGER_PIN = A0;
const int LEFT_WING_OPEN_REED_PIN = A4;
const int LEFT_WING_CLOSE_REED_PIN = A5;
const int RIGHT_WING_OPEN_REED_PIN = A2;
const int RIGHT_WING_CLOSE_REED_PIN = A3;
const int IR_SENSOR_PIN = A1;
const int CMD_LEFT_WING_OPEN_PIN = 3;
const int CMD_LEFT_WING_CLOSE_PIN = 2;
const int CMD_RIGHT_WING_OPEN_PIN = 6;
const int CMD_RIGHT_WING_CLOSE_PIN = 10;
const int LED_PIN = 13;

const int IR_SENSOR_BLOCKED = 1;
const int REED_SENSOR_ON = 0;
const int TRIGGER_ON = 0;

int triggerVal;
int irSensorVal;
int leftWingOpenReedVal;
int leftWingCloseReedVal;
int rightWingOpenReedVal;
int rightWingCloseReedVal;

boolean isClosing;
boolean isMovingLeft;
boolean isMovingRight;
int previousTriggerVal;
unsigned long triggerCycle;
boolean isTriggered;
unsigned long movementStarted;
volatile unsigned int ticker;
volatile boolean ledState;

void setup() {
  Serial.begin(9600);
  
  //Inputs
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  pinMode(LEFT_WING_OPEN_REED_PIN, INPUT_PULLUP);
  pinMode(LEFT_WING_CLOSE_REED_PIN, INPUT_PULLUP);
  pinMode(RIGHT_WING_OPEN_REED_PIN, INPUT_PULLUP);
  pinMode(RIGHT_WING_CLOSE_REED_PIN, INPUT_PULLUP);
  pinMode(IR_SENSOR_PIN, INPUT_PULLUP);
  
  //Outputs
  pinMode(CMD_LEFT_WING_OPEN_PIN, OUTPUT);
  pinMode(CMD_LEFT_WING_CLOSE_PIN, OUTPUT);
  pinMode(CMD_RIGHT_WING_OPEN_PIN, OUTPUT);
  pinMode(CMD_RIGHT_WING_CLOSE_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
    
  //Default Status
  isMovingLeft = false;
  isMovingRight = false;
  isClosing = false;
  isTriggered = false;
  triggerCycle = TRIGGER_CYCLE_COUNTER;
  previousTriggerVal = !TRIGGER_ON;
  ledState = false;
  
  //clear
  digitalWrite(CMD_LEFT_WING_CLOSE_PIN, LOW);
  digitalWrite(CMD_LEFT_WING_OPEN_PIN, LOW);
  digitalWrite(CMD_RIGHT_WING_CLOSE_PIN, LOW);
  digitalWrite(CMD_RIGHT_WING_OPEN_PIN, LOW);
  
  //Timer
  ticker = 0;
  setTimer();
}

void loop() {
  readSensors();
  if (irSensorVal == IR_SENSOR_BLOCKED && isClosing && (isMovingLeft || isMovingRight)) {
    stop(GATE_LEFT);
    stop(GATE_RIGHT);
    isClosing = false;
  } else if (irSensorVal == IR_SENSOR_BLOCKED && isClosing) {
    ticker = 0;
    if (isTriggered) {
      isTriggered = false;
    }
  } else if (isMovingLeft || isMovingRight) {
    if (millis()-movementStarted > MAX_GATE_RUNTIME_MS) {
      Serial.println("Movement time exceeded allowed period");
      stop(GATE_LEFT);
      stop(GATE_RIGHT);
    }
    if (isTriggered) {
      Serial.println("Stop and direction change on trigger");
      stop(GATE_LEFT);
      stop(GATE_RIGHT);
      isClosing = !isClosing;
      isTriggered = false;
    } else {
      if (isMovingLeft) {
        if (isClosing && leftWingCloseReedVal == REED_SENSOR_ON) {
          Serial.println("LEFT Wing closed");
          stop(GATE_LEFT);
        } else if (!isClosing && leftWingOpenReedVal == REED_SENSOR_ON) {
          Serial.println("LEFT Wing opened");
          stop(GATE_LEFT);
        }
      }
      if (isMovingRight) {
        if (isClosing && rightWingCloseReedVal == REED_SENSOR_ON) {
          Serial.println("RIGHT Wing closed");
          stop(GATE_RIGHT);
        } else if (!isClosing && rightWingOpenReedVal == REED_SENSOR_ON) {
          Serial.println("RIGHT Wing opened");
          stop(GATE_RIGHT);
        }
      }
      if (!isMovingLeft && !isMovingRight) { //Now both wings are stopped
          isClosing = !isClosing;
          Serial.println("Switched closing flag");
          Serial.println(isClosing);
      }
    }
  } else if (isTriggered) {
    if (rightWingCloseReedVal == REED_SENSOR_ON || leftWingCloseReedVal == REED_SENSOR_ON || !isClosing) { //Wings are closed, or were closing just stopped -> opening
      Serial.println("OPTION A");
      isClosing = false;
      if (leftWingOpenReedVal != REED_SENSOR_ON) {
        start(GATE_LEFT, GATE_OPEN);
        delay(GATE_WING_DELAY);
        Serial.println("Opening left");
      }
      if (rightWingOpenReedVal != REED_SENSOR_ON) {
        start(GATE_RIGHT, GATE_OPEN);
        Serial.println("Opening right");
      }
    }
    if ((leftWingOpenReedVal == REED_SENSOR_ON || rightWingOpenReedVal == REED_SENSOR_ON || isClosing) && irSensorVal != IR_SENSOR_BLOCKED) {
      Serial.println("OPTION B");
      isClosing = true;
      if (rightWingCloseReedVal != REED_SENSOR_ON) {
        start(GATE_RIGHT, GATE_CLOSE);
        delay(GATE_WING_DELAY);
        Serial.println("Closing right");
      }
      if (leftWingCloseReedVal != REED_SENSOR_ON) {
        start(GATE_LEFT, GATE_CLOSE);
        Serial.println("Closing left");
      }
    }
    isTriggered = false;
  } else if (leftWingOpenReedVal == REED_SENSOR_ON || rightWingOpenReedVal == REED_SENSOR_ON) {
    if (ticker > GATE_OPEN_MAX_TIME) {
      isTriggered = true;
      ticker = 0;
      Serial.println("Gate open timeout reached");
    }
  } else {
    ticker = 0;
  }
}

void readSensors() {
  irSensorVal = digitalRead(IR_SENSOR_PIN);
  triggerVal = digitalRead(TRIGGER_PIN);
  leftWingOpenReedVal = readSensor(LEFT_WING_OPEN_REED_PIN);
  leftWingCloseReedVal = readSensor(LEFT_WING_CLOSE_REED_PIN);
  rightWingOpenReedVal = readSensor(RIGHT_WING_OPEN_REED_PIN);
  rightWingCloseReedVal = readSensor(RIGHT_WING_CLOSE_REED_PIN);
  if (triggerVal != TRIGGER_ON) {
    previousTriggerVal = !TRIGGER_ON;
    triggerCycle = TRIGGER_CYCLE_COUNTER;
  } else if (triggerVal != previousTriggerVal && triggerCycle == TRIGGER_CYCLE_COUNTER && triggerVal == TRIGGER_ON) {
    delay(TRIGGER_STABILIZE_DELAY);
    triggerVal = digitalRead(TRIGGER_PIN);
    if (triggerVal == TRIGGER_ON) {
      isTriggered = true;
      previousTriggerVal = triggerVal;
      triggerCycle--;
    } else {
      Serial.println("False trigger");
    }
  } else if (triggerCycle <= 0 && triggerVal == TRIGGER_ON) {
    Serial.println("Trigger CONTINUE");
    triggerCycle = TRIGGER_CYCLE_COUNTER;
    triggerCycle--;
  } else if (triggerCycle <= 0) {
    triggerCycle = TRIGGER_CYCLE_COUNTER;
    previousTriggerVal = !TRIGGER_ON;
    Serial.println("Trigger RESET");
  } else if (triggerCycle < TRIGGER_CYCLE_COUNTER) {
    triggerCycle--;
  }
  
  if (isTriggered) {
    Serial.println("TRIGGER");
    if (isClosing) {
      Serial.println("Closing");  
    } else {
      Serial.println("Opening");   
    }
    if (isMovingLeft) {
      Serial.println("LEFT moving");  
    }
    if (isMovingRight) {
      Serial.println("RIGHT moving");  
    }
    Serial.println("*****************");
    Serial.println(irSensorVal==IR_SENSOR_BLOCKED, DEC);
    Serial.println(leftWingOpenReedVal==REED_SENSOR_ON, DEC);
    Serial.println(leftWingCloseReedVal==REED_SENSOR_ON, DEC);
    Serial.println(rightWingOpenReedVal==REED_SENSOR_ON, DEC);
    Serial.println(rightWingCloseReedVal==REED_SENSOR_ON, DEC);
    Serial.println("*****************");
  }
}

int readSensor(int sensorPin) {
  int sensorVal = digitalRead(sensorPin);
  if (sensorVal == REED_SENSOR_ON) {
    delay(SENSOR_STABILIZE_DELAY);
    sensorVal = digitalRead(sensorPin);
    if (sensorVal != REED_SENSOR_ON) {
      Serial.println("False reed sensor value");
    }
  }
  return sensorVal;
}

void stop(int gate) {
   if (gate == GATE_LEFT) {
      digitalWrite(CMD_LEFT_WING_CLOSE_PIN, LOW);
      digitalWrite(CMD_LEFT_WING_OPEN_PIN, LOW);
      isMovingLeft = false;
    } else {
      digitalWrite(CMD_RIGHT_WING_CLOSE_PIN, LOW);
      digitalWrite(CMD_RIGHT_WING_OPEN_PIN, LOW);
      isMovingRight = false;
    }
}

void start(int gate, int direction) { 
  if (gate == GATE_LEFT) {
     isMovingLeft = true;
     if (direction == GATE_OPEN) {
       digitalWrite(CMD_LEFT_WING_CLOSE_PIN, LOW);
       digitalWrite(CMD_LEFT_WING_OPEN_PIN, HIGH);
     } else {
       digitalWrite(CMD_LEFT_WING_CLOSE_PIN, HIGH);
       digitalWrite(CMD_LEFT_WING_OPEN_PIN, LOW);
     }
   } else {
     isMovingRight = true;
      if (direction == GATE_OPEN) {
        digitalWrite(CMD_RIGHT_WING_CLOSE_PIN, LOW);
        digitalWrite(CMD_RIGHT_WING_OPEN_PIN, HIGH);
      } else {
        digitalWrite(CMD_RIGHT_WING_CLOSE_PIN, HIGH);
        digitalWrite(CMD_RIGHT_WING_OPEN_PIN, LOW);
      }
   }
   movementStarted = millis();
}

void setTimer() {
  cli();
  TCCR1A = 0;// set entire TCCR1A register to 0
  TCCR1B = 0;// same for TCCR1B
  TCNT1  = 0;//initialize counter value to 0
  // set compare match register for 1hz increments
  OCR1A = 15624;// = (16*10^6) / (1*1024) - 1 (must be <65536)
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  // Set CS10 and CS12 bits for 1024 prescaler
  TCCR1B |= (1 << CS12) | (1 << CS10);  
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);
  sei();
}

ISR(TIMER1_COMPA_vect) {
  ticker++;
  if (ledState) {
    digitalWrite(LED_PIN, HIGH);  
  } else {
    digitalWrite(LED_PIN, LOW);    
  }
  ledState = !ledState;
}
