#include <Arduino.h>
/*
ACERCA
Este control de temperatura para fue creado para controlar la fermentacion de la cerveza. El objetivo de este es que la temperatura que es tomada por los sensores dentro del fermentador (sensoresDeTemperaturax),
no supere la especificada via serial que es guardadda en fermNumx. El valor de temperatura para cada fermentador(fermNumx) es eviado mediante una app python via puerto serie y estos son controlados directamente por arduino,
ademas se agrego la posibilidad de consultar la temperatura de los sensores dentro de los fermentadores.


COMANDOS
Los comandos son recibidos por arduino via serial y hasta el momento puede realizar dos acciones como las antes descriptas.
Ejemplos.

Seteo de temperatura a 23 grados en fermentador 1:
s123 ("s" le dice a arduino que el comando es de seteo, "1" que es para fermNum1 y por ultimo "23" es la temperatura a fijar en este fermentador)

Temperatura actual en fermentador 1:
g1 ("g" hace un get de la temperatura a sensoresDeTemperatura1, el cual estaria dentro del fermentador 1)

*/
//Se importan las librerías
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal.h>

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
float sensoresDeTemperatura[6]={};

float sensoresDeTemperatura1 = 0;
float sensoresDeTemperatura2 = 0;


//Variables para almacenar temperaturas fijadas para fermentadores 1 y 2
int temperaturaSeteada[6] = {99,99,99,99,99,99};


//Variables para consulta y seteo de temperaturas
int fermentadorNumero = 0;
int temperatura = 0;


//Variables para almacenado de datos entrantes a serie
char comandoSerie[10]; // Creo array para almacenar comandos a procesar
char caracterSerieEntrante; // Creo variable para almacenar comando entrante via serie
byte indexComandoSerie = 0; // indice para recorrer array

String inputString = "";         // Variable que almacena cada cadena entrante al puerto serie
char inputChar []= {};
boolean stringComplete = false;  // Define si se completó la cadena


//Defino pines para electro valvulas y bomba
int bomba_1 = 10;
int bomba_2 = 11;

//Estados de pines de relees para electro valvulas y bomba
int estadoBomba_1 = HIGH;
int estadoBomba_2 = HIGH;

//Variables para control de encendido de bombas, espera tiempo minimo de espera para encendido(5 minutos)
long intervaloEncendidoBombas = 500000;

///Almaceno timestamp de encendido anterior
long intervaloEncendidoPrevBomba_1 = 0;
long intervaloEncendidoPrevBomba_2 = 0;


//Intervalo minimo para tomar temperatura nuevamente(30 segundos)
long intervaloTomaTemp = 30000;
long intervaloTomaTempPrevia = 0;


// Intervalo de espera para LCD
unsigned long intervaloLCDPrint = 1500;
unsigned long intervaloLCD2 = 2000;
unsigned long intervaloLCDPrintPrev = 0;
unsigned long intervaloLCDScrollPrev = 0;

int getTemp (int numeroFermentador);
int setearTemperatura (int numeroFermentador, int temperatura);
void imprimirTemperatura(DeviceAddress deviceAddress);
long recuperarTemperatura(DeviceAddress deviceAddress);


int getTemp (int numeroFermentador);
int getSetTemp (int numeroFermentador);
int setearTemperatura (int numeroFermentador, int temperatura);
void imprimirTemperatura(DeviceAddress deviceAddress);
long imprimirTemperaturaLcd(DeviceAddress deviceAddress);
long recuperarTemperatura(DeviceAddress deviceAddress);

void setup(void){
  Serial.begin(9600);
  Serial.setTimeout(2000);

  // Inicializao LCD
  lcd.begin(16,2);

  // Inicializo sensores para lectura
  sensors.begin();

  //Defino pines a utilizar como salida para electro valvulas y bomba
  //for (int pin=9; pin>12; pin++){
  pinMode(bomba_1,OUTPUT);
  pinMode(bomba_2,OUTPUT);
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
  //Recupero temperatura de sensor DS, convierto la temperatura de Farenheit a Celcius y paso valor a sensor 1
      sensors.requestTemperatures();
      sensoresDeTemperatura[1] = recuperarTemperatura(sensor1);
      sensoresDeTemperatura[2] = recuperarTemperatura(sensor2);
  }
