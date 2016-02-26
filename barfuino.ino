#include <Arduino.h>
/*
ACERCA
Este control de cmdTemperatura para fue creado para controlar la fermentacion de la cerveza. El objetivo de este es que la cmdTemperatura que es 
tomada por los sensores dentro del fermentador (sensoresDeTemperaturax),
no supere la especificada via serial que es guardadda en fermNumx. El valor de cmdTemperatura para cada fermentador(fermNumx) es 
eviado mediante una app python via puerto serie y estos son controlados directamente por arduino,
ademas se agrego la posibilidad de consultar la cmdTemperatura de los sensores dentro de los fermentadores.


COMANDOS
Los comandos son recibidos por arduino via serial y hasta el momento puede realizar dos acciones como las antes descriptas.
Ejemplos.

Seteo de cmdTemperatura a 23 grados en fermentador 1:
s123 ("s" le dice a arduino que el comando es de seteo, "1" que es para fermNum1 y por ultimo "23" es la cmdTemperatura a fijar en este fermentador)

Temperatura actual en fermentador 1:
g1 ("g" hace un get de la cmdTemperatura a sensoresDeTemperatura1, el cual estaria dentro del fermentador 1)

*/
//Se importan las librerías
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal.h>

// Defino pines para LCD
LiquidCrystal lcd(12,11,5,4,3,2);

// Defino pin para sensores DS
#define ONE_WIRE_BUS 9
// Cantidad de sensores conectados
#define cantidadSensores 1 
// Device Addresses are 8-element byte arrays
byte sensoresTemp [cantidadSensores][8];
// Declare variable to store number of One Wire devices that are actually discovered.
byte totalSensores; 
// Defino Precision de lectura
#define TEMPERATURE_PRECISION 9
// Instacio OneWire para todos los dispositivos en el bus
OneWire OneWire(ONE_WIRE_BUS);
// Paso como referencia el bus a la lib Dallas
DallasTemperature sensors(&OneWire);

// Defino direcciones de acceso a sensores
DeviceAddress sensor0 = { 0x28, 0xFF, 0x34, 0x71, 0x68, 0x14, 0x04, 0xC2 }; //Sensor1
//DeviceAddress sensor0 = { 0x28, 0xFF, 0xB5, 0x80, 0x63, 0x14, 0x03, 0x78 }; //sensor1
//DeviceAddress sensor0   = {0x28, 0xFF, 0x4A, 0xE4, 0x6D, 0x14, 0x04, 0x58}; //LEO

///////////////////////////////////////////////////// VARIABLES GLOBALES//////////////////////////////////////////////////////////////////////////

//Variables de lectura para Temperatura de sensores 1 a 5
float temperatura[cantidadSensores]={};
//Variables para almacenar temperaturas fijadas para fermentadores 1 y 2
int temperaturaSeteada[6] = {99,99,99,99,99,99};

//Variables para consulta y seteo de temperaturas
int cmdFermentadorNumero = 0;
int cmdTemperatura = 0;

//Variables para almacenado de datos entrantes a serie
String inputString = "";         // Variable que almacena cada cadena entrante al puerto serie
char inputChar []= {};
boolean stringComplete = false;  // Define si se completó la cadena

//Defino pines para electro valvulas y bomba
int bombaPin [] = {10,11};

// TODO // Para uso con array dinamico parametrizando cantidad de bombas
#define bombasCantidad 2
//int* bombaPin = 0;

//Estados de pines de relees para electro valvulas y bomba
//int bombaEstado [] = {LOW,LOW};
int bombaEstado [] = {HIGH,HIGH};
//int bombaEstado [bombasCantidad] = {LOW,LOW};

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
  Serial.setTimeout(2000);
  sensors.begin();
  // Buscar dispositivos 1-wire
  sensors.begin();
  // Guarda todas las mac de los sensores enallAddress array 
  totalSensores = discoverOneWireDevices();         
  for (byte i=0; i < totalSensores; i++) 
    sensors.setResolution(sensoresTemp[i], 10);

  // Inicializo LCD
  lcd.begin(16,2);

  // Inicializo sensores DS 
  sensors.begin();

  //Defino pines a utilizar como salida para electro valvulas y bomba
  //for (int pin=10; pin > pin + bombasCantidad; pin++){
  //pinMode(bombaPin[pin],OUTPUT);
  //}
  pinMode(bombaPin[0],OUTPUT);
  pinMode(bombaPin[1],OUTPUT);
  Serial.println("Pines seteados en modo salida");

}

