/*
PINES DE ETHERNET SHIELD 
ARDUINO ONE
10 (SELECCIOIN DE SHIELD)
4 (SD)
11,12,13 (SPI/ICSP)

ARDUINO MEGA
10 (SELECCIOIN DE SHIELD)
4 (SD)
50,51,52 (SPI/ICSP)
53 (SS)



rest.function("getSensorTemp",getSensorTemp);
rest.function("setFermentadorTemp",setFermentadorTemp);
rest.function("getFermentadorTemp",getFermentadorTemp);

// Consulta de temperatura a sensor
// http://192.168.0.108/getSensorTemp?params=X

// Seteo de temperaturas
http://192.168.0.108/setFermentadorTemp?params=F,T (Donde F = Numero de fermentador y T = Temperatura maxima)

// Consulta de seteo de temperatura en fermentador
http://192.168.0.108/getFermentadorTemp?params=X
*/
//Se importan las librerías
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal.h>
#include <SPI.h>
#include <Ethernet.h>
#include <aREST.h>
#include <avr/wdt.h>

// Defino pines para LCD
LiquidCrystal lcd(12,11,5,4,3,2);

////////////////////////////////////////////// INICIO PARAMETROS PARA SENSORES DS ////////////////////////////////////////////////////
// Defino pin para sensores DS
#define ONE_WIRE_BUS 9
// Cantidad de sensores conectados
#define cantidadSensores 2
// Defino Precision de lectura
#define TEMPERATURE_PRECISION 10

// Almacenado de MAC de cada sensor descubierto
byte sensoresTemp[cantidadSensores][8];

// Guardado de nuemero total de dispositivos descubiertos
byte totalSensores; 

// Instacio OneWire para todos los dispositivos en el bus
OneWire OneWire(ONE_WIRE_BUS);

// Paso como referencia el bus a la lib Dallas
DallasTemperature sensors(&OneWire);
////////////////////////////////////////////// FIN PARAMETROS PARA SENSORES DS ////////////////////////////////////////////////////

////////////////////////////////////////////// INICIO PARAMETROS RED Y API REST ////////////////////////////////////////////////////
// DEFINO MAC DE ETHERNET
byte mac[] = { 0x90, 0xA2, 0xDA, 0x0E, 0xFE, 0x40 };

// SETEO IP EN CASO DE FALLA DHCP
IPAddress ip(192,168,0,177);

// DEFINO PUERTO DE ESCUCHA
EthernetServer server(80);

// CREO INSTANCIA AREST
aREST rest = aREST();

////////////////////////////////////////////// FIN PARAMETROS RED Y API REST ////////////////////////////////////////////////////

///////////////////////////////////////////////////// VARIABLES GLOBALES//////////////////////////////////////////////////////////////////////////

//Variables de lectura para Temperatura de sensores
float temperatura[cantidadSensores];

//Variables para almacenar temperaturas fijadas para fermentadores 1 y 2
int temperaturaSeteada[cantidadSensores];

//Defino pines para electro valvulas y bomba
int bombaPin[cantidadSensores] = {14,15};

//Estados de pines de relees para electro bombas
int bombaEstado[cantidadSensores] = {LOW,LOW};

//Variables para control de encendido de bombas, espera tiempo minimo de espera para encendido(5 minutos)
//long intervaloEncendidoBombas = 500000;
long intervaloEncendidoBombas = 500;

///Almaceno timestamp de encendido anterior
long intervaloEncendidoPrevBomba_0 = 0;
long intervaloEncendidoPrevBomba_1 = 0;

//Intervalo minimo para tomar cmdTemperatura nuevamente(30 segundos)
long tempIntervaloSensado = 30000;
long tempIntervaloSensadoPrev = 0;

// Intervalo de espera para LCD
unsigned long intervaloLCDPrint = 1500;
unsigned long intervaloLCDPrintPrev = 0;
unsigned long intervaloLCDScrollPrev = 0;


