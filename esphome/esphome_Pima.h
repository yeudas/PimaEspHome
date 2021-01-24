#include "esphome.h"
#include "ArduinoQueue.h"

#define	PIMA_STATUS_IDLE				0
#define	PIMA_STATUS_COLLECTING_MESSAGE	1
#define	PIMA_STATUS_CHECK_CRC1			2
#define	PIMA_STATUS_CHECK_CRC2			3

#define NEW_MESSAGE_TIME_SPACE	200  // actually 100ms - but spare is always recommended

//class esphome_pima_component : public Component, CustomAPIDevice {
class esphome_pima_component : public Component, CustomMQTTDevice {
 ArduinoQueue<String> commandQueue;
 ArduinoQueue<String> requests;

 public:
	//Sensor *alarm_status_sensor = new Sensor("Alarm Status");
	std::function<void (uint8_t, bool)> zoneStatusChangeCallback;
	std::function<void (std::string)> alarmStatusChangeCallback;

	const std::string STATUS_UNAVAILABLE = "unavailable";
	const std::string STATUS_ARM   = "armed_away";
	const std::string STATUS_HOME1 = "armed_home";
	const std::string STATUS_HOME2 = "armed_night";
	const std::string STATUS_OFF   = "disarmed";

	unsigned long 	last_time;
	unsigned long 	new_time;
	unsigned char	unlogged_count;

	char			cur_message[259]; // max length 255 + 3 (header+crc)
	unsigned char	cur_message_length;
	unsigned char	collected_bytes;
	unsigned int 	IncomingCalculatedCRC;
	unsigned char	IncomingMessageStatus;


  void setup() override {
	cur_message_length = 0;
	Serial.begin(2400,SERIAL_8N1);
	Serial.setTimeout(1000);
	while(Serial.available()>0)
	{
		Serial.read();
	}
	last_time = millis();
	IncomingMessageStatus = PIMA_STATUS_IDLE;

	subscribe("pima/command/alarm_state", &esphome_pima_component::on_alarm_command);
	subscribe("pima/command/keypad_press", &esphome_pima_component::on_keypad_command);
    //register_service(&esphome_pima_component::setVolume, "linp_set_volume", {"volume"});
  }

 void on_alarm_command(const std::string &payload) {
    if (payload == STATUS_ARM) {
		ESP_LOGD("alarm_command", "Lets arm");
		send_alarm_message(1);
    } else if (payload == STATUS_HOME1) {
		ESP_LOGD("alarm_command", "arm home");
 		send_alarm_message(2);
    }
	else if (payload == STATUS_HOME2) {
		ESP_LOGD("alarm_command", "good night");
 		send_alarm_message(3);
    }
	else if (payload == STATUS_OFF) {
		ESP_LOGD("alarm_command", "welcome home");
 		send_alarm_message(0);
   }
}

 void on_keypad_command(const std::string &payload) {
	if 		(payload=="0")  ESP_LOGD("alarm_keypad", "0");
	else if (payload=="1")  ESP_LOGD("alarm_keypad", "1");
	else if (payload=="2")  ESP_LOGD("alarm_keypad", "2");
	else if (payload=="3")  ESP_LOGD("alarm_keypad", "3");
	else if (payload=="4")  ESP_LOGD("alarm_keypad", "4");
	else if (payload=="5")  ESP_LOGD("alarm_keypad", "5");
	else if (payload=="6")  ESP_LOGD("alarm_keypad", "6");
	else if (payload=="7")  ESP_LOGD("alarm_keypad", "7");
	else if (payload=="8")  ESP_LOGD("alarm_keypad", "8");
	else if (payload=="9")  ESP_LOGD("alarm_keypad", "9");
}

