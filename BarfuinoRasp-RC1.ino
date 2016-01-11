#include <Arduino.h>
/*
ACERCA
Este control de cmdTemperatura para fue creado para controlar la fermentacion de la cerveza. El objetivo de este es que la cmdTemperatura que es tomada por los sensores dentro del fermentador (sensoresDeTemperaturax),
no supere la especificada via serial que es guardadda en fermNumx. El valor de cmdTemperatura para cada fermentador(fermNumx) es eviado mediante una app python via puerto serie y estos son controlados directamente por arduino,
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


//Defino pin para busqueda de dispositivos OneWire
OneWire  ds(9);  

// Defino pines para LCD
LiquidCrystal lcd(12,11,5,4,3,2);

// Defino pin para sensores DS
#define ONE_WIRE_BUS 9

// Defino Precision de lectura
#define TEMPERATURE_PRECISION 9

// Instacio OneWire para todos los dispositivos en el bus
OneWire OneWire(ONE_WIRE_BUS);


// Paso como referencia el bus a la lib Dallas
DallasTemperature sensors(&OneWire);

// Defino direcciones de acceso a sensores
DeviceAddress sensor2 = { 0x28, 0xFF, 0x34, 0x71, 0x68, 0x14, 0x04, 0xC2 }; //Sensor1
DeviceAddress sensor1 = { 0x28, 0xFF, 0xB5, 0x80, 0x63, 0x14, 0x03, 0x78 }; //Sensor2
//DeviceAddress sensor1   = {0x28, 0xFF, 0x4A, 0xE4, 0x6D, 0x14, 0x04, 0x58}; //LEO

//////////////////////////////////////////////////////////////// VARIABLES GLOBALES//////////////////////////////////////////////////////////////////////////

//Variables de lectura para Temperatura de sensores 1 a 5
float temperatura[6]={};

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
int bombasPines [] = {10,11};

// TODO // Para uso con array dinamico parametrizando cantidad de bombas
int bombasCantidad = 3;
//int* bombasPines = 0;


//Estados de pines de relees para electro valvulas y bomba
int estadoBomba_1 = HIGH;
int estadoBomba_2 = HIGH;

//Variables para control de encendido de bombas, espera tiempo minimo de espera para encendido(5 minutos)
long intervaloEncendidoBombas = 500000;

///Almaceno timestamp de encendido anterior
long intervaloEncendidoPrevBomba_1 = 0;
long intervaloEncendidoPrevBomba_2 = 0;


//Intervalo minimo para tomar cmdTemperatura nuevamente(30 segundos)
long intervaloTomaTemp = 30000;
long intervaloTomaTempPrevia = 0;


// Intervalo de espera para LCD
unsigned long intervaloLCDPrint = 1500;
unsigned long intervaloLCDPrintPrev = 0;
unsigned long intervaloLCDScrollPrev = 0;


void setup(void){
  Serial.begin(9600);
  Serial.setTimeout(2000);

  // Buscar dispositivos 1-wire
  //discoverOneWireDevices();

  // Inicializo LCD
  lcd.begin(16,2);

  // Inicializo sensores DS 
  sensors.begin();

  //Defino pines a utilizar como salida para electro valvulas y bomba
  //for (int pin=9; pin>12; pin++){
  pinMode(bombasPines[0],OUTPUT);
  pinMode(bombasPines[1],OUTPUT);
  //}


  //Seteo de resolucion para sensores
  sensors.setResolution(sensor1, TEMPERATURE_PRECISION);
  sensors.setResolution(sensor2, TEMPERATURE_PRECISION);

}

void loop(void){

/////////////////////////////////////////////////////////////////////////////// VARIABLES LOCALES DE LOOP()////////////////////////////////////////////////////////////////////////////

  //Guardo tiempo actual
  unsigned long intervaloTomaTempActual = millis();

  //Tomo temperaturas cada 2 minutos
  if (intervaloTomaTempActual - intervaloTomaTempPrevia > intervaloTomaTemp){
  //Recupero cmdTemperatura de sensor DS, convierto la cmdTemperatura de Farenheit a Celcius y paso valor a sensor 1
      sensors.requestTemperatures();
      temperatura[1] = recuperarTemperatura(sensor1);
      temperatura[2] = recuperarTemperatura(sensor2);
  }

  // Modifico array de caracteres entrantes dinamicamente y convierto string to char
  char inputChar[inputString.length()+1];
  inputString.toCharArray(inputChar,inputString.length()+1);

  
  // Llamo a la funcion de evento serie para verificar entrada de datos
  serialEvent();

  
  // Imprime la cadena cuando llega una nueva linea:
  if (stringComplete) {

    Serial.println(inputString);

    // Parseo de comandos entrantes
    parsearComando();

  
    // Limpio la cadena para esperar nuevos datos entrantes
    memset(inputChar, 0, sizeof(inputChar));
    inputString = "";
    stringComplete = false;

  }

//Empiezo el control de cmdTemperatura segun temps comparando temperaturas en sensores 1 y 2 comparando con las fijadas en fermNum1 y fermNum2
  
  controlarTemps();
 
// Muestro datos por LCD
  escrituraLCD();
  
}

/////////////////////////////////////////////////////////////////////FUNCIONES///////////////////////////////////////////////////////////////////////
// Evento en serie (Lee cadena entrante a puerto serie y luego de un "\n" almacena el string en inputString)
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

      
    }
  }
}

void parsearComando(){
  char modo = inputString[0];
  char cmdFermentadorNumero = inputString[1] - '0';;
  String tempStr = "";

  switch (modo) {
      case 's':
          tempStr += inputString[2];
          tempStr += inputString[3];
          cmdTemperatura = tempStr.toInt();
          Serial.println("Modo seteo activado");
          setearTemperatura(cmdFermentadorNumero,cmdTemperatura);
          
      
        break;
      case 'g':
          Serial.println("Modo consulta activado");
          cmdTemperatura=getTemp(cmdFermentadorNumero);
          Serial.println(cmdTemperatura);
      
        break;
      case 'f':
          Serial.println(getSetTemp(cmdFermentadorNumero));
        break;
      default:
        Serial.println("Ingrese un comando valido");
  }
}

void imprimirChars(){
    // Imprimo los caracteres dentro del array
    for (int i=0;i < inputString.length();i++) {
        
        Serial.println(inputChar[i]);
        
    }
}

//Funcion para recuperar cmdTemperatura de fermentador en variable de ultima consulta
int getTemp (int numeroFermentador){
    int result;
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
void discoverOneWireDevices(void) {
  byte i;
  byte addr[8];
  //byte present = 0;
  //byte data[12];
  
  
  Serial.print("Buscando dispositivos 1-Wire...\n\r");
  while(ds.search(addr)) {
    Serial.print("\n\rSe encontro un dispositivo \'1-Wire\' device with address:\n\r");
    for( i = 0; i < 8; i++) {
      Serial.print("0x");
      if (addr[i] < 16) {
        Serial.print('0');
      }
      Serial.print(addr[i], HEX);
      if (i < 7) {
        Serial.print(", ");
      }
    }
    if ( OneWire::crc8( addr, 7) != addr[7]) {
        Serial.print("CRC no valido!\n");
        return;
    }
  }
  Serial.print("\n\r\n\rEso es todo.\r\n");
  ds.reset_search();
  return;
}

// Funcion para el control de cmdTemperatura
void controlarTemps(){
  long intervaloEncendidoActual = millis();

  //Cuando la cmdTemperatura del fermentador supere la deseada durante el intervalo seteado en (intervaloEncendidoBombas) se activan las bombas
  if (temperatura[1]> temperaturaSeteada[1] && intervaloEncendidoActual - intervaloEncendidoPrevBomba_1 > intervaloEncendidoBombas){
    estadoBomba_1=LOW;
    intervaloEncendidoPrevBomba_1 = millis();

  }
  else if (temperatura[1]<= temperaturaSeteada[1]){
    estadoBomba_1=HIGH;
  }

  if (temperatura[2]> temperaturaSeteada[2] && intervaloEncendidoActual - intervaloEncendidoPrevBomba_2 > intervaloEncendidoBombas){
    intervaloEncendidoPrevBomba_2 = millis();
    estadoBomba_2=LOW;

  }
  else if (temperatura[2]<= temperaturaSeteada[2]){
    estadoBomba_2=HIGH;
  }


  //Enciendo y apago valvulas segun las condiciones anteriores
  digitalWrite(bombasPines[0],estadoBomba_1);
  digitalWrite(bombasPines[1],estadoBomba_2);

}

void escrituraLCD(){

  unsigned long intervaloLCDPrintActual = millis();


  if (intervaloLCDPrintActual - intervaloLCDPrintPrev > intervaloLCDPrint){
    lcd.setCursor(5,0);
    lcd.print("Fermentador: ");
    lcd.print(temperatura[1]);
    lcd.print("C");

    lcd.setCursor(14,3);
    lcd.print("Temperatura seteada: ");
    lcd.print(temperaturaSeteada[1]);
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