void setup(void){
  Serial.begin(9600);
  
  // Inicializo los sensores  
  sensors.begin();
  // Busca y guarda todas las mac de los sensores en allAddress array 
  totalSensores = discoverOneWireDevices();         
  
  // Seteo resolucion en sensores, temperatura por default de fermentadores, declaro pines en modo salida y seteo el estado de los 
  //mismos al inicio del arduino
  for (byte i=0; i < totalSensores; i++) {
    sensors.setResolution(sensoresTemp[i], 10);
    temperaturaSeteada[i] = 99;
    pinMode(bombaPin[i],OUTPUT);
    Serial.println("Pines seteados en modo salida");
    bombaEstado[i] = LOW;

    }
  /*
  // Seteo todos los fermentadores en 99 para evitar el arranque de bombas
  for (byte i=0; i < totalSensores; i++)
    temperaturaSeteada[i] = 99;

  // Inicio los pines de las bombas en apagado para evitar que arranquen durante el inicio
  for (byte i=0; i < totalSensores; i++)
    bombaEstado[i] = LOW;
  */
  // Inicializo LCD
  lcd.begin(16,2);

  // REST
  // Asigno ID y Nombre a Arduino
  rest.set_id("001");
  rest.set_name("Barfuino");

  //rest.variable(esta,&temperaturas[i]);
  rest.function("getSensorTemp",getSensorTemp);
  rest.function("setFermentadorTemp",setFermentadorTemp);
  rest.function("getFermentadorTemp",getFermentadorTemp);

  // ARRANCO RED Y SERVIDOR
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // no point in carrying on, so do nothing forevermore:
    // try to congifure using IP address instead of DHCP:
    Ethernet.begin(mac, ip);
  }
  server.begin();
  Serial.print("El servidor escucha en la IP: ");
  Serial.println(Ethernet.localIP());

  // Start watchdog
  wdt_enable(WDTO_4S);

}

void loop(void){

////////////////////////////////////////////// VARIABLES LOCALES DE LOOP()////////////////////////////////////////////////////////////////////////////
  iniciarRed();
  // Realiza la lectura de temperatura de todos los sensores cada 2 mins y son guardadas en array temperatura
  sensarTemperatura();
  
  //Empiezo el control de cmdTemperatura segun temps comparando temperaturas en sensores 1 y 2 con las fijadas en fermNum1 y fermNum2
  controlarTemps();
 
  // Muestro datos por LCD
  escrituraLCD();

  delay(1000);
  
  Serial.print("Temperatura de Sensor 0: ");
  Serial.println(temperatura[0]);
  Serial.print("Temperatura de Sensor 1: ");
  Serial.println(temperatura[1]);
  Serial.println("");
}

/////////////////////////////////////////////////////////////////////FUNCIONES///////////////////////////////////////////////////////////////////////


// Funcion para imprimir temperaturas
void imprimirTemperatura(DeviceAddress deviceAddress){
    float tempC = sensors.getTempC(deviceAddress);
    Serial.println(tempC);
}

long imprimirTemperaturaLcd(DeviceAddress deviceAddress){
  float tempC = sensors.getTempC(deviceAddress);
  
  return(tempC);
}

// Funcion para recuperar temperaturas sobre sensor
long recuperarTemperatura(DeviceAddress deviceAddress){
    float tempC = sensors.getTempC(deviceAddress);
    
    return tempC;
}

// Funcion para busqueda de dispositivos OneWire
byte discoverOneWireDevices() {
  byte j=0;                                        
  //Busca sensores y agrega la mac al array
  while ((j < cantidadSensores) && (OneWire.search(sensoresTemp[j]))) {        
    j++;
  }
  for (byte i=0; i < j; i++) {
    Serial.print("Sensor ");
    Serial.print(i);  
    Serial.print(": ");                          
    // Imprime macs de sensores encontrados 
    printAddress(sensoresTemp[i]);                  
  }
  Serial.print("\r\n");
  
  // Devuelve el total de dispositivos encontrados
  return j;                 
}