/*
/////////////////////////////////////////////////////////////Leo comandos entrantes a serial, no se lee a menos que haya datos en el buffer
  while(Serial.available() > 0){
      // Uno menos que el tamaño del array
      if(indexComandoSerie < 9){
          caracterSerieEntrante = Serial.read(); // Se lee el caracter entrante al puerto serie
          comandoSerie[indexComandoSerie] = caracterSerieEntrante; // Se almacena en el array
          indexComandoSerie++; // Se incrementa el nro de indexComandoSerie para almacenar el proximo
          comandoSerie[indexComandoSerie] = '\0'; // Termina la entrada de la cadena con un NULL
          delay(10);
      }
  }

////////////////////////////////////////////////////////////Empiezo a analizar comandos entrantes para realizar acciones
  //Seteo de temperatura para fermentador con 's' y consulta con 'g'
  if (comandoSerie[0] == 's'){
      String tempStr = "";
      tempStr += comandoSerie[2];
      tempStr += comandoSerie[3];
      fermentadorNumero = comandoSerie[1] - '0';
      temperatura = tempStr.toInt();
      setearTemperatura(fermentadorNumero,temperatura);
      indexComandoSerie=0;
      memset(comandoSerie,0,sizeof(comandoSerie)); //Borro todos los datos del array
  }
  else if (comandoSerie[0] == 'g'){
      fermentadorNumero= comandoSerie[1] - '0'; //convierto char to int
      temperatura=getTemp(fermentadorNumero);
      Serial.println(temperatura);
      indexComandoSerie=0;
      memset(comandoSerie,0,sizeof(comandoSerie)); //Borro todos los datos del array
  }
  else if (comandoSerie[0] == 'f') {
      fermentadorNumero= comandoSerie[1] - '0';
      Serial.println(getSetTemp(fermentadorNumero));

      indexComandoSerie=0;
      memset(comandoSerie,0,sizeof(comandoSerie)); //Borro todos los datos del array
  }
      //memset(comandoSerie,0,sizeof(comandoSerie));
*/
///////////////////////////////////////////////////////////////////////////////////
// Modifico array dinamicamente y convierto string to char
    char inputChar[inputString.length()+1];
    inputString.toCharArray(inputChar,inputString.length()+1);

  
  // Llamo a la funcion de evento serie para verificar entrada de datos
  serialEvent();

  // Imprime la cadena cuando llega una nueva linea:
  if (stringComplete) {

  Serial.println(inputString);

  //imprimirChars();

  char modo = inputString[0];
  char fermentadorNumero = inputString[1] - '0';;
  String tempStr = "";

  switch (modo) {
      case 's':
          tempStr += inputString[2];
          tempStr += inputString[3];
          temperatura = tempStr.toInt();
          Serial.println("Modo seteo activado");
          setearTemperatura(fermentadorNumero,temperatura);
          
      
        break;
      case 'g':
          Serial.println("Modo consulta activado");
          temperatura=getTemp(fermentadorNumero);
          Serial.println(temperatura);
      
        break;
      case 'f':
          fermentadorNumero= comandoSerie[1] - '0';
          Serial.println(getSetTemp(fermentadorNumero));
        break;
      default:
        Serial.println("Ingrese un comando valido");
  }

  // Limpio la cadena para esperar nuevos datos entrantes
    memset(inputChar, 0, sizeof(inputChar));
    inputString = "";
    stringComplete = false;

}



///////////////////////////////////////Empiezo el control de temperatura segun temps comparando temperaturas en sensores 1 y 2 comparando con las fijadas en fermNum1 y fermNum2
  long intervaloEncendidoActual = millis();

  //Cuando la temperatura del fermentador supere la deseada durante el intervalo seteado en (intervaloEncendidoBombas) se activan las bombas
  if (sensoresDeTemperatura[1]> temperaturaSeteada[1] && intervaloEncendidoActual - intervaloEncendidoPrevBomba_1 > intervaloEncendidoBombas){
    estadoBomba_1=LOW;
    intervaloEncendidoPrevBomba_1 = millis();

  }
  else if (sensoresDeTemperatura[1]<= temperaturaSeteada[1]){
    estadoBomba_1=HIGH;
  }

  if (sensoresDeTemperatura[2]> temperaturaSeteada[2] && intervaloEncendidoActual - intervaloEncendidoPrevBomba_2 > intervaloEncendidoBombas){
    intervaloEncendidoPrevBomba_2 = millis();
    estadoBomba_2=LOW;

  }
  else if (sensoresDeTemperatura[2]<= temperaturaSeteada[2]){
    estadoBomba_2=HIGH;
  }


  //Enciendo y apago valvulas segun las condiciones anteriores
  digitalWrite(bomba_1,estadoBomba_1);
  digitalWrite(bomba_2,estadoBomba_2);


  // Muestro datos por LCD
  unsigned long intervaloLCDPrintActual = millis();


if (intervaloLCDPrintActual - intervaloLCDPrintPrev > intervaloLCDPrint){
  lcd.setCursor(5,0);
  lcd.print("Fermentador: ");
  lcd.print(sensoresDeTemperatura[1]);
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

void imprimirChars(){
    // Imprimo los caracteres dentro del array
    for (int i=0;i < inputString.length();i++) {
        
        Serial.println(inputChar[i]);
        
    }
}






//Funcion para recuperar temperatura de fermentador en variable de ultima consulta
int getTemp (int numeroFermentador){
    int result;
    result = sensoresDeTemperatura[numeroFermentador];

    return result;
}

int getSetTemp (int numeroFermentador){
    int result;
    result = temperaturaSeteada[numeroFermentador];

    return result;
}

//Funcion para seteo de temperatura en fermentador
int setearTemperatura (int numeroFermentador, int temperatura){
    temperaturaSeteada[numeroFermentador]=temperatura;

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




