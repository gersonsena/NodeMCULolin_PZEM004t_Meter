/*CABEÇALHO DE IDENTIFICAÇÃO AUTORAL
 * Autor: Gerson E O Sena
 * Projeto: Medidor Datalogger de Energia Elétrica OpenSource com NodeMCU Lolin
 * Data: 2018
 * Local: Unipampa, Alegrete, RS, Brasil
 * Contribuições: Comunidade OpenSource, desenvolvedores, autores de artigos WEB.
 */

/*==================================================================================================================
 * INCLUSÃO DAS BIBLIOTECAS NECESSÁRIAS
 * 
 */
#include <PZEM004T.h>       //Biblioteca exclusiva do PZEM004T escrita para Arduino
#include <SD.h>             //Biblioteca de comunicação com o cartão SD nativa SPI
#include <Wire.h>           //Biblioteca de comunicação I2C nativa
#include "RTClib.h"         //Biblioteca de comunidação RTC que funcionou com Arduino/ ESP
//#include <ESP8266WiFi.h>    //Biblioteca nativa do ESP para conexões WiFi
 

/*Comunicação I2C: RTC, PCF8574.
 * D1 (05) = SCL
 * D2 (04) = SDA
 * Endereços scaneados:
 * 0x3F = PCF8574
 * 0x57 = EEProm do RTC DS3231
 * 0x68 = RTC DS3231
 * 
 * Comunicação SPI: SD Card
 * D8 (15) = CS
 * D7 (13) = MOSI
 * D6 (12) = SCK
 * D5 (14) = MISO
 */


/*==================================================================================================================
 * DEFINIÇÃO DAS VARIÁVEIS GLOBAIS
 */
//LCD


//RTC
//RTC_Millis rtc;               // Cria o objeto rtc associado ao chip DS3231
RTC_DS3231 rtc;

//PZEM
PZEM004T pzem(0, 2);          // D3 (GPIO00) = RX (recebe TX do PZEM), D4 (GPIO02) = TX (recebe RX do Pzem pelo Q 2N2907)
IPAddress ip(192,168,1,1);    // IP de comunicação do PZEM. Inicializa a comunicação com o módulo
bool pzemrdy = false;         // Variável tipo bolean para identificar se a comunicação com o ESP teve sucesso

//SD
const int chipSelect = 4;     // Valor atribuído ao Pino CS do SD card
bool CardOk = true;           // Variável tipo bolean para identificar se a comunicação com o SD teve sucesso
File dataFile;                // Cria objeto responsável por ler/ escrever no SD

float v, i, p, e, maior, menor, x, y, z = 0; // Variáveis para armazenar e tratar leituras do PZEM
char daysOfTheWeek[7][12] = {"DOM", "SEG", "TER", "QUA", "QUI", "SEX", "SÁB"};  // Cria array nomes e posição dos dias da semana



/*==================================================================================================================
 * SETUP, INICIALIZAÇÃO DAS VARIÁVEIS, FUNÇÕES E BIBLIOTECAS
 */

void setup() {  
Serial.begin(9600);
  Wire.begin();

//LCD


//RTC
//rtc.begin();

if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }
  else {
    Serial.println("Could find RTC!");
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, lets set the time!");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }

delay(3000);
    
//PZEM
  while (!pzemrdy) {                        // Testa e força a comunicação com o PZEM
      Serial.println("Connecting to PZEM...");
      pzemrdy = pzem.setAddress(ip);
      delay(1000);
   }

//SD
  if (!SD.begin(chipSelect)) {              // Testa comunicação com o SD
    Serial.println("Erro na leitura do arquivo, não existe um SD Card ou Módulo incorretamente conectado");
    CardOk = false;
    return;
  }

  if (CardOk) {                             // Caso SD esteja esteja OK, cria o arquivo 'datalog.csv' em modo ESCRITA 
    dataFile = SD.open("datalog.csv", FILE_WRITE);
    Serial.println("SD Card OK!");
  }
 
String leitura = "";                        // Limpa o campo string que será armazenado no SD como .csv
delay(1000);

leitura = String("DATA") + ";" + String("DSEM") + ";" + String("HORA") + ";" 
          + String("Volts") + ";" + String("Amps") + ";" + String("POTi") + ";" + String("Energy");
if (dataFile) {                         // Continuar a escrever no arquivo, mas 'zeros'  
      Serial.println("Escrevendo CABEÇALHO...");
      dataFile.println(leitura);            // Escrevemos no arquivos e pulamos uma linha
      dataFile.close();                     // Fechamos o arquivo
      Serial.println("Done");
      }
    return;

}


/*==================================================================================================================
 * LAÇO DE EXECUÇÃO DAS ROTINAS
 */