// Imprime direcciones de sensores descubiertos
void printAddress(DeviceAddress addr) {
  byte i;
  for( i=0; i < 8; i++) {                         // prefix the printout with 0x
      Serial.print("0x");
      if (addr[i] < 16) {
        Serial.print('0');                        // add a leading '0' if required.
      }
      Serial.print(addr[i], HEX);                 // print the actual value in HEX
      if (i < 7) {
        Serial.print(", ");
      }
    }
  Serial.print("\r\n");
} 


// Funcion para el control de temperatura
void controlarTemps(){
  long intervaloEncendidoActual = millis();

  //Cuando la cmdTemperatura del fermentador supere la seteada en temperaturaSeteada[x]
  // durante el intervalo seteado en (intervaloEncendidoBombas) se activa las bomba
  if (temperatura[0]> temperaturaSeteada[0] && intervaloEncendidoActual - intervaloEncendidoPrevBomba_0 > intervaloEncendidoBombas){
    
     bombaEstado[0]=LOW;
     
    
    intervaloEncendidoPrevBomba_0 = millis();
    //Serial.print("Bomba activada en fermentador 0 con pin ");
    //Serial.println(bombaPin[0]);

  }
  else if (temperatura[0]<= temperaturaSeteada[0]){
    bombaEstado[0]=HIGH;
  }

  digitalWrite(bombaPin[0],bombaEstado[0]);
}

void escrituraLCD(){
  unsigned long intervaloLCDPrintActual = millis();

  if (intervaloLCDPrintActual - intervaloLCDPrintPrev > intervaloLCDPrint){
    lcd.setCursor(5,0);
    lcd.print("Fermentador: ");
    lcd.print(temperatura[0]);
    lcd.print("C");

    lcd.setCursor(14,3);
    lcd.print("Temperatura seteada: ");
    lcd.print(temperaturaSeteada[0]);
    lcd.print("C");
    intervaloLCDPrintPrev = millis();

    }

  unsigned long intervaloLCDScrollActual = millis();
  if (intervaloLCDScrollActual - intervaloLCDScrollPrev > intervaloLCDPrint){
    for (int scrollCounter = 0; scrollCounter < 31; scrollCounter++){
      lcd.scrollDisplayRight();
      intervaloLCDScrollPrev = millis();
    }
  }
}

// Sensado de temperatura en intervalo definido y guardo la misma en temperatura[x], donde x es el numero de fermentador
void sensarTemperatura(){
  //Guardo tiempo actual
  unsigned long intervaloTomaTempActual = millis();

  //Tomo temperaturas cada 2 minutos
  if (intervaloTomaTempActual - tempIntervaloSensadoPrev > tempIntervaloSensado){
  //Recupero cmdTemperatura de sensor DS, convierto la temperatura de Farenheit a Celcius y paso valor a sensor 1
    sensors.requestTemperatures();
    for (int i = 0; i < cantidadSensores; ++i){
      temperatura[i] = recuperarTemperatura(sensoresTemp[i]);
    }
  }
}

// Funcion para recuperar temperatura de sensor desde api rest
int getSensorTemp(String sensor){
  
  int sensorId = sensor.toInt();
  int temp = int(temperatura[sensorId]);

  return temp;
}

// Funcion para seteo de temperaturas en fermentador
int setFermentadorTemp(String command){
  String valores[2];
  
  // Parseo de comando
  for (int i = 0; i < command.length(); i++) {
    if (command.substring(i, i+1) == ",") {
      valores[0] = command.substring(0, i);
      valores[1] = command.substring(i+1);
      break;
    }
  }
  
  // Asigno valores a variables de fermentador y temperatura
  int ferNum = valores[0].toInt();
  temperaturaSeteada[ferNum]= valores[1].toInt();

  return temperaturaSeteada[ferNum];
}


int getFermentadorTemp(String command){
  int ferNum = command.toInt();

  return temperaturaSeteada[ferNum];
}



// Funcion de inicio de red
void iniciarRed(){
  // listen for incoming clients
  EthernetClient client = server.available();
  rest.handle(client);
  wdt_reset();

}
