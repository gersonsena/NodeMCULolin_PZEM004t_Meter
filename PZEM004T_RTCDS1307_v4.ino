#include <SoftwareSerial.h> 
#include <PZEM004T.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <RTClib.h> 
#include <LiquidCrystal_I2C.h>

////////////////////////////////////////////////////////////LCD_Serial////////////////////////////////////////////////////////////
// Inicializa o display no endereco 0x27
LiquidCrystal_I2C lcd(0x27,2,1,0,4,5,6,7,3, POSITIVE);


////////////////////////////////////////////////////////////RTC////////////////////////////////////////////////////////////
// Quantos segundos entre a captura de dados e o registro destes
#define LOG_INTERVAL  1000    // 1000 é de milisegundos = 1s
/* 
 *  Quantos segundos antes de gravar os dados registrados permanentemente no disco
 *  Configure-o para LOG_INTERVAL para escrever cada vez (mais seguro)
 *  Configurá-lo para 10 * LOG_INTERVAL para escrever tudo a cada 10 dados lidos, pode perder até
 *  as últimas 10 leituras se a energia é perdida, mas usa menos energia e é muito mais rápido!
 */
#define SYNC_INTERVAL 1000    // Tempo para chamar flush() - escrever dados no disco
uint32_t syncTime = 0;        // Tempo da última sincronização

RTC_DS1307 RTC;               // Define o objeto Real Time Clock como RTC

#define ECHO_TO_SERIAL   1    // echo para porta serial
#define WAIT_TO_START    0    // Aguardando pela entrada via serial do setup()

////////////////////////////////////////////////////////////LEDs////////////////////////////////////////////////////////////
// Define as variáveis e seta a elas os Pinos digitais para conectar os LEDs de status:
#define redLEDpin 2           //Digital 2 - LED Vermelho
#define greenLEDpin 3         //Digital 3 - LED Verde
#define yellowLEDpin 4        //Digital 4 - LED Amarelo = indica falha conexão PZEM004T
#define blueLEDpin 7          //Digital 7 - LED Azul = indica falha OU ausência Cartão SD

////////////////////////////////////////////////////////////SD_Card////////////////////////////////////////////////////////////
// Para o DATA_LOGGER_SHIELD usamos o pino digital 10 para a linha CS do disco
const int chipSelect = 10;

// O arquivo de registro [?]    
File logfile;

//Nome desse tipo de fração de código [?]
void error(char *str, int *pino)           // Função onde o parâmetro é um ponteiro para uma string e inteiro para um pino (LED)
{
  Serial.print("error: ");      // SE ocorrer falha OU ausência SD Card, liga o LED e trava o código em while(1)
  Serial.println(str);
  
// LED - Indicador de erro: chamada por parâmetro onde ocorre o ERRO
  digitalWrite(pino, HIGH); 

  while(1);
}

////////////////////////////////////////////////////////////PZEM////////////////////////////////////////////////////////////
//Configuração do módulo PZEM004T.

PZEM004T pzem(5, 6);             // Seta pino TX (3) PZEM no pino 5 UNO; Seta pino RX (2) PZEM no pino 6 UNO. 
IPAddress ip(192,168,1,1);       // Seta o endereço IP do módulo PZEM

bool pzemrdy = false;            // Atribui uma variável lógica do tipo true or false para teste de conexão PZEM


