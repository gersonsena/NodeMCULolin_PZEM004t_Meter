# NodeMCULolin_PZEM004t_Meter
Contém os códigos e bibliotecas utilizadas no meu trabalho de conclusão do curso de engenharia elétrica. MEDIDOR DE CONSUMO DE ENERGIA ELÉTRICA COM ACESSO LOCAL E REMOTO USANDO PLATAFORMA ESP8266
O projeto é funcional, mas didático, pois encontra alguns problemas de compatibilidade entre a biblioteca (e o próprio módulo) PZEM004T v1.0 e a arquiterura ESP, não resolvidos neste trabalho.
PRIMEIROS PASSOS:
Usar IDE Arduino 1.8.5 ou superior: https://www.arduino.cc/en/Main/Software (siga as instruções do seu SO);
Fazer as configuração para usar o ESP8266 na IDE Arduino: http://pedrominatel.com.br/pt/arduino/como-utilizar-o-esp8266-com-a-ide-arduino-instalando-o-modulo/
Baixe a lib estável do PZEM004T e instale na IDE (se tiver alguma dificuldade com esse passo: https://www.robocore.net/tutoriais/adicionando-bibliotecas-na-ide-arduino.html): https://github.com/olehs/PZEM004T
Efetue as conexões elétricas do seu módulo PZEM
Efetue o teste do módulo rodando o arquivo TestPZEM004T (disponível neste repositório): https://github.com/gersonsena/NodeMCULolin_PZEM004t_Meter/blob/master/TestePZEM004T.rar

Leia o trabalho: MEDIDOR DE CONSUMO DE ENERGIA ELÉTRICA COM ACESSO LOCAL E REMOTO USANDO PLATAFORMA ESP8266.
Também carregado aqui e que contem detalhes, fontes de pesquisa e demais materiais sobre o projeto, incluindo dados sobre o MQTT e módulos utilizados.
