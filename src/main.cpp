#include <WiFi.h>
#include "DHT.h"

// =================== Réglages Wi-Fi ===================
const char* ssid     = "LAPTOP-FHJBJ2FC 2173";  
const char* password = "0r725[6N";              

// --- DHT ---
#define DHTPIN   13
#define DHTTYPE  DHT11
DHT dht(DHTPIN, DHTTYPE);

// --- LED RGB ---
#define COMMON_ANODE 1      // 1 = anode commune
#define PIN_R 27            // Rouge
#define PIN_G 26            // Vert
#define PIN_B 25            // Bleu

// PWM LEDC
const int LEDC_FREQ = 5000;
const int LEDC_RES  = 8;
const int CH_R = 0, CH_G = 1, CH_B = 2;

// État LED
uint8_t curR=0, curG=0, curB=0;
bool ledOn = true;        // LED allumée ou éteinte
int colorValue = 0;       // valeur de la jauge 0–255

// Serveur Web
WiFiServer server(80);

// =================== Helpers LED ===================
inline uint8_t maybeInvert(uint8_t x){ return COMMON_ANODE ? (255 - x) : x; }

void setRGB(uint8_t r,uint8_t g,uint8_t b){
  curR=r; curG=g; curB=b;
  if (ledOn) {
    ledcWrite(CH_R, maybeInvert(r));
    ledcWrite(CH_G, maybeInvert(g));
    ledcWrite(CH_B, maybeInvert(b));
  } else {
    ledcWrite(CH_R, maybeInvert(0));
    ledcWrite(CH_G, maybeInvert(0));
    ledcWrite(CH_B, maybeInvert(0));
  }
}

void initRGB(){
  ledcSetup(CH_R, LEDC_FREQ, LEDC_RES);
  ledcSetup(CH_G, LEDC_FREQ, LEDC_RES);
  ledcSetup(CH_B, LEDC_FREQ, LEDC_RES);
  ledcAttachPin(PIN_R, CH_R);
  ledcAttachPin(PIN_G, CH_G);
  ledcAttachPin(PIN_B, CH_B);
  setRGB(0,0,0);
}

// conversion jauge → couleur RGB
void setColorFromValue(int val) {
  colorValue = val % 256;
  int region = colorValue / 85;
  int remainder = colorValue % 85;

  switch(region) {
    case 0: // rouge -> vert
      setRGB(255 - remainder*3, remainder*3, 0);
      break;
    case 1: // vert -> bleu
      setRGB(0, 255 - remainder*3, remainder*3);
      break;
    case 2: // bleu -> rouge
      setRGB(remainder*3, 0, 255 - remainder*3);
      break;
  }
}

// utilitaires HTTP
int getParamI(const String& src,const char* key,int deflt){
  String k=String(key)+"="; int i=src.indexOf(k); if(i<0) return deflt; i+=k.length();
  int j=src.indexOf('&',i); if(j<0) j=src.indexOf(' ',i); if(j<0) j=src.length();
  return constrain(src.substring(i,j).toInt(),0,255);
}
bool startsWithPath(const String& req,const char* path){ String p=String("GET ")+path; return req.startsWith(p); }

// =================== Wi-Fi ===================
void initWiFi() {
  WiFi.mode(WIFI_STA);                 
  WiFi.begin(ssid, password);          
  Serial.println("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println();
  Serial.println(WiFi.localIP());      
  Serial.print("RSSI: "); 
  Serial.println(WiFi.RSSI());         
}

// =================== Pages Web ===================
void sendHtml(WiFiClient& client, float h, float t){
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html; charset=utf-8");
  client.println("Connection: close");
  client.println();
  client.println("<!doctype html><html><head><meta charset='utf-8'><title>ESP32</title></head><body>");
  client.println("<h1>ESP32 • DHT11 + LED RGB</h1>");
  
  if(isnan(h)||isnan(t)){
    client.println("<p><b>Erreur capteur DHT</b></p>");
  }else{
    client.print("<p>Humidité : "); client.print(h,1); client.println(" %</p>");
    client.print("<p>Température : "); client.print(t,1); client.println(" °C</p>");
  }

  client.print("<p>État LED : <b>");
  client.print(ledOn ? "allumée" : "éteinte");
  client.println("</b></p>");
  client.println("<p><a href='/toggle'>Allumer/Éteindre</a></p>");

  client.print("<p>Couleur actuelle : R="); client.print(curR);
  client.print(" G="); client.print(curG);
  client.print(" B="); client.print(curB);
  client.println("</p>");

  client.println("<h3>Changer la couleur avec la jauge</h3>");
  client.println("<form action='/color' method='get'>");
  client.println("<input type='range' name='val' min='0' max='255' value='");
  client.print(colorValue);
  client.println("' oninput='this.nextElementSibling.value=this.value'>");
  client.print("<output>"); client.print(colorValue); client.println("</output>");
  client.println("<br><input type='submit' value='Appliquer'>");
  client.println("</form>");

  client.println("</body></html>");
}

// =================== Programme principal ===================
void setup(){
  Serial.begin(115200);
  dht.begin();
  initWiFi();
  initRGB();
  server.begin();
  Serial.println("HTTP server started (port 80).");
}

void loop(){
  WiFiClient client = server.available();
  if(!client) return;
  unsigned long t0=millis();
  while(client.connected() && !client.available() && millis()-t0<2000){ delay(1); }
  if(!client.available()){ client.stop(); return; }

  String req=client.readStringUntil('\r'); 
  while(client.available()) client.read();
  
  float h=dht.readHumidity(), T=dht.readTemperature();
  
  if(startsWithPath(req,"/toggle")) {
    ledOn = !ledOn;
    setRGB(curR, curG, curB);
    sendHtml(client,h,T);
  }
  else if(startsWithPath(req,"/color")) {
    int val = getParamI(req,"val",colorValue);
    setColorFromValue(val);
    sendHtml(client,h,T);
  }
  else sendHtml(client,h,T);

  client.stop();
}
