/*CABEÇALHO DE IDENTIFICAÇÃO AUTORAL
 * Autor: Gerson E O Sena
 * Projeto: Medidor Datalogger de Energia Elétrica OpenSource com NodeMCU Lolin
 * Data: 2018
 * Local: Unipampa, Alegrete, RS, Brasil
 * Contribuições: Comunidade OpenSource, desenvolvedores, autores de artigos WEB.
 * REFERÊNCIAS: Comentadas ao final do Código
 */

/*==================================================================================================================
 * INCLUSÃO DAS BIBLIOTECAS NECESSÁRIAS
 * 
 */
#include <PZEM004T.h>             //Biblioteca exclusiva do PZEM004T escrita para Arduino
#include <SD.h>                   //Biblioteca de comunicação com o cartão SD nativa
#include <Wire.h>                 //Biblioteca de comunicação I2C nativa
#include "RTClib.h"               //Biblioteca de comunidação RTC que funcionou com Arduino/ ESP
//#include <ESP8266WiFi.h>          //Biblioteca nativa do ESP para conexões WiFi
#include <BlynkSimpleEsp8266.h>   //

/* Comentar esta linha para desabilitar debug SerialMonitor e economia de espaço memória */
#define BLYNK_PRINT Serial

/*Comunicação I2C: RTC, PCF8574.
 * D1 (05) = SCL
 * D2 (04) = SDA
 * Endereços scaneados:
 * 0x3F = PCF8574
 * 0x57 = RTC DS3231
 * 0x68 = ? (pode ser algo na própria placa do NodeMCU
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
//PZEM
PZEM004T pzem(0, 2);          // D3 (GPIO00) = RX (recebe TX do PZEM), D4 (GPIO02) = TX (recebe RX do Pzem pelo Q 2N2907)
IPAddress ip(192,168,1,1);    // IP de comunicação do PZEM. Inicializa a comunicação com o módulo
bool pzemrdy = false;         // Variável tipo bolean para identificar se a comunicação com o ESP teve sucesso

//SD
const int chipSelect = 4;     // Valor atribuído ao Pino CS do SD card
bool CardOk = true;           // Variável tipo bolean para identificar se a comunicação com o SD teve sucesso
File dataFile;                // Cria objeto responsável por ler/ escrever no SD

//RTC
RTC_Millis rtc;               // Cria o objeto rtc associado ao chip DS3231

float v, i, p, e, maior, menor, x, y, z = 0; // Variáveis para armazenar e tratar leituras do PZEM
char daysOfTheWeek[7][12] = {"DOM", "SEG", "TER", "QUA", "QUI", "SEX", "SÁB"};  // Cria array nomes e posição dos dias da semana

//BLYNK
float voltage_blynk = 0;
float current_blynk = 0;
float power_blynk = 0;
float energy_blynk = 0;

// Inserir o "Auth Token" obtido no Blynk App. Vá até "Project Settings" (ícone parafuso).
// É enviado ao e-mail cadastrado ou pode ser copiado para área transferência SmartPhone.
char auth[] = "e05a2d366f984181a1ad6306d59a90d1";

unsigned long lastMillis = 0; // Dá a base de tempo para cada transmissão o CloudServer Blynk

// WiFi: NodeMCU Lolin conectado na Rede.
// SSID e Senha devem ser escritos entre as aspas duplas a seguir.
char ssid[] = "REDETESTE";
char pass[] = "SENHARED";


/*==================================================================================================================
 * SETUP, INICIALIZAÇÃO DAS VARIÁVEIS, FUNÇÕES E BIBLIOTECAS
 */

