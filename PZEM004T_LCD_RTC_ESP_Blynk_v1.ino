/*CABEÇALHO DE IDENTIFICAÇÃO AUTORAL
 * Autor: Gerson E O Sena
 * Projeto: Medidor Datalogger de Energia Elétrica OpenSource com NodeMCU Lolin
 * Data: 2018
 * Local: Unipampa, Alegrete, RS, Brasil
 * Contribuições: Comunidade OpenSource, desenvolvedores, autores de artigos WEB.
 * REFERÊNCIAS: Comentadas ao final do Código
 * ---------------------------------------------------------------------------------------------------------------
 * IMPORTANTE: Esta versão apresentou problemas com o barramento SPI (SD Card),
 * inviabilizando a gravações in locco de arquivos de logging.
 * Não se sabe se são problemas de conflitos de hardware ou bliblioteca (nativa Arduino),
 * sendo que outras questões foram testadas diversas vezes, sem sucesso. Desconfiava-se
 * da tensão de alimentação, inclusive, mas como o módulo tinha a opção de 3,3V e 
 * mesmo assim o erro persistiu, este bloco (SD Card) foi descontinuado.
 * Com isso o relógio RTC voltou a funcionar assim como foi possível adicionar o LCD,
 * ambos não funcionais na presenta do módulo SD Card.
 *  
 * A solução de relógio NTP do próprio Blynk foi testada enquanto isso (julgava-se ser problema
 * no RTC) e mostrou-se funcional. Nesta versão de código ela será usada para ajustar o
 * RTC quando este perder a memória por algum motivo (carga da bateria e estando desligado).
 * Mas tanto o RTC quando o NTP são blocos que podem ser removidos sem prejuízo algum das 
 * funções pensadas para este sistema
 */

/*==================================================================================================================
 * INCLUSÃO DAS BIBLIOTECAS NECESSÁRIAS
 * 
 */
#include <PZEM004T.h>               //Biblioteca exclusiva do PZEM004T escrita para Arduino
//#include <SD.h>                     //Biblioteca de comunicação com o cartão SD nativa
#include <Wire.h>                   //Biblioteca de comunicação I2C nativa
#include "RTClib.h"                 //Biblioteca de comunidação RTC que funcionou com Arduino/ ESP
#include <ESP8266WiFi.h>            //Biblioteca nativa do ESP para conexões WiFi
#include <BlynkSimpleEsp8266.h>     //Biblioteca de configuração do MQTT Blynk sobre ESP8266
#include <LiquidCrystal_PCF8574.h>  //Bilblioteca usada no módulo PCF8574T

#include <TimeLib.h>
#include <WidgetRTC.h>

/* Comentar esta linha para desabilitar debug SerialMonitor e economia de espaço memória */
#define BLYNK_PRINT Serial

/*Comunicação I2C: RTC, PCF8574.
* Ver http://playground.arduino.cc/Main/I2cScanner
* Deve-se usar o código acima para identificar os endereço I2C
 * D1 (05) = SCL
 * D2 (04) = SDA
 * Endereços scaneados:
 * 0x3F = LCD Display
 * 0x57 = EEPROM RTC DS3231
 * 0x68 = PCF8574

* Comunicação SPI: SD Card
 * D8 (15) = CS
 * D7 (13) = MOSI
 * D6 (12) = SCK
 * D5 (14) = MISO
 */


/*==================================================================================================================
 * DEFINIÇÃO DAS VARIÁVEIS GLOBAIS
 */
//RTC
//RTC_Millis rtc;               // Cria o objeto rtc - lib testada e funcional, precisa mudar código (ver exemplos)
RTC_DS3231 rtc;               // Cria o objeto rtc associado ao chip DS3231
String loggEvent, date, hr = "";


//LCD
LiquidCrystal_PCF8574 lcd(0x3F); //Seta o endereço do LCD para 0x3F
int error = 0;


//SD


//PZEM
PZEM004T pzem(0, 2);          // D3 (GPIO00) = RX (recebe TX do PZEM), D4 (GPIO02) = TX (recebe RX do Pzem pelo Q 2N2907)
IPAddress ip(192,168,1,1);    // IP de comunicação do PZEM. Inicializa a comunicação com o módulo
bool pzemrdy = false;         // Variável tipo bolean para identificar se a comunicação com o ESP teve sucesso
float v, i, p, e, maior, menor, x, y, z = 0; // Variáveis para armazenar e tratar leituras do PZEM


