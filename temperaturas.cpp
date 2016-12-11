#include <temperaturas.h>

//############################# TODO Pasar como parametro las variables definidas abajo cuando llamo a la clase 
#define cantidadSensores 2

extern byte totalSensores;

extern float temperatura[cantidadSensores];

extern int bombaEstado[cantidadSensores];

extern int bombaPin[cantidadSensores];

extern int temperaturaSeteada[cantidadSensores];

extern long intervaloEncendidoActual;

extern long intervaloEncendidoPrevBomba[cantidadSensores];

extern long intervaloEncendidoBombas[cantidadSensores];




temperaturas::temperaturas(){}

// Funcion para el control de temperatura
void temperaturas::controlarTemps(){
  long intervaloEncendidoActual = millis();
  //Cuando la cmdTemperatura del fermentador supere la seteada en temperaturaSeteada[x]
  // durante el intervalo seteado en (intervaloEncendidoBombas) se activa las bomba
  for (byte i=0; i < totalSensores; i++) {
    if (temperatura[i]> temperaturaSeteada[i] && intervaloEncendidoActual - intervaloEncendidoPrevBomba[i] > intervaloEncendidoBombas){
      
       bombaEstado[i]=HIGH;
       
      // Guardo el momento en que se encendió por ultima vez la bomba para realizar el calculo de limite de encendido cada x minutos
      intervaloEncendidoPrevBomba[i] = millis();
    }
    // Si la temperatura es menor o igual a la seteada para el fermentador, la bomba se apaga
    else if (temperatura[i]<= temperaturaSeteada[i]){
      bombaEstado[i]=LOW;
    }

    digitalWrite(bombaPin[i],bombaEstado[i]);
  }
}