//-----------------------------------------------------------------------------------------------------------------------//
//----------------------------------------------------------SETUP--------------------------------------------------------//
//-----------------------------------------------------------------------------------------------------------------------//
void setup() {
delay(5000);
////////////////////////////////////////////////////////////LCD////////////////////////////////////////////////////////////
//Inicializa Display LCD  
  lcd.begin (16,2);              //Inicializa o Display LCD como tipo 16x2 (16 colunas e 2 linhas)

////////////////////////////////////////////////////////////Serial Monitor////////////////////////////////////////////////////////////
//Inicializa Serial Monitor (debug USB)
  Serial.begin(9600);            //BaudRate SeriaMonitor
  Serial.println();              //Imprime uma linha em branco

////////////////////////////////////////////////////////////LEDs_DEBUG////////////////////////////////////////////////////////////
//Define cada pino nomeado e numerado como tipo SAÍDA
  pinMode(yellowLEDpin, OUTPUT);
  pinMode(redLEDpin, OUTPUT);
  pinMode(greenLEDpin, OUTPUT);
  pinMode(blueLEDpin, OUTPUT);
  

////////////////////////////////////////////////////////////PZEM_IP_COM////////////////////////////////////////////////////////////
/*
 * Se a placa do UNO perder alimentação durante o processo, precisa ser reinicializada (RESET) quando for
 * novamente alimentada. As fontes precisam ser confiáveis e duráveis para as medições.
 */
//Inicialização do módulo PZEM004T. Sem esta conexão ele não recebe e nem envia os comandos AT de dados
  while (!pzemrdy) {                            //Testa e Espera pela Conexão com PZEM004T
      digitalWrite(yellowLEDpin, HIGH);         //Mantém LED Amarelo ligado enquanto não conectar
      Serial.println("Connecting to PZEM...");  //Imprime a status SerialMonitor
      pzemrdy = pzem.setAddress(ip);            //Faz conexão PZEM via IP informado
        lcd.setBacklight(HIGH);                 //Liga backlight do LCD
        lcd.setCursor(0,0);                     //Indica posição escrever status
        lcd.print("Connecting PZEM");           //Imprime status LCD
        delay(900);                             //Aguarda
        lcd.clear();                            //Limpa tela LCD
        lcd.setBacklight(LOW);                  //Desliga backlight LCD
      delay(100);                               //Aguarda
   }
   digitalWrite(yellowLEDpin, LOW);             //Desliga LED Amarelo indicando que houve conexão PZEM


////////////////////////////////////////////////////////////COM_SERIAL////////////////////////////////////////////////////////////
//Testa comunicação serial [?]
#if WAIT_TO_START
  Serial.println("Type any character to start");
  while (!Serial.available());
#endif //WAIT_TO_START

////////////////////////////////////////////////////////////SD_Card////////////////////////////////////////////////////////////
// Inicializando o disco - SD Card
/*
 * Digital 13 - SPI clock
 * Digital 12 - SPI MISO
 * Digital 11 - SPI MOSI
 * Digital 10 - CS
 * A4 - SDA
 * A5 - SCL
 */
 Serial.print("Initializing SD card...");
        lcd.setBacklight(HIGH);
        lcd.setCursor(0,0);
        lcd.print("Initializing SD");
        delay(1000);
        lcd.clear();       
/*
 * Certifique-se de que o pino de seleção de chip padrão esteja configurado para saída, 
 * mesmo que não o use para outra função:
 */
  pinMode(10, OUTPUT);    //SD Card chip select (CS)
  
// Verifica se o disco está presente e pode ser inicializado:
  if (!SD.begin(chipSelect)) {
    lcd.clear(); 
      lcd.setBacklight(HIGH);
      lcd.setCursor(0,0);
      lcd.print("SD Card failed!!");
    error("Card failed, or not present", redLEDpin);
      
  }
  Serial.println("card initialized.");
        lcd.setBacklight(HIGH);
        lcd.setCursor(0,0);
        lcd.print("SD initialized");
        delay(1000);
        lcd.clear();
  
//Criando um novo arquivo após um existente.
  char filename[] = "LOGGER00.CSV";
  for (uint8_t i = 0; i < 100; i++) {
    filename[6] = i/10 + '0';
    filename[7] = i%10 + '0';
    if (! SD.exists(filename)) {
      // Apenas abre um novo arquivo se não existir um
      logfile = SD.open(filename, FILE_WRITE); 
      break;  // Deixa o loop
    }
  }
  
  if (! logfile) {
    lcd.clear(); 
      lcd.setBacklight(HIGH);
      lcd.setCursor(0,0);
      lcd.print("Cant create file");
    error("couldnt create file", blueLEDpin);
  }

//Aguardando para escrever no arquivo
  Serial.print("Logging to: ");
  lcd.setBacklight(HIGH);
      lcd.setCursor(0,0);
      lcd.print("Logging to: ");
      lcd.setCursor(0,1);
      lcd.print(filename);
  Serial.println(filename);

////////////////////////////////////////////////////////////RTC////////////////////////////////////////////////////////////
//Testa ERRO na inicialização do RTC
  Wire.begin();  
  if (!RTC.begin()) {
    lcd.setBacklight(HIGH);
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("RTC failed!!!");
    logfile.println("RTC failed");

#if ECHO_TO_SERIAL
    Serial.println("RTC failed");
#endif  //ECHO_TO_SERIAL
  }
  
////////////////////////////////////////////////////////////COLUNAS////////////////////////////////////////////////////////////
// Cabeçalho dos dados, títulos das colunas  
logfile.println("DATA, Tensao, Corrente, Potencia, Energia");    

// Cabeçalho do Arquivo e SerialMonitor
#if ECHO_TO_SERIAL
  Serial.println("DATA, Tensao, Corrente, Potencia, Energia");
#endif //ECHO_TO_SERIAL

}