void loop() {

//SD
  if (CardOk) {                             // Caso SD esteja esteja OK, cria o arquivo 'datalog.csv' em modo ESCRITA 
    dataFile = SD.open("datalog.csv", FILE_WRITE);
    Serial.println("Cartão SD inicializado para escrita...");
  }
 
String leitura = "";                        // Limpa o campo string que será armazenado no SD como .csv
delay(1000);

//RTC
DateTime now = rtc.now();

Serial.print(now.year(), DEC); Serial.print('/'); Serial.print(now.month(), DEC); Serial.print('/'); Serial.print(now.day(), DEC);
    Serial.print(" ("); Serial.print(daysOfTheWeek[now.dayOfTheWeek()]); Serial.print(") ");
    Serial.print(now.hour(), DEC); Serial.print(':'); Serial.print(now.minute(), DEC); Serial.print(':'); Serial.print(now.second(), DEC);
    Serial.print("; ");

//PZEM
/* Tratamento de valores espúrios (a lib do PZEM foi escrita para arquitetura AVR, não ESP
 *  Armazena 3 valores em sequência e prepara para comparação/ limpeza de valores espúrios
 */
//Leitura Tensão para Testar FALTA
x = pzem.voltage(ip); y = pzem.voltage(ip); z = pzem.voltage(ip);
maior = x; menor = x;
  /*
   * LAÇO CONDICIONAL PRINCIPAL
   */
  if (x < 0.0 && y < 0.0 && z < 0.0) {      // Laço que identifica e confirma falta de energia x = y = z = -1 do PZEM
    v = 0.0; i = 0.0; p = 0.0; e = 0.0;     // Zera todos os valores para não 'printar' código '-1'
    Serial.print(v); Serial.println("V FALTA");
    
    //SD *** String linhas com zeros, para facilitar plotar curvas de carga
    //...
    leitura = String(now.year(), DEC) + ";" + String(now.month(), DEC) + ";" + String(now.day(), DEC) + ";"
    + String(daysOfTheWeek[now.dayOfTheWeek()]) + ";"
    + String(now.hour(), DEC) + ":" + String(now.minute(), DEC) + ":" + String(now.second(), DEC) + ";"
    + String(v) + ";" + String(i) + ";" + String(p) + ";" + String(e) + ";";
    
    if (dataFile) {                         // Continuar a escrever no arquivo, mas 'zeros'  
      Serial.println("Escrevendo FALTA...");
      dataFile.println(leitura);            // Escrevemos no arquivos e pulamos uma linha
      dataFile.close();                     // Fechamos o arquivo
      Serial.println("Done");
      }
    return;
  }
  else {                                    // Leitura de valores PZEM em Regime (sem FALTA)
    espurgov(pzem.voltage(ip), pzem.voltage(ip), pzem.voltage(ip));
    espurgoi(pzem.current(ip), pzem.current(ip), pzem.current(ip));
    espurgop(pzem.power(ip), pzem.power(ip), pzem.power(ip));
    espurgoe(pzem.energy(ip), pzem.energy(ip), pzem.energy(ip));
      Serial.print(v);Serial.print("V; ");
      Serial.print(i);Serial.print("A; ");
      Serial.print(p);Serial.print("W; ");
      Serial.print(e);Serial.println("Wh; ");
  }

// Caso os valores lidos do sensor não sejam válidos
  if (isnan(v) || isnan(i) || isnan(p) || isnan(e)) { 
    Serial.println ("Falha na leitura do sensor");
    delay(1500);
    return;
  }

//SD
// SE tudo estiver certo com os dados, cria a string com os valores lidos
// SD *** String linhas com zeros, para facilitar plotar curvas de carga
    //...
    leitura = String(now.year(), DEC) + "/" + String(now.month(), DEC) + "/" + String(now.day(), DEC) + ";"
    + String(daysOfTheWeek[now.dayOfTheWeek()]) + ";"
    + String(now.hour(), DEC) + ":" + String(now.minute(), DEC) + ":" + String(now.second(), DEC) + ";"
    + String(v) + ";" + String(i) + ";" + String(p) + ";" + String(e) + ";"; 
    
    
  if (dataFile) {   
    Serial.println("Escrevendo...");
    dataFile.println(leitura);  // Escrevemos no arquivos e pulamos uma linha
    dataFile.close();           // Fechamos o arquivo
    Serial.println("Done");
  }
 
  delay(1500);

}


/*==================================================================================================================
 * BLOCO DE FUNÇÕES UTILIZADAS
 * Funções de espurdo dados quebrados
 */
//RTC


// Leitura e Tratamento dos valores de Tensão
void *espurgov(float x, float y, float z){
  float maior = x; float menor = x;
      if (y > menor){
        menor = y;
        }
        else {
          menor = 0.0;
        }
        if (y > maior && y >= 0.0){
          maior = y;
          }
          if (z > menor){
            menor = z;
            }
          else {
            menor = 0.0;
            }
            if (z > maior && z >= 0.0){
              maior = z;
              }
      v = maior;
}

// Leitura e Tratamento dos valores de Corrente
void *espurgoi(float x, float y, float z){
  float maior = x; float menor = x;
      if (y > menor){
        menor = y;
        }
        else {
          menor = 0.0;
        }
        if (y > maior && y >= 0.0){
          maior = y;
          }
          if (z > menor){
            menor = z;
            }
          else {
            menor = 0.0;
            }
            if (z > maior && z >= 0.0){
              maior = z;
              }
      i = maior;
}

// Leitura e Tratamento dos valores de Potência Instantânea
void *espurgop(float x, float y, float z){
  float maior = x; float menor = x;
      if (y > menor){
        menor = y;
        }
        else {
          menor = 0.0;
        }
        if (y > maior && y >= 0.0){
          maior = y;
          }
          if (z > menor){
            menor = z;
            }
          else {
            menor = 0.0;
            }
            if (z > maior && z >= 0.0){
              maior = z;
              }
      p = maior;
}

// Leitura e Tratamento dos valores de Energia Consumida
void *espurgoe(float x, float y, float z){
  float maior = x; float menor = x;
      if (y > menor){
        menor = y;
        }
        else {
          menor = 0.0;
        }
        if (y > maior && y >= 0.0){
          maior = y;
          }
          if (z > menor){
            menor = z;
            }
          else {
            menor = 0.0;
            }
            if (z > maior && z >= 0.0){
              maior = z;
              }
      e = maior;
}
