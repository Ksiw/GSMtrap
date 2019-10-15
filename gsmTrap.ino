/*
	Name:       gsmTrap.ino
	Created:  06.09.2019 16:58:29
	Author:     Ksiw
*/

#include <SoftwareSerial.h>
#include <EEPROM.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/wdt.h>

//#define ModemResetPin 6 //Номер выхода подключенного пину перезагрузки модема.
#define GERKON 4
#define BUTTON_OPTION 5
#define ModemResetPin 9 //Номер выхода подключенного пину перезагрузки модема.
#define PHONENUMBER 13 // цифр в номере +1 байт конца строки 
#define SENTSMSSTATUS 32400 //сколько раз спать по 8 секунд, до отправки смс о статусе.

typedef struct
{
	char PhoneNum[PHONENUMBER];
	unsigned short addr;
} Phone;

//-----------------prototypes---------------------------
uint8_t eepromcheckphone(String);
void endcall();
void MasterCommands();

Phone data[1] = { "", 100 };
SoftwareSerial gsm(7, 8);  // RX, TX
char PhoneNum[PHONENUMBER];
char PhoneCharArray[PHONENUMBER];
unsigned long previousGerkonTimeMillis = 0; // Переменная для хранения времени последнего считывания с датчика
int8_t TempCheckTime; // Определяем периодичность проверок в минутах
bool gerkon;
bool smsdone = true;
uint8_t ch = 0;
uint8_t _creg;  // статус сети
uint8_t CSQ1;   //уровень сигнала
String val = "";
String NotCheckRingPhone = "";
const String Marker = " -> ";
const String SendSmsText = "WTF? Look in me!";
const String Status = "The trap is working!";
uint32_t sleepCount = 0;

void setup()
{
	Serial.begin(115200);
	Serial.println("START");
	lineprint();

	pinMode(ModemResetPin, OUTPUT);
	pinMode(GERKON, INPUT_PULLUP);
	pinMode(BUTTON_OPTION, INPUT);
	leavemodem();
}
//--------------------------------------------------------------
void loop()
{// отправить смс, если ловушка сработала
	if (!digitalRead(GERKON) && smsdone)
	{
		switchModem();
		InitModem();
		delay(2000);
		sms("+" + String(EEPROM.get(data[0].addr, PhoneNum)), SendSmsText);
		smsdone = false;
		delay(60000);
		gsm.println(F("AT+CPOWD=1"));
		delay(15000);
	}
	//--------------------------------------------------------------
		
	if (digitalRead(BUTTON_OPTION))  
	{//зажать кнопку для включения модема и приема смс
		switchModem();
		InitModem();
		delay(2000);
		while (digitalRead(BUTTON_OPTION))
		{
			checkGSM();
			delay(1000);
		}
		gsm.println(F("AT+CPOWD=1"));
		delay(15000);
		
	}
	//------------------------------------------------------------
	
	if (sleepCount >= SENTSMSSTATUS)
	{//отправить смс о том, что модем все еще жив	
		leavemodem();
		sleepCount = 0;
	}
	//--------------------------------------------------------------

	delay(200);
	myWatchdogEnable(0b100001); //8сек
	sleepCount++;
}

//--------------------------------------------------------------
void lineprint()
{
	Serial.println("---------------------------------");
}

// --------------------------- Инициализация модема--------------
void InitModem()
{
	delay(5000);  //Время на инициализацию модуля
	gsm.begin(19200);
	for (uint8_t i = 0; i < 10; i++)
	{
		gsm.println(F("AT"));
		delay(200);
	}

	gsm.println(F("ATI"));
	delay(10);
	if (gsm.available())
	{  //Если GSM модуль что-то послал нам, то
		while (gsm.available())
		{  //сохраняем входную строку в переменную val
			ch = gsm.read();
			val += char(ch);
			delay(20);
		}
		val = "";
	}

	gsm.println(F("AT+CLIP=1")); //Включаем АОН
	delay(100);
	gsm.println(F("AT+CMGF=1")); //Режим кодировки СМС - обычный (для англ.)
	delay(100);
	gsm.println(F("AT+CSCS=\"GSM\"")); //Режим кодировки текста
	delay(100);
	gsm.println(F("AT+CNMI=2,2")); //Отображение смс в терминале сразу после приема (без этого сообщения молча падают в память)
	delay(100);
	gsm.println(F("ATE1")); //эхо
	delay(100);
}

