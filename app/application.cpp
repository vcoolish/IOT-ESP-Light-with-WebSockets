#include <user_config.h>
#include <SmingCore/SmingCore.h>
#include "pcf8574_esp.h"
#include "Data/Stream/TemplateFlashMemoryStream.h"
#include <SmingCore/Network/Http/Websocket/WebsocketResource.h>
#include "CUserData.h"

#define SDA_PIN 5
#define SCL_PIN 4
#define INTERRUPT_PIN 5

TwoWire testWire;
HttpServer server;

int totalActiveSockets = 0;

Timer procTimer;

volatile uint8_t state = 0;
volatile uint8_t timerFlag = 0;
volatile uint8_t timer = 0;

PCF857x pcf8574(0x20, &testWire, true);

CUserData users("", "");

#define ROM_0_URL "http://192.168.1.20:8000/rom0.bin"
#define ROM_1_URL "http://192.168.1.20:8000/rom1.bin"
#define SPIFFS_URL "http://192.168.1.20:8000/spiff_rom.bin"

#ifndef WIFI_SSID
#define WIFI_SSID "TitaniumLabs-2G"
#define WIFI_PWD "yodamaster"
#endif


void togglePin(uint8_t number);
uint8_t get_status_on_line(uint8_t number);

rBootHttpUpdate* otaUpdater = 0;

void OtaUpdate_CallBack(rBootHttpUpdate& client, bool result)
{
	Serial.println("In callback...");
	if(result == true) {
		// success
		uint8 slot;
		slot = rboot_get_current_rom();
		if(slot == 0)
			slot = 1;
		else
			slot = 0;
		// set to boot new rom and then reboot
		Serial.printf("Firmware updated, rebooting to rom %d...\r\n", slot);
		rboot_set_current_rom(slot);
		System.restart();
	} else {
		// fail
		Serial.println("Firmware update failed!");
	}
}

void OtaUpdate()
{
	uint8 slot;
	rboot_config bootconf;

	Serial.println("Updating...");

	// need a clean object, otherwise if run before and failed will not run again
	if(otaUpdater)
		delete otaUpdater;
	otaUpdater = new rBootHttpUpdate();

	// select rom slot to flash
	bootconf = rboot_get_config();
	slot = bootconf.current_rom;
	if(slot == 0)
		slot = 1;
	else
		slot = 0;

#ifndef RBOOT_TWO_ROMS
	// flash rom to position indicated in the rBoot config rom table
	otaUpdater->addItem(bootconf.roms[slot], ROM_0_URL);
#else
	// flash appropriate rom
	if(slot == 0) {
		otaUpdater->addItem(bootconf.roms[slot], ROM_0_URL);
	} else {
		otaUpdater->addItem(bootconf.roms[slot], ROM_1_URL);
	}
#endif

#ifndef DISABLE_SPIFFS
	// use user supplied values (defaults for 4mb flash in makefile)
	if(slot == 0) {
		otaUpdater->addItem(RBOOT_SPIFFS_0, SPIFFS_URL);
	} else {
		otaUpdater->addItem(RBOOT_SPIFFS_1, SPIFFS_URL);
	}
#endif

	// request switch and reboot on success
	//otaUpdater->switchToRom(slot);
	// and/or set a callback (called on failure or success without switching requested)
	otaUpdater->setCallback(OtaUpdate_CallBack);

	// start update
	otaUpdater->start();
}

void onUpdate(HttpRequest& request, HttpResponse& response)
{
	OtaUpdate();
}

void onIndex(HttpRequest& request, HttpResponse& response)
{
	TemplateFileStream* tmpl = new TemplateFileStream("index.html");
	response.sendTemplate(tmpl); // will be automatically deleted
}

void onStatus(HttpRequest& request, HttpResponse& response)
{
	//TemplateFileStream* tmpl = new TemplateFileStream("index.html");
	//auto& vars = tmpl->variables();
	//response.sendTemplate(tmpl); // will be automatically deleted
	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject& json = stream->getRoot();

	for(int i=0; i <= 7; i++){
		json["light"+String(i)] = get_status_on_line(i);
	}

	response.setAllowCrossDomainOrigin("*");
	response.sendDataStream(stream, MIME_JSON);
}

void broadcastPins() 
{
	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject& json = stream->getRoot();
	for(int i=0; i <= 7; i++){
		json["light"+String(i)] = get_status_on_line(i);
	}
	String msg = "";
	json.printTo(msg);
	// for(int i = 0; i < users.activeWebSockets.count(); i++) {
	// 	(*(users.activeWebSockets[i])).broadcast(message.c_str(), message.length());
	// }
	if(users.activeWebSockets.count() > 0) {
		(*(users.activeWebSockets[0])).broadcast(msg.c_str(), msg.length());
	}
}

