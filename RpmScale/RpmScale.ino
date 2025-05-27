unsigned long ulSlidingMean = 0;
unsigned long tickValues[8] = {0};

volatile unsigned long lastMicros = 0;
volatile unsigned long intervalMicros = 0;
volatile bool newRPM = false;

void setup() {
  Serial.begin(9600);
  
  memset( tickValues, 0, sizeof(tickValues) );
  
  pinMode(2, INPUT_PULLUP);  // z. B. Hall-Sensor an Pin 2
  attachInterrupt(digitalPinToInterrupt(2), pulseISR, RISING);  // oder FALLING
}

void loop() {
  if (newRPM) {
    noInterrupts();
    unsigned long interval = intervalMicros;
    newRPM = false;
    interrupts();

    if (interval > 0) {
      float rpm = 60.0e6 / interval; // 60 Mio µs / Zeit = RPM
      Serial.print("RPM: ");
      Serial.println(rpm);
    } else {
      Serial.println("Warte auf 2. Impuls...");
    }
    Serial.println( calcMean() );
  }
  
  pulseISR();
}

void pulseISR() {
  unsigned long now = micros();
  intervalMicros = now - lastMicros;
  lastMicros = now;
  tickValues[ 0x7 & ulSlidingMean++ ] = intervalMicros;
  newRPM = true;
}

unsigned long calcMean()
{
  unsigned long sum = 0;
  for ( unsigned int i = 0; i < 8; i++ )
    sum += tickValues[ i ];
  
  return sum / 8;
}