//---------------------------------------------------------------
void checkGSM()
{
	if (gsm.available())
	{  //Если GSM модуль что-то послал нам, то
		while (gsm.available())
		{  //сохраняем входную строку в переменную val
			ch = gsm.read();
			val += char(ch);
			delay(10);
		}
		uint8_t l = 1;   //   <+++++++++++++++++++++++++++++++++++++!
		if (val.indexOf(F("RING")) > -1)
		{  //Если звонок обнаружен, то проверяем номер
			String NotCheckRingPhone = val.substring(String(val.indexOf("\"")).toInt() + l, String(val.indexOf(F(","))).toInt() - 1);
			Serial.print(F("Ring from: +"));
			Serial.println(NotCheckRingPhone);
			endcall();
			Serial.println("Get away from me!");
		}

		if (val.indexOf(F("+CMT:")) > -1)
		{  //Если пришло смс, то проверяем номер
			Serial.println(val);
			NotCheckRingPhone = val.substring(String(val.indexOf("+CMT: \"")).toInt() + 8, String(val.indexOf(F(",,"))).toInt() - 1);
			Serial.print(F("SMS from: +"));
			Serial.println(NotCheckRingPhone);
			if (!eepromcheckphone(NotCheckRingPhone))
			{
				MasterCommands(); //искать  в смс команду 
			}
			else
			{
				Serial.println(F("This number already exists!"));
			}
			clearsms();
		}
		else if (val.indexOf(F("+CREG:")) > -1)
		{  //Если пришла инфа о регистрации в сети
			Serial.println(val);
			if (val.indexOf(": ") > -1)
			{
				_creg = (val.substring(String(val.indexOf(",")).toInt() + 1)).toInt();
				Serial.println(_creg);
				if (_creg != 1) //Если модем не в домашней сети перезагрузить модем
				{
					gsm.println(F("AT+CPOWD=1"));
					delay(15000);
					Serial.println("Modem OFF"); 
					switchModem();
					InitModem();
				}

			}
		}
		else if (val.indexOf(F("+CSQ:")) > -1)
		{  //Если пришел уровень сигнала сети
			Serial.println(val);
			if (val.indexOf(": ") > -1)
			{
				CSQ1 = (val.substring(String(val.indexOf(": ")).toInt() + 2, String(val.indexOf(",")).toInt())).toInt();
				Serial.println(CSQ1);
				if (CSQ1 == 99)
				{
					gsm.println(F("AT+CPOWD=1"));
					delay(15000);
					Serial.println("Modem OFF"); //Если нет сети перезагрузить модем
					switchModem();
					InitModem;
				}


			}
		}
		Serial.println(val);  //Печатаем в монитор порта пришедшую строку
		val = "";
	}
}
//--------перезапуск модема--------------------------------------
void switchModem()
{
	digitalWrite(ModemResetPin, LOW);
	delay(1000);
	digitalWrite(ModemResetPin, HIGH);
	delay(3000);
	digitalWrite(ModemResetPin, LOW);
	delay(3000);
}

// ----- Проверка номера-----------------------------------------
uint8_t eepromcheckphone(String CheckPhoneMNumber)
{
	if (String(EEPROM.get(data[0].addr, PhoneNum)) == CheckPhoneMNumber)
	{
		return 1;
	}
	return 0;
}

// ----- Обработка команд----------------------------------------
void MasterCommands()
{
	Serial.println(val);
	val.replace("\r", "");
	val.replace("\n", "");
	val.toLowerCase();
	if ((val.indexOf(F("addphone")) > -1))
	{
		eepromaddphone(NotCheckRingPhone, 0);
	}
	else
	{
		Serial.println("SMS contains wrong command. Ignore.");
	}
}

// ----- Добавление номера----------------------------------------
void eepromaddphone(String AddPhoneNumber, uint8_t Cell)
{
	AddPhoneNumber.toCharArray(PhoneCharArray, PHONENUMBER);
	EEPROM.put(data[Cell].addr, PhoneCharArray);
	delay(100);

	Serial.print("AddPhoneNumber: ");
	Serial.println(AddPhoneNumber);

	Serial.print(F("Phone written :"));
	Serial.println("+" + String(EEPROM.get(data[0].addr, PhoneNum)));
	lineprint();
}

//----------Отправка смс------------------------------------------
void sms(String phone, String smstext)
{  //Процедура отправки СМС
	delay(1000);
	gsm.print(F("AT+CMGS=\""));
	gsm.print(phone);
	gsm.println(F("\""));
	delay(500);
	gsm.print(smstext);
	delay(500);
	gsm.print((char)26);
	delay(500);
	Serial.print("Sent! ");
	Serial.print(phone);
	Serial.print(Marker);
	Serial.println(smstext);
	delay(5000);
}

// ----- Завершение вызова----------------------------------------
void endcall()
{
	delay(2000);
	gsm.println(F("AT+CHUP"));
	delay(1000);
}
//-------Удалить смски--------------------------------------------
void clearsms()
{
	gsm.print(F("AT+CMGD="));
	gsm.println(F("4"));
	//delay(2000);
	Serial.println(F("SMS removed"));
}

//------------прерывание сторожевого таймера----------------------
ISR(WDT_vect)
{
	wake();
} 
//----------------------------------------------------------------
void myWatchdogEnable(const byte interval)
{
	noInterrupts();
	MCUSR = 0;                          // сбрасываем различные флаги
	WDTCSR |= 0b00011000;               // устанавливаем WDCE, WDE
	WDTCSR = 0b01000000 | interval;    // устанавливаем WDIE, и соответсвующую задержку
	wdt_reset();

	byte adcsra_save = ADCSRA;
	ADCSRA = 0;  // запрещаем работу адс
	power_all_disable();   // выключаем все модули
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);   // устанавливаем режим сна
	sleep_enable();
	//attachInterrupt(0, wake, LOW);   // позволяем заземлить pin 2 для выхода из сна
	interrupts();
	sleep_cpu();            // переходим в сон и ожидаем прерывание
	detachInterrupt(0);     // останавливаем прерывание LOW

	ADCSRA = adcsra_save;  // останавливаем понижение питания
	power_all_enable();   // включаем все модули
	wake();
}
void wake()
{
	wdt_disable();  // отключаем сторожевой таймер
}
//-------------ловушка живая---------------------------------------
void leavemodem()
{
	switchModem();
	InitModem();

	sms("+" + String(EEPROM.get(data[0].addr, PhoneNum)), Status);
	delay(60000);

	gsm.println(F("AT+CPOWD=1")); // soft off
	delay(15000);
	Serial.println("Modem OFF");
}