void onSwitchOnRelay(HttpRequest& request, HttpResponse& response)
{
	String text = request.getQueryParameter("pin");
	int number = text.toInt();
	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject& json = stream->getRoot();
	pcf8574.write(number, 1);
	json["changed"] = "true";
	
	response.setAllowCrossDomainOrigin("*");
	response.sendDataStream(stream, MIME_JSON);
	broadcastPins();
}

void onSwitchOffRelay(HttpRequest& request, HttpResponse& response)
{
	String text = request.getQueryParameter("pin");
	int number = text.toInt();
	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject& json = stream->getRoot();

	pcf8574.write(number, 0);
	json["changed"] = "true";
		
	
	response.setAllowCrossDomainOrigin("*");
	response.sendDataStream(stream, MIME_JSON);
	broadcastPins();
}

void onSwitchOn(HttpRequest& request, HttpResponse& response)
{
	String text = request.getQueryParameter("pin");
	int number = text.toInt();
	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject& json = stream->getRoot();

	if(number == 15){
		uint8_t pin_status = get_status_on_line(7);
		if (pin_status == 1) {
 		
		} else {
			togglePin((uint8_t)number);
		}
		json["changed"] = "true";
	} else 	if(number == 14){
		uint8_t pin_status = get_status_on_line(6);
		if (pin_status == 1) {
 		
		} else {
			togglePin((uint8_t)number);
		}
		json["changed"] = "true";
	} else	if(number == 13){
		uint8_t pin_status = get_status_on_line(5);
		if (pin_status == 1) {
 		
		} else {
			togglePin((uint8_t)number);
		}
		json["changed"] = "true";
	} else	if(number == 12){
		uint8_t pin_status = get_status_on_line(4);
		if (pin_status == 1) {
 		
		} else {
			togglePin((uint8_t)number);
		}
		json["changed"] = "true";
	} else {

	}
	
	response.setAllowCrossDomainOrigin("*");
	response.sendDataStream(stream, MIME_JSON);
	broadcastPins();
}

void onSwitchOff(HttpRequest& request, HttpResponse& response)
{
	String text = request.getQueryParameter("pin");
	int number = text.toInt();
	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject& json = stream->getRoot();

	if(number == 15){
		uint8_t pin_status = get_status_on_line(7);
		if (pin_status == 0) {
 		
		} else {
			togglePin((uint8_t)number);
		}
		json["changed"] = "true";
	} else 	if(number == 14){
		uint8_t pin_status = get_status_on_line(6);
		if (pin_status == 0) {
 		
		} else {
			togglePin((uint8_t)number);
		}
		json["changed"] = "true";
	} else	if(number == 13){
		uint8_t pin_status = get_status_on_line(5);
		if (pin_status == 0) {
 		
		} else {
			togglePin((uint8_t)number);
		}
		json["changed"] = "true";
	} else	if(number == 12){
		uint8_t pin_status = get_status_on_line(4);
		if (pin_status == 0) {
 		
		} else {
			togglePin((uint8_t)number);
		}
		json["changed"] = "true";
	} else {

	}
	
	response.setAllowCrossDomainOrigin("*");
	response.sendDataStream(stream, MIME_JSON);
	broadcastPins();
}

uint8_t get_status_on_line(uint8_t number) {
	uint8_t value;
	if(pcf8574.read(number) == 1) {
		value = 0;
	}
	else {
		value = 1;
	}
	return value;
}


void togglePin(uint8_t number) {
	if(pcf8574.read(number) == 1){
		pcf8574.write(number, 0);		
	}
	else {
		pcf8574.write(number, 1);
	}
}

void onToggle(HttpRequest& request, HttpResponse& response)
{
	String text = request.getQueryParameter("pin");
	int number = text.toInt();
	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject& json = stream->getRoot();

	togglePin((uint8_t)number);

	json["changed"] = "true";
		
	response.setAllowCrossDomainOrigin("*");
	response.sendDataStream(stream, MIME_JSON);
	broadcastPins();
}

void IRAM_ATTR PCFInterrupt()
{  
	Serial.println(pcf8574.read16(), HEX);
	broadcastPins();
}

void wsConnected(WebsocketConnection& socket)
{
	totalActiveSockets++;
	users.addSession(socket);
}