//BLYNK
/* Inserir o "Auth Token" obtido no Blynk App. Vá até "Project Settings" (ícone parafuso),
 *  entre "" a seguir.
 *  É enviado ao e-mail cadastrado ou pode ser copiado para área transferência SmartPhone.
 *  WiFi: Fazer o NodeMCU Lolin conectado na Rede,
 *  SSID e Senha devem ser escritos entre respectivas "" a seguir.
*/
char auth[] = "e05a2d366f984181a1ad6306d59a90d1";

char ssid[] = "REDETESTE";
char pass[] = "SENHARED";

float voltage_blynk = 0;
float current_blynk = 0;
float power_blynk = 0;
float energy_blynk = 0;

unsigned long lastMillis = 0; // Armazenar a base de tempo para cada transmissão o CloudServer Blynk

//NTP
BlynkTimer timer;
WidgetRTC rtc2;

//OTHERS
unsigned long timerun, prun = 0; // Usado para contar o tempo completo de cada ciclo de loop

/*==================================================================================================================
 * SETUP, INICIALIZAÇÃO DAS VARIÁVEIS, FUNÇÕES E BIBLIOTECAS
 */

void setup() {  
timerun = millis();

Serial.begin(115200);

//LCD
Serial.println("LCD...");

  while (!Serial);
    Serial.println("Checking LCD...");
    Wire.begin(4,5);
    Wire.beginTransmission(0x3F);   //Testa o endereço de comunicação
    error = Wire.endTransmission(); //Armazena o retorno do teste
      Serial.print("LCD Status (erro): ");      //Imprime o 
      Serial.print(error);                      //erro retornado

      if (error == 0) {                 //Se o erro = 0, ok
        lcd.setBacklight(255);                  //Liga backlight do LCD máximo brilho
        Serial.println(": LCD found.");
          lcd.clear();
          lcd.print("LCD found.");
        delay(1000);
        } 
        else {                          //Se o erro = (1 a 4), problema de comunicação
          Serial.println(": LCD not found.");
          }

lcd.begin(16, 2);     //Inicializa o LCD 16x2

//RTC
if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    lcd.clear();
        lcd.print("Not find RTC");
    while (1);
  }
  else {
    Serial.println("Could find RTC!");
    lcd.clear();
        lcd.print("RTC found!");
        delay(1000);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, lets set the time!");
    lcd.clear();
    lcd.setCursor(0,0);
        lcd.print("RTC pwr LOST");
        delay(1000);
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    lcd.setCursor(0,1);
        lcd.print("System Adj...");
        delay(1000);
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
    // lcd.setCursor(0,1);
    //    lcd.print("Manual Adj...");
    // This line sets the RTC with an NTP date & time:
    // rtc.adjust(DateTime(year(), month(), (day()), (hour()), minute(), second()));
    // lcd.setCursor(0,1);
    //    lcd.print("NTP Time Adj...");
  }
                                                  
//SD


//PZEM
  while (!pzemrdy) {                            // Testa e força a comunicação com o PZEM
      Serial.println("Connecting to PZEM...");
      pzemrdy = pzem.setAddress(ip);
        lcd.setBacklight(255);                  //Liga backlight do LCD máximo brilho
        lcd.setCursor(0,0);                     //Indica posição escrever status
        lcd.print("Connecting PZEM");           //Imprime status LCD
        delay(900);                             //Aguarda
        lcd.clear();                            //Limpa tela LCD
        lcd.setBacklight(0);                    //Desliga backlight LCD
      delay(100);    
   }


//BLYNK
  Serial.println("Blink begin...");
  lcd.setBacklight(255);                        //Liga backlight do LCD máximo brilho
    lcd.setCursor(0,0);                           //Indica posição escrever status
        lcd.print("Blynk begin...");            //Imprime Blynk tentando conexão...
  Blynk.begin(auth, ssid, pass);                  //Faz a conexão com o Blynk
  Serial.println("Blink begin OK");
    lcd.setCursor(0,1);                     
        lcd.print("Blynk OK!");                 //Imprime Sucesso na conexão...
  delay(500);
  lcd.setBacklight(0);                          //Liga backlight do LCD máximo brilho

//NTP
timer.setInterval(5000L, clockDisplay);

}


/*==================================================================================================================
 * LAÇO DE EXECUÇÃO DAS ROTINAS
 */
