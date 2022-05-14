/*
  Echo serial for testing
*/


void setup() {
  // initialize serial communication at 9600 bits per second:
  Serial.begin(9600);
}

void loop() {

  // echo
  if(Serial.available()) {
    char character = Serial.read();
    Serial.print(character);
  }

}