void loop(void){

////////////////////////////////////////////// VARIABLES LOCALES DE LOOP()////////////////////////////////////////////////////////////////////////////
  // Realiza la lectura de temperatura de todos los sensores cada 2 mins y son guardadas en array temperatura
  sensarTemperatura();

  // TODO// Modifico array de caracteres entrantes dinamicamente y convierto string to char
  char inputChar[inputString.length()+1];
  inputString.toCharArray(inputChar,inputString.length()+1);

  // Llamo a la funcion de evento serie para verificar entrada de datos
  serialEvent();
  
  // Imprime comando entrante
  imprimirCmdIn();

  //Empiezo el control de cmdTemperatura segun temps comparando temperaturas en sensores 1 y 2 con las fijadas en fermNum1 y fermNum2
  controlarTemps();
 
  // Muestro datos por LCD
  escrituraLCD();
  
}

/////////////////////////////////////////////////////////////////////FUNCIONES///////////////////////////////////////////////////////////////////////
// Evento en serie (Lee cadena entrante a puerto serie y luego de un "\n" almacena el comando en inputString)
void serialEvent() {
  while (Serial.available()) {
    
    // Toma un nuevo byte
    char inChar = (char)Serial.read();
       
    // Si el caracter entrante es una nueva linea (\n) se setea el flag de cadena completa (StringComplete) en true sino agrega el char a inputString
    if (inChar == '\n') {
      stringComplete = true;
      // Quito el caracter de nueva linea (\n) de inputString
      inputString.trim();
    }
    else{
      // Agrego la cadena a inputString:
      inputString += inChar;
      delay(10);
    }
  }
}

// Parseo de comandos entrantes
void parsearComando(){
  char modo = inputString[0];
  char cmdFermentadorNumero = inputString[1] - '0';
  String tempStr = "";

  switch (modo) {
      case 's':
          tempStr += inputString[2];
          tempStr += inputString[3];
          cmdTemperatura = tempStr.toInt();
          //Serial.println("Modo seteo activado");
          setearTemperatura(cmdFermentadorNumero,cmdTemperatura);
        break;
      case 'g':
          //Serial.println("Modo consulta activado");
          cmdTemperatura=getTemp(cmdFermentadorNumero);
          Serial.println(cmdTemperatura);
        break;
      case 'f':
          Serial.println(getSetTemp(cmdFermentadorNumero));
        break;
      default:
        Serial.println("err");
  }
}

// Imprime caracteres entrantes
void imprimirChars(){
    // Imprimo los caracteres dentro del array
    for (int i=0;i < inputString.length();i++) {
        
        Serial.println(inputChar[i]);
    }
}

//Funcion para recuperar cmdTemperatura de fermentador en variable de ultima consulta
int getTemp (int numeroFermentador){
    float result;
    result = temperatura[numeroFermentador];

    return result;
}

int getSetTemp (int numeroFermentador){
    int result;
    result = temperaturaSeteada[numeroFermentador];

    return result;
}

//Funcion para seteo de cmdTemperatura en fermentador
int setearTemperatura (int numeroFermentador, int cmdTemperatura){
    temperaturaSeteada[numeroFermentador]=cmdTemperatura;

    Serial.print("Temperatura en fermentador ");
    Serial.print(numeroFermentador);
    Serial.print(" ");
    Serial.println(temperaturaSeteada[numeroFermentador]);
}

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

//Imprime comando entrante
void imprimirCmdIn(){
  // Imprime la cadena cuando llega una nueva linea:
  if (stringComplete) {

    //Serial.println(inputString);

    // Parseo de comandos entrantes
    parsearComando();
  
    // Limpio la cadena para esperar nuevos datos entrantes
    memset(inputChar, 0, sizeof(inputChar));
    inputString = "";
    stringComplete = false;
  }
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

/*
  // DesCOMENTAR PARA USO DE ELECTROVALVULAS
  if (temperatura[2]> temperaturaSeteada[1] && intervaloEncendidoActual - intervaloEncendidoPrevBomba_1 > intervaloEncendidoBombas){
    intervaloEncendidoPrevBomba_1 = millis();
    bombaEstado[0]=LOW;

  }
  else if (temperatura[2]<= temperaturaSeteada[1]){
    bombaEstado[0]=HIGH;
  }
*/
  //Enciendo y apago valvulas segun las condiciones anteriores
  //digitalWrite(bombaPin[1],bombaEstado[1]);
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

// Sensado de temperatura
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