void loop() {
//LCD


//RTC
DateTime now = rtc.now();

    loggEvent = String(now.year(), DEC) + "/" + String(now.month(), DEC) + "/" + String(now.day(), DEC) + ";"
    + String(now.hour(), DEC) + ":" + String(now.minute(), DEC) + ":" + String(now.second(), DEC);
                                                  
//SD


//PZEM


//BLYNK
  Blynk.run();
  Serial.println("Blink run...");
  //NTP
  timer.run();
  Serial.println("timer run...");


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
    
    //RTC
    date = String(now.year(), DEC) + "/" + String(now.month(), DEC) + "/" + String(now.day(), DEC);
    hr = String(now.hour(), DEC) + ":" + String(now.minute(), DEC) + ":" + String(now.second(), DEC);

    //LCD
    lcd.setBacklight(255);
    lcd.clear();
      lcd.setCursor(0,0); lcd.print("FALTA_V");
      lcd.setCursor(8,0); lcd.print(hr);
      lcd.setCursor(0,1); lcd.print(date);

    //SD

    
    //BLYNK
      voltage_blynk = 1.00;                   // TENDO DIFICULDADES PARA MOSTRAR VALOR 0.0 NO BLYNK...       
      current_blynk = 1.00;
      power_blynk = 1.00;

    if (millis() - lastMillis > 5000) {       // 06/05/18... testar isso aqui...
       lastMillis = millis();
       Blynk.virtualWrite(V0, voltage_blynk); // Envia o valor de tensão para o VirtualPin V0
       Blynk.virtualWrite(V1, current_blynk); // ... VirtualPin V1            
       Blynk.virtualWrite(V2, power_blynk);   // ... VirtualPin V2
       Blynk.virtualWrite(V3, energy_blynk);  // ... VirtualPin V3      
    }
  }
    else {                                    // Leitura de valores PZEM em Regime (sem FALTA)
    espurgov(pzem.voltage(ip), pzem.voltage(ip), pzem.voltage(ip));
    espurgoi(pzem.current(ip), pzem.current(ip), pzem.current(ip));
    espurgop(pzem.power(ip), pzem.power(ip), pzem.power(ip));
    espurgoe(pzem.energy(ip), pzem.energy(ip), pzem.energy(ip));
      
      //RTC Serial Monitor
      Serial.print(now.year(), DEC); Serial.print('/'); Serial.print(now.month(), DEC); Serial.print('/'); Serial.print(now.day(), DEC);
      Serial.print(" - ");
      Serial.print(now.hour(), DEC); Serial.print(':'); Serial.print(now.minute(), DEC); Serial.print(':'); Serial.print(now.second(), DEC);
      Serial.print("; ");
      
      //PZEM Serial Monitor
      Serial.print(v);Serial.print("V; ");
      Serial.print(i);Serial.print("A; ");
      Serial.print(p);Serial.print("W; ");
      Serial.print(e);Serial.println("Wh; ");

      //LCD
      lcd.setBacklight(255);
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(v); lcd.print("V");
      lcd.setCursor(8,0);
      lcd.print(i); lcd.print("A");
      lcd.setCursor(0,1);
      lcd.print(p); lcd.print("W");
      lcd.setCursor(7,1);
      lcd.print(e); lcd.print("Wh");

      //SD
      
      //BLYNK
      //Publica (publisher) no servidor a cada 10s (10000 milliseconds). Mudar o valor altera o intervalo.
      if (millis() - lastMillis > 5000) {
        lastMillis = millis();
          Blynk.virtualWrite(V0, voltage_blynk); // Envia o valor de tensão para o VirtualPin V0
          Blynk.virtualWrite(V1, current_blynk); // ... VirtualPin V1            
          Blynk.virtualWrite(V2, power_blynk);   // ... VirtualPin V2
          Blynk.virtualWrite(V3, energy_blynk);  // ... VirtualPin V3
      }
    }//else
    
prun = (millis() - timerun)/1000;
Serial.print(prun); Serial.println("s ");

}//loop
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

void clockDisplay(){
  // You can call hour(), minute(), ... at any time
  // Please see Time library examples for details

  String currentTime = String(hour()) + ":" + minute() + ":" + second();
  String currentDate = String(day()) + " " + month() + " " + year();
  Serial.print("Current time: ");
  Serial.print(currentTime);
  Serial.print(" ");
  Serial.print(currentDate);
  Serial.println();

  // Send time to the App
  Blynk.virtualWrite(V5, currentTime);
  // Send date to the App
  Blynk.virtualWrite(V6, currentDate);
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

  FULL material for this job (portuguese):
  GITHub: https://github.com/gersonsena/NodeMCULolin_PZEM004t_Meter 

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
