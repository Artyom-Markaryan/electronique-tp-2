// Libraries
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

// Adresses des pins GPIO sur ESP32
const int gpio5 = 5;
const int gpio33 = 33;
const int gpio32 = 32;
const int gpio26 = 26;

// Adresse habituelle du ADS7830
#define ADS7830_ADDR 0x4B
// 0x27 est typiquement l'adresse I2C de l'ecran. 
// Si ça ne fonctionne pas, essayez avec 0x3f ou 0x4b
LiquidCrystal_I2C lcd(0x27, 16, 2);
// Moteur Servo
Servo servo_gpio26;

// Symbole de clignotant pour l'écran LCD
uint8_t lcd_lamp_symbol[8] = { 
	0b01110, 
	0b11111, 
	0b11111, 
	0b01110, 
	0b00000, 
	0b01110, 
	0b00000, 
	0b01110, 
};

// Variables pour l'accéleration et la vitesse
const float max_forward_speed = 100.0;
const float max_backward_speed = -50.0;

const float forward_acceleration = 0.5;
const float backward_acceleration = 0.3;
const float deceleration_speed = 0.1;

float current_speed = 0.0;

// Valeurs de sorties : -1 pour err, sinon entre 0 et 255.
// addr = adresse du module adc
// int channel = le canal de l'adc. A0 = 0, A1 = 1, ...
int ads7830_read_selected_channel(int addr, int channel)
{
	const int read_command_base = 0x84;
	
	// Transmet la commande de sélection de canal
	Wire.beginTransmission(addr);
	{
		// 3 bits dans l'octet de commande sont utilisés pour indiquer le canal de lecture
		// mais le mapping est un peu bizarre :
		// En considérant le nibble suivant : MSB C2 C1 C0
		// C2 = 1
		// C1 = 4
		// C0 = 2
		// Le canal 5 devient : MSB 1 1 0
		// Le canal 3 devient : MSB 1 0 1
		// Le canal 6 devient : MSB 0 1 1
		unsigned int arg_channel = (((channel << 2) | (channel >> 1)) & 0x07) << 4;
		unsigned char cmd = (unsigned char)((read_command_base | arg_channel) & 0xFF);
		Wire.write(cmd);
	}
	Wire.endTransmission();
	
	// Lit la réponse.
	Wire.requestFrom(addr, 1);
	if( !Wire.available() )
	{
		return -1;
	}
	
	return Wire.read();
}

void night_light() {
	int photoresistor_value = ads7830_read_selected_channel(ADS7830_ADDR, 7);
  // Serial.println(photoresistor_value, DEC);
	if (photoresistor_value > 150) {
		digitalWrite(gpio5, HIGH);
	}
	else {
		digitalWrite(gpio5, LOW);
	}
}

void turn_signal(void* parameter) {
	while (true) {
		digitalWrite(gpio33, HIGH);
		delay(500);
		digitalWrite(gpio33, LOW);
		delay(250);
	}
}

void update_servo_motor() {
	float joystick_x_value = ads7830_read_selected_channel(ADS7830_ADDR, 0);
	int rotation_to_do = constrain(joystick_x_value, -180, 180);
	servo_gpio26.write(rotation_to_do);

	Serial.print("X: ");
	Serial.println(joystick_x_value, DEC);

	delay(250);
}

void update_lcd_screen(int speed) {
	lcd.clear();
	lcd.print("Vitesse: ");
	lcd.print(speed);
	lcd.print(" ");

	if (digitalRead(gpio32) == 1) {
		lcd.print("(");
		lcd.write((byte)0);
		lcd.print(")");
	}
}

void update_speed() {
	float joystick_y_value = ads7830_read_selected_channel(ADS7830_ADDR, 1);
	
	Serial.print("Y: ");
	Serial.println(joystick_y_value, DEC);

	if (joystick_y_value > 0) {
		current_speed += (joystick_y_value / 100) * forward_acceleration;
		current_speed = constrain(current_speed, max_backward_speed, max_forward_speed);
	} 
	else if (joystick_y_value < 0) {
		current_speed += (joystick_y_value / 100) * backward_acceleration;
		current_speed = constrain(current_speed, max_backward_speed, max_forward_speed);
	}
	else {
		if (current_speed > 0) {
			current_speed -= deceleration_speed;
			current_speed = constrain(current_speed, 0, max_forward_speed);
		} else if (current_speed < 0) {
			current_speed += deceleration_speed;
			current_speed = constrain(current_speed, max_backward_speed, 0);
		}
	}

	update_lcd_screen(current_speed);
}

void setup() {
  Serial.begin(115200);

  Wire.begin(); // La librairie Wire doit être initialisé avant utilisation

	// Une deuxième tâche pour le clignotant
	xTaskCreate(
		turn_signal,
		"task_turn_signal",
		1024,
		nullptr,
		tskIDLE_PRIORITY + 1,
		nullptr
	);

	// Configuration de l'écran LCD
	lcd.init();
	lcd.backlight();
	lcd.clear();
	lcd.createChar(0, lcd_lamp_symbol);
	lcd.home();
	update_lcd_screen(current_speed);

	// Configuration du moteur Servo
	servo_gpio26.attach(gpio26);

	pinMode(gpio5, OUTPUT);
	pinMode(gpio33, OUTPUT);
	pinMode(gpio32, INPUT);
}

void loop() {
	update_speed();
	night_light();
	update_servo_motor();
  delay(100);
}