  void loop() override {
	while(Serial.available()>0)
	{
		char c = Serial.read();
		new_time = millis();

		if (IncomingMessageStatus==PIMA_STATUS_IDLE) {
			if (new_time-last_time > NEW_MESSAGE_TIME_SPACE) {
				// new message
				cur_message[0] = c;
				cur_message_length = c;
				collected_bytes = 1;
				//IncomingCalculatedCRC = 0;
				IncomingCalculatedCRC = UpdateCRC(0,c);
				IncomingMessageStatus = PIMA_STATUS_COLLECTING_MESSAGE;
			}
		}

		else if (IncomingMessageStatus==PIMA_STATUS_COLLECTING_MESSAGE){
			unsigned char ucBidx;
			cur_message[collected_bytes] = c;
			collected_bytes += 1;
			IncomingCalculatedCRC = UpdateCRC(IncomingCalculatedCRC,c);
			if (collected_bytes-1==cur_message_length) {
				IncomingMessageStatus = PIMA_STATUS_CHECK_CRC1;
			}
		}

		else if (IncomingMessageStatus==PIMA_STATUS_CHECK_CRC1){
			cur_message[collected_bytes] = c;
			IncomingMessageStatus = PIMA_STATUS_CHECK_CRC2;
		}

		else if (IncomingMessageStatus==PIMA_STATUS_CHECK_CRC2){
			int i;
			IncomingMessageStatus = PIMA_STATUS_IDLE;

			cur_message[collected_bytes+1] = c;
			if (cur_message[collected_bytes+1] + (cur_message[collected_bytes]<<8) == IncomingCalculatedCRC) { // CRC OK
				parse_message();
			}
			else {
				// BAD_CRC
			}
		}

		last_time = millis();
	}
  }


void parse_message()
{
	ESP_LOGD("message parser", "length(%d) : %x %x %x %x %x %x %x %x %x %x",cur_message_length,cur_message[0],cur_message[1],cur_message[2],cur_message[3],cur_message[4],cur_message[5],cur_message[6],cur_message[7],cur_message[8],cur_message[9]);
	
	if ((cur_message[0]==8) and (cur_message[2]==5) and (cur_message[3]==0)) {
		send_login_message();
	}
	else {
		send_idle_message();
		unlogged_count = 3;
	}

	if ((cur_message[0]==8) and (cur_message[2]==5) and (cur_message[3]==0)) {
		if (unlogged_count>0) {
			unlogged_count -= 1;
		} else {
			alarmStatusChangeCallback(STATUS_UNAVAILABLE);
		}
	}
	else if ((cur_message[0]==98) and (cur_message[2]==5) and (cur_message[3]==1)) {
		uint8_t 	  cur_bit;
		uint8_t 	  cur_byte;
		uint8_t 	  cur_sensor;
		unsigned char cur_char;
		
		// Alarm status
		switch (cur_message[55]) {
			case 0: alarmStatusChangeCallback(STATUS_OFF); break;
			case 1: alarmStatusChangeCallback(STATUS_ARM); break;
			case 2: alarmStatusChangeCallback(STATUS_HOME1); break;
			case 3: alarmStatusChangeCallback(STATUS_HOME2); break;
		}

		// open zones
		cur_sensor = 1;
		for (cur_byte=7 ; cur_byte<19 ; cur_byte++) {
			cur_char = cur_message[cur_byte];
			for (cur_bit=0 ; cur_bit<8 ; cur_bit++) {
				zoneStatusChangeCallback(cur_sensor, (cur_char & (unsigned char)0x01) == (unsigned char)0x01 );
				cur_char >>= 1;
				cur_sensor+= 1;
			}
		}
	}
}

void send_idle_message()
{
	unsigned int crc=0;
	
	crc = send_byte(0  ,0x4);
	crc = send_byte(crc,0xd);
	crc = send_byte(crc,0x5);
	crc = send_byte(crc,0x0);
	crc = send_byte(crc,0x0);

	send_byte(0,crc>>8);
	send_byte(0,crc&0xff);
}


void send_login_message()
{
	unsigned int crc=0;

	crc = send_byte(0  ,0xa);	// length
	crc = send_byte(crc,0xd);   // 
	crc = send_byte(crc,0xf);  // write
	crc = send_byte(crc,0x4);	// 4 insert you technician password length
	crc = send_byte(crc,0x0);	// 
	crc = send_byte(crc,0x1);	// 1  insert you actual technician password - this example is for 1234
	crc = send_byte(crc,0x2);	// 2
	crc = send_byte(crc,0x3);	// 3
	crc = send_byte(crc,0x4);	// 4
	crc = send_byte(crc,0xff);	// ff
	crc = send_byte(crc,0xff);	// ff

	send_byte(0,crc>>8);
	send_byte(0,crc&0xff);
}

void send_alarm_message(unsigned char alarm_mode)
{
	unsigned int crc=0;

	crc = send_byte(0  ,0x7);	// length
	crc = send_byte(crc,0xd);   // 
	if (alarm_mode==0) 
		crc = send_byte(crc,0x1);  // message=open
	else
		crc = send_byte(crc,0x19);  // message=close
		
	crc = send_byte(crc,0x1);	//
	crc = send_byte(crc,0x2);	// 
	crc = send_byte(crc,0x1);	// partition low
	crc = send_byte(crc,0x0);	// partition high
	crc = send_byte(crc,alarm_mode);	// 1=full alarm, 2=home1, 3=home2, 0=disarm

	send_byte(0,crc>>8);
	send_byte(0,crc&0xff);
}


unsigned int send_byte(unsigned int CurrentCRC, char NewChar) {
	unsigned char ucBidx;

	Serial.write(NewChar);
	
	for (ucBidx = 0; ucBidx < 8; ucBidx++){
	  NewChar ^= (CurrentCRC &1);
	  CurrentCRC >>= 1;
	  if (NewChar &1)
	  CurrentCRC ^= 0xA001;
	  NewChar >>= 1;
	}

	return CurrentCRC;
}

unsigned int UpdateCRC(unsigned int CurrentCRC, char NewChar) {
	unsigned char ucBidx;

	for (ucBidx = 0; ucBidx < 8; ucBidx++){
	  NewChar ^= (CurrentCRC &1);
	  CurrentCRC >>= 1;
	  if (NewChar &1)
	  CurrentCRC ^= 0xA001;
	  NewChar >>= 1;
	}

	return CurrentCRC;
}

/**
   * Zone status changed.
   * 
   * @param callback      callback to notify
   * @param zone          zone number [0..MAX]
   * @param isOpen        true if zone open (disturbed), false if zone closed
   */
  void onZoneStatusChange(std::function<void (uint8_t zone, bool isOpen)> callback) { zoneStatusChangeCallback = callback; }

  /**
   * Alarm status changed.
   * 
   * @param callback      callback to notify
   * @param statusCode    status code
   */
  void onAlarmStatusChange(std::function<void (std::string status)> callback) { alarmStatusChangeCallback = callback; }


};