void wsMessageReceived(WebsocketConnection& socket, const String& message)
{
	Serial.printf("WebSocket message received:\r\n%s\r\n", message.c_str());

	if(message == "shutdown") {
		String message = "The server is shutting down...";
		socket.broadcast(message.c_str(), message.length());
		server.shutdown();
		return;
	}

	String response = "Echo: " + message;
	socket.sendString(response);

	//Normally you would use dynamic cast but just be careful not to convert to wrong object type!
	// CUserData* user = (CUserData*)socket.getUserData();
	// if(user) {
	// 	user->printMessage(socket, message);
	// }
}

void wsBinaryReceived(WebsocketConnection& socket, uint8_t* data, size_t size)
{
	Serial.printf("Websocket binary data recieved, size: %d\r\n", size);
}

void wsDisconnected(WebsocketConnection& socket)
{
	totalActiveSockets--;

	// Normally you would use dynamic cast but just be careful not to convert to wrong object type!
	CUserData* user = (CUserData*)socket.getUserData();
	if(user) {
		user->removeSession(socket);
	}

	// Notify everybody about lost connection
	String message = "We lost our friend :( Total: " + String(totalActiveSockets);
	socket.broadcast(message.c_str(), message.length());
}


void onFile(HttpRequest& request, HttpResponse& response)
{
	String file = request.uri.Path;
	if(file[0] == '/')
		file = file.substring(1);

	if(file[0] == '.')
		response.code = HTTP_STATUS_FORBIDDEN;
	else {
		response.setCache(86400, true); // It's important to use cache for better performance.
		response.sendFile(file);
	}
}

void startWebServer()
{
	server.listen(80);
	server.addPath("/", onIndex);
	server.addPath("/on", onSwitchOn);
	server.addPath("/ron", onSwitchOnRelay);
	server.addPath("/off", onSwitchOff);
	server.addPath("/roff", onSwitchOffRelay);
	server.addPath("/toggle", onToggle);
	server.addPath("/status", onStatus);
	server.addPath("/update", onUpdate);
	server.setDefaultHandler(onFile);
}

void startServers()
{
	//startFTP();
	startWebServer();

	// Web Sockets configuration
	WebsocketResource* wsResource = new WebsocketResource();
	wsResource->setConnectionHandler(wsConnected);
	wsResource->setMessageHandler(wsMessageReceived);
	wsResource->setBinaryHandler(wsBinaryReceived);
	wsResource->setDisconnectionHandler(wsDisconnected);

	server.addPath("/ws", wsResource);
}
void gotIP(IPAddress ip, IPAddress netmask, IPAddress gateway)
{
	Serial.println("I'm CONNECTED");

}

void onInitTimer() {
	if( timerFlag == 0 ) {
		timerFlag++;
		uint16_t temp = pcf8574.read16();
		state =  temp;
		uint16_t stateTemp = (state << 8) + 0xFF;
		pcf8574.write16(stateTemp);
	}

	// else {
	// 	uint16_t temp = pcf8574.read16();
	// 	state =  (uint8_t)temp;
	// 	Serial.println(state, BIN);
	// }
}

void Switch()
{
	uint8 before, after;
	before = rboot_get_current_rom();
	if(before == 0)
		after = 1;
	else
		after = 0;
	Serial.printf("Swapping from rom %d to rom %d.\r\n", before, after);
	rboot_set_current_rom(after);
	Serial.println("Restarting...\r\n");
	System.restart();
}

void ShowInfo()
{
	Serial.printf("\r\nSDK: v%s\r\n", system_get_sdk_version());
	Serial.printf("Free Heap: %d\r\n", system_get_free_heap_size());
	Serial.printf("CPU Frequency: %d MHz\r\n", system_get_cpu_freq());
	Serial.printf("System Chip ID: %x\r\n", system_get_chip_id());
	Serial.printf("SPI Flash ID: %x\r\n", spi_flash_get_id());
	//Serial.printf("SPI Flash Size: %d\r\n", (1 << ((spi_flash_get_id() >> 16) & 0xff)));

	rboot_config conf;
	conf = rboot_get_config();

	debugf("Count: %d", conf.count);
	debugf("ROM 0: %d", conf.roms[0]);
	debugf("ROM 1: %d", conf.roms[1]);
	debugf("ROM 2: %d", conf.roms[2]);
	debugf("GPIO ROM: %d", conf.gpio_rom);
}