void setup() {  
  Serial.begin(115200);
  Wire.begin();

//BLYNK
  Serial.println("Blink begin...");
  Blynk.begin(auth, ssid, pass);                  //Faz a conexão com o Blynk
  Serial.println("Blink begin OK");

//RTC
//rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));   // Configura a Data e Hora do RTC pela do SISTEMA no momento da compilação
                                                  // Após compilar e ajustando a hora da primeira vez precisa comentar essa
                                                  //linha, senão a cada reset (sem USB volta pra info da primeira configuração
                                                  
//rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));       // Configura a Data e Hora do RTC MANUALMENTE

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
//BLYNK
  Blynk.run();
  Serial.println("Blink run...");

//SD
  if (CardOk) {                             // Caso SD esteja esteja OK, cria o arquivo 'datalog.csv' em modo ESCRITA 
    dataFile = SD.open("datalog.csv", FILE_WRITE);
    Serial.println("Cartão SD inicializado para escrita...");
  }
 
String leitura = "";                        // Limpa o campo string que será armazenado no SD como .csv
delay(1000);

//RTC
DateTime now = rtc.now();                   // Registra o tempo do RTC quando a rotina passa por essa linha

delay(3000);
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
    voltage_blynk = 0.01;                      // TENDO DIFICULDADES PARA MOSTRAR VALOR 0.0 NO BLYNK       
    current_blynk = 0.01;
    power_blynk = 0.01;
    energy_blynk = 0.01;

    if (millis() - lastMillis > 5000) {       // 06/05/18... testar isso aqui...
       lastMillis = millis();
       Blynk.virtualWrite(V1, voltage_blynk); // Envia o valor de tensão para o VirtualPin V1
       Blynk.virtualWrite(V2, current_blynk); // ... VirtualPin V2            
       Blynk.virtualWrite(V3, power_blynk);   // ... VirtualPin V3
       Blynk.virtualWrite(V4, energy_blynk);  // ... VirtualPin V4      
    }
 
    delay(1000);
    
    //SD *** String linhas com zeros, para facilitar plotar curvas de carga
    leitura = String(now.year(), DEC) + "/" + String(now.month(), DEC) + "/" + String(now.day(), DEC) + ";"
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
  if (isnan(v) || isnan(i) || isnan(p) || isnan(e)) {   // Ver a real necessidade disso aqui...
    Serial.println ("Falha na leitura do sensor");
    delay(1500);
    return;
  }

//SD
// SE tudo estiver certo com os dados, cria a string com os valores lidos
// SD *** String linhas com zeros, para facilitar plotar curvas de carga
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

delay(1000);

//BLYNK
//Publica (publisher) no servidor a cada 10s (10000 milliseconds). Mudar o valor altera o intervalo.

    if (millis() - lastMillis > 5000) {
       lastMillis = millis();
       Blynk.virtualWrite(V1, voltage_blynk); // Envia o valor de tensão para o VirtualPin V1
       Blynk.virtualWrite(V2, current_blynk); // ... VirtualPin V2            
       Blynk.virtualWrite(V3, power_blynk);   // ... VirtualPin V3
       Blynk.virtualWrite(V4, energy_blynk);  // ... VirtualPin V4      
    }
 
delay(1000);
}


/*==================================================================================================================
 * BLOCO DE FUNÇÕES UTILIZADAS
 * Funções de espurdo dados quebrados
 */
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
      voltage_blynk = v;
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
      current_blynk = i;
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
      power_blynk = p;
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
      energy_blynk = e;
}

/*REFERÊNCIAS
 * Todas as referências foram pontos de partida do projeto. Várias delas
 * não funcionaram a contento precisando consultar diversas outras fontes
 * a fim de verificar os erros de configuração, pinagens, integração de 
 * módulos e bibliotecas, conflitos diversos e funcionamento do próprio 
 * hardwere.
 *************************************************************
 
 * Blynk: versão 2.20.2, Server: Blynk Cloud
  Download latest Blynk library here:
    https://github.com/blynkkk/blynk-library/releases/latest

  Blynk is a platform with iOS and Android apps to control
  Arduino, Raspberry Pi and the likes over the Internet.
  You can easily build graphic interfaces for all your
  projects by simply dragging and dropping widgets.

    Downloads, docs, tutorials: http://www.blynk.cc
    Sketch generator:           http://examples.blynk.cc
    Blynk community:            http://community.blynk.cc
    Follow us:                  http://www.fb.com/blynkapp
                                http://twitter.com/blynk_app

  Blynk library is licensed under MIT license
  This example code is in public domain.

  Examples blynk virtual data : 
          https://examples.blynk.cc/?board=ESP8266&shield=ESP8266%20WiFi&example=GettingStarted%2FVirtualPinWrite  

          Blynk Github:
          https://github.com/blynkkk/blynk-library/blob/master/examples/Boards_WiFi/ESP8266_Standalone/ESP8266_Standalone.ino

 *************************************************************
  * ESP8266: NodeMCU Lolin (v3)
  This example runs directly on ESP8266 chip.

  Note: This requires ESP8266 support package:
    https://github.com/esp8266/Arduino

  Please be sure to select the right ESP8266 module
  in the Tools -> Board menu!

  Change WiFi ssid, pass, and Blynk auth token to run :)
  Feel free to apply it to any other example. It's simple!

  SoftwareSerial for esp8266:
  https://github.com/plerup/espsoftwareserial
 
 *************************************************************
  * PZEM-004T
  Complete Tutorial English:
  http://pdacontrolen.com/meter-pzem-004-esp8266-platform-iot-blynk-app/
  
  PZEM004T library for olehs:
  https://github.com/olehs/PZEM004T
 
 */