//-----------------------------------------------------------------------------------------------------------------------//
//-----------------------------------------------------------LOOP--------------------------------------------------------//
//-----------------------------------------------------------------------------------------------------------------------// 
void loop() {
////////////////////////////////////////////////////////////RTC////////////////////////////////////////////////////////////
//Inicializa o relógio com o tempo atual do RTC
  DateTime now;

//Quantidade de tempo que queremos atrasar entre as leituras
  delay((LOG_INTERVAL -1) - (millis() % LOG_INTERVAL));

//LED VERDE ligado: Indicador de INÍCIO processo de preparar dados para impressão no SD Card
  digitalWrite(greenLEDpin, HIGH);

/*
 * Nas próximas linhas, dados de tempo, tensão e corrente elétricas serão
 * lidos e preparados para gravação no arquivo .CSV criado
 */

//LOG milissegundos desde o início - Não mostrar esses valores
  uint32_t m = millis();      // armazena o tempo em m

//fetch - (busca) a Data e Hora atuais???
  now = RTC.now();
  
//Imprime as Informações de Data e Tempo no SD
  logfile.print('"'); 
  logfile.print(now.year(), DEC); logfile.print("/"); 
  logfile.print(now.month(), DEC); logfile.print("/"); 
  logfile.print(now.day(), DEC);
  logfile.print(" ");
  logfile.print(now.hour(), DEC); logfile.print(":"); logfile.print(now.minute(), DEC); logfile.print(":"); logfile.print(now.second(), DEC);
  logfile.print('"');

////////////////////////////////////////////////////////////PZEM_SDCard_Arq////////////////////////////////////////////////////////////
//Atraso para leitura correta dos dados do PZEM 
  delay(1000);
/*
 * Acessa os dados lidos pelo PZEM004T
 * Grava nas variáveis necessárias
 */

//Testa se o sinal de TENSÃO está presente e repassa o valor. SE não estiver, retorna -1 para todos os valores
  float v = pzem.voltage(ip);
  //if (v < 0.0) v = 0.0;
  logfile.print(", ");    logfile.print(v);
  
//Lê o sinal de CORRENTE 
  float i = pzem.current(ip);
  logfile.print(", ");    logfile.print(i);
  
//Lê o sinal de POTÊNCIA INSTANTÂNEA calculado pelos valores (v) e (i) 
  float p = pzem.power(ip);
  logfile.print(", ");    logfile.print(p);
  
//Lê o acumulado de ENERGIA consumida desde o último reset do módulo  
  float e = pzem.energy(ip);
  logfile.print(", ");    logfile.print(e);
  
//Quebra uma linha nos valores sendo impressos no Arquivo do SD Card
  logfile.println();

//LED VERDE desligado: indicador de que que o processo de obtenção dos valores está concluído para gravação
  digitalWrite(greenLEDpin, LOW);

/* 
 *  Agora escrevemos dados no disco!
 *  Não sincronizar com muita frequência - requer 2048 bytes de E/S para cartão SD.
 *  que usa uma certa quantidade de energia de energia e leva tempo.
 */

//[?]
  if ((millis() - syncTime) < SYNC_INTERVAL) return;
  syncTime = millis();
  
//Pisca o LED VERMELHO (no tempo de flush) sinalizanddo que os dados estão sincronizando com o cartão e atualizando FAT!
  digitalWrite(redLEDpin, HIGH);
    logfile.flush();
  digitalWrite(redLEDpin, LOW);

////////////////////////////////////////////////////////////LCD////////////////////////////////////////////////////////////
//Escreve no LCD Display - Monitoramento dos Valores lidso do PZEM e armazenados no Arduino
  lcd.setBacklight(HIGH);
  lcd.setCursor(0,0);
  lcd.print(v); lcd.print("V");
  lcd.setCursor(8,0);
  lcd.print(i); lcd.print("A");
  lcd.setCursor(0,1);
  lcd.print(p); lcd.print("W");
  lcd.setCursor(8,1);
  lcd.print(e); lcd.print("Wh");
  delay(1000);
  lcd.setBacklight(LOW);
  delay(1000);

////////////////////////////////////////////////////////////Serial_Monitor////////////////////////////////////////////////////////////
//Data e Hora - Imprime no Mnitor Serial (DEBUG)
#if ECHO_TO_SERIAL
  // Serial.print(now.unixtime()); Serial.print(", "); // segundos desde 1/1/1970
  Serial.print('"');
  Serial.print(now.year(), DEC); Serial.print("/");
  Serial.print(now.month(), DEC); Serial.print("/");
  Serial.print(now.day(), DEC);
  Serial.print(" ");
  Serial.print(now.hour(), DEC); Serial.print(":");
  Serial.print(now.minute(), DEC); Serial.print(":");
  Serial.print(now.second(), DEC);
  Serial.print('"');
#endif //ECHO_TO_SERIAL
  
//Valores lidos PZEM - Imprime no Monitor Serial (DEBUG)
#if ECHO_TO_SERIAL
  Serial.print(", ");   
  Serial.print(v);Serial.print("V ");
  Serial.print(", ");    
  Serial.print(i);Serial.print("A ");
  Serial.print(", ");   
  Serial.print(p);Serial.print("W ");
  Serial.print(", ");    
  Serial.print(e);Serial.print("Wh ");
#endif //ECHO_TO_SERIAL

#if ECHO_TO_SERIAL
  Serial.println();
#endif // ECHO_TO_SERIAL

}
