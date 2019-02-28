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
		json["light"+String(i)] = pcf8574.read(i)==1 ? 0 : 1;
	}

	response.setAllowCrossDomainOrigin("*");
	response.sendDataStream(stream, MIME_JSON);
}

void broadcastPins() 
{
	String message = String(pcf8574.read16());
	for(int i = 0; i < users.activeWebSockets.count(); i++) {
		(*(users.activeWebSockets[i])).broadcast(message.c_str(), message.length());
	}
}

void onSwitchOn(HttpRequest& request, HttpResponse& response)
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

void onSwitchOff(HttpRequest& request, HttpResponse& response)
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

void onToggle(HttpRequest& request, HttpResponse& response)
{
	String text = request.getQueryParameter("pin");
	int number = text.toInt();
	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject& json = stream->getRoot();
		if(pcf8574.read(number) == 1){
			pcf8574.write(number, 0);
		}
		else {
			pcf8574.write(number, 1);
		}
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
	server.addPath("/off", onSwitchOff);
	server.addPath("/toggle", onToggle);
	server.addPath("/status", onStatus);
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
 
  Serial.println("OLOLO");

 
  // pcf8574.write(3,1);
}