void serialCallBack(Stream& stream, char arrivedChar, unsigned short availableCharsCount)
{
	int pos = stream.indexOf('\n');
	if(pos > -1) {
		char str[pos + 1];
		for(int i = 0; i < pos + 1; i++) {
			str[i] = stream.read();
			if(str[i] == '\r' || str[i] == '\n') {
				str[i] = '\0';
			}
		}

		if(!strcmp(str, "connect")) {
			// connect to wifi
			WifiStation.config(WIFI_SSID, WIFI_PWD);
			WifiStation.enable(true);
			WifiStation.connect();
		} else if(!strcmp(str, "ip")) {
			Serial.printf("ip: %s mac: %s\r\n", WifiStation.getIP().toString().c_str(), WifiStation.getMAC().c_str());
		} else if(!strcmp(str, "ota")) {
			OtaUpdate();
		} else if(!strcmp(str, "switch")) {
			Switch();
		} else if(!strcmp(str, "restart")) {
			System.restart();
		} else if(!strcmp(str, "ls")) {
			Vector<String> files = fileList();
			Serial.printf("filecount %d\r\n", files.count());
			for(unsigned int i = 0; i < files.count(); i++) {
				Serial.println(files[i]);
			}
		} else if(!strcmp(str, "cat")) {
			Vector<String> files = fileList();
			if(files.count() > 0) {
				Serial.printf("dumping file %s:\r\n", files[0].c_str());
				Serial.println(fileGetContent(files[0]));
			} else {
				Serial.println("Empty spiffs!");
			}
		} else if(!strcmp(str, "info")) {
			ShowInfo();
		} else if(!strcmp(str, "help")) {
			Serial.println();
			Serial.println("available commands:");
			Serial.println("  help - display this message");
			Serial.println("  ip - show current ip address");
			Serial.println("  connect - connect to wifi");
			Serial.println("  restart - restart the esp8266");
			Serial.println("  switch - switch to the other rom and reboot");
			Serial.println("  ota - perform ota update, switch rom and reboot");
			Serial.println("  info - show esp8266 info");
#ifndef DISABLE_SPIFFS
			Serial.println("  ls - list files in spiffs");
			Serial.println("  cat - show first file in spiffs");
#endif
			Serial.println();
		} else {
			Serial.println("unknown command");
		}
	}
}

void init() {
	spiffs_mount(); // Mount file system, in order to work with files
	Serial.begin(SERIAL_BAUD_RATE); // 115200 by default
	Serial.systemDebugOutput(true); // Enable debug output to serial
	//AppSettings.load();

	WifiStation.config("TitaniumLabs-2G", "yodamaster");
	WifiStation.enable(true);
	WifiAccessPoint.enable(false);

	// Run WEB server on system ready
	WifiEvents.onStationGotIP(gotIP);
	System.onReady(startServers);

  Wire.begin();

  //Serial.begin(115200);
  Serial.println("Firing up...");
 
  pcf8574.begin();
 // digitalWrite(INTERRUPT_PIN, HIGH);
  attachInterrupt(INTERRUPT_PIN, PCFInterrupt, CHANGE);

  pinMode(INTERRUPT_PIN, INPUT_PULLUP);

  pcf8574.write16(0xFFFF);

  procTimer.initializeMs(1000, onInitTimer).start();

	// mount spiffs
	int slot = rboot_get_current_rom();
#ifndef DISABLE_SPIFFS
	if(slot == 0) {
#ifdef RBOOT_SPIFFS_0
		debugf("trying to mount spiffs at 0x%08x, length %d", RBOOT_SPIFFS_0, SPIFF_SIZE);
		spiffs_mount_manual(RBOOT_SPIFFS_0, SPIFF_SIZE);
#else
		debugf("trying to mount spiffs at 0x%08x, length %d", 0x100000, SPIFF_SIZE);
		spiffs_mount_manual(0x100000, SPIFF_SIZE);
#endif
	} else {
#ifdef RBOOT_SPIFFS_1
		debugf("trying to mount spiffs at 0x%08x, length %d", RBOOT_SPIFFS_1, SPIFF_SIZE);
		spiffs_mount_manual(RBOOT_SPIFFS_1, SPIFF_SIZE);
#else
		debugf("trying to mount spiffs at 0x%08x, length %d", 0x300000, SPIFF_SIZE);
		spiffs_mount_manual(0x300000, SPIFF_SIZE);
#endif
	}
#else
	debugf("spiffs disabled");
#endif
	Serial.printf("\r\nCurrently running rom %d.\r\n", slot);
	Serial.println("Type 'help' and press enter for instructions.");

	Serial.setCallback(serialCallBack);
}
