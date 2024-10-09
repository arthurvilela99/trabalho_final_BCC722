#include <Arduino_FreeRTOS.h>

// Definições do Sensor de Umidade do Solo
const int pinSensorUmidade = A0;
int valorSensorUmidade = 0;
int valorMin = 0;
int valorMax = 1023;

// Definições do Sensor de Chuva
const int pinSensorChuva = A1;  // Pino para o sensor de chuva
int valorSensorChuva = 0;
int limiarChuva = 400;  // Limiar para definir quando está chovendo

// Definições do Sensor de Nível
const int pinSensorNivel = 2;  // Pino para o sensor de nível

// Definições de pinos para os LEDs
const int ledBaixa = 10;     // LED para umidade baixa
const int ledModerada = 9;   // LED para umidade moderada
const int ledAlta = 8;       // LED para umidade alta

// Pino da bomba de água
const int pinBomba = 4;  // Pino que aciona o relé (LED no seu caso)

// Variáveis globais
volatile bool umidadeBaixa = false;
volatile bool bombaLigada = false;
volatile bool bombaRecente = false;  // Variável para evitar que a bomba seja ligada repetidamente
volatile bool estaChovendo = false;  // Variável que indica se está chovendo
volatile bool nivelAdequado = false; // Variável que indica se o nível de água é adequado

// Handles das tarefas para poder suspendê-las e retomá-las
TaskHandle_t xHandleTarefaLeituraUmidade = NULL;
TaskHandle_t xHandleTarefaControleBomba = NULL;
TaskHandle_t xHandleTarefaSensorChuva = NULL;
TaskHandle_t xHandleTarefaSensorNivel = NULL;

// Função para a tarefa do sensor de nível
void tarefaSensorNivel(void *pvParameters) {
  while (1) {
    // Leitura do Sensor de Nível
    nivelAdequado = (digitalRead(pinSensorNivel) == 1);  // Verifica se a chave está fechada

    if (!nivelAdequado) {
      // Suspende todas as tarefas relacionadas ao controle da bomba e leitura de umidade
      if (xHandleTarefaLeituraUmidade != NULL) {
        vTaskSuspend(xHandleTarefaLeituraUmidade);
      }
      if (xHandleTarefaControleBomba != NULL) {
        vTaskSuspend(xHandleTarefaControleBomba);
      }
      if (xHandleTarefaSensorChuva != NULL) {
        vTaskSuspend(xHandleTarefaSensorChuva);
      }
    } else {
      // Retoma todas as tarefas se o nível de água for adequado
      if (xHandleTarefaLeituraUmidade != NULL) {
        vTaskResume(xHandleTarefaLeituraUmidade);
      }
      if (xHandleTarefaControleBomba != NULL) {
        vTaskResume(xHandleTarefaControleBomba);
      }
      if (xHandleTarefaSensorChuva != NULL) {
        vTaskResume(xHandleTarefaSensorChuva);
      }
    }

    // Aguarda 1 segundo antes de verificar novamente
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

// Função para a tarefa do sensor de chuva
void tarefaSensorChuva(void *pvParameters) {
  while (1) {
    // Leitura do Sensor de Chuva
    valorSensorChuva = analogRead(pinSensorChuva);
    estaChovendo = (valorSensorChuva < limiarChuva);  // Verifica se o valor é menor que o limiar

    if (estaChovendo) {
      // Suspende as outras tarefas
      if (xHandleTarefaLeituraUmidade != NULL) {
        vTaskSuspend(xHandleTarefaLeituraUmidade);
      }
      if (xHandleTarefaControleBomba != NULL) {
        vTaskSuspend(xHandleTarefaControleBomba);
      }

      // Desliga a bomba se estiver ligada
      if (bombaLigada) {
        Serial.println("Desligando bomba devido à chuva...");
        digitalWrite(pinBomba, LOW);
        bombaLigada = false;
      }

      // Desliga os LEDs
      digitalWrite(ledBaixa, LOW);
      digitalWrite(ledModerada, LOW);
      digitalWrite(ledAlta, LOW);
    } else {
      if (xHandleTarefaLeituraUmidade != NULL) {
        vTaskResume(xHandleTarefaLeituraUmidade);
      }
      if (xHandleTarefaControleBomba != NULL) {
        vTaskResume(xHandleTarefaControleBomba);
      }
    }

    // Aguarda 1 segundo antes de verificar novamente
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

// Função para leitura do sensor de umidade e controle dos LEDs
void tarefaLeituraUmidade(void *pvParameters) {
  while (1) {
    // Leitura do Sensor de Umidade
    valorSensorUmidade = analogRead(pinSensorUmidade);

    // Definindo a Umidade do Solo em Porcentagem
    int porcentagemUmidade = map(valorSensorUmidade, valorMin, valorMax, 100, 0);
    porcentagemUmidade = constrain(porcentagemUmidade, 0, 100);

    // Exibe o valor da umidade no monitor serial
    Serial.print("Umidade do solo: ");
    Serial.print(porcentagemUmidade);
    Serial.println("%");

    // Controle dos LEDs baseado na porcentagem de umidade
    if (porcentagemUmidade <= 40) {
      digitalWrite(ledBaixa, HIGH);  // Acende LED de umidade baixa
      digitalWrite(ledModerada, LOW);
      digitalWrite(ledAlta, LOW);
      umidadeBaixa = true;  // Atualiza a variável de controle
    } else if (porcentagemUmidade > 40 && porcentagemUmidade < 80) {
      digitalWrite(ledModerada, HIGH);  // Acende LED de umidade moderada
      digitalWrite(ledBaixa, LOW);
      digitalWrite(ledAlta, LOW);
      umidadeBaixa = false;
    } else if (porcentagemUmidade >= 80) {
      digitalWrite(ledAlta, HIGH);  // Acende LED de umidade alta
      digitalWrite(ledModerada, LOW);
      digitalWrite(ledBaixa, LOW);
      umidadeBaixa = false;
    }

    // Aguarda 1 segundo antes de repetir a leitura
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

// Função para controle da bomba de água
void tarefaControleBomba(void *pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();

  while (1) {
    // Reduzir o tempo de delay para testes (10 segundos)
    vTaskDelayUntil(&xLastWakeTime, 10000 / portTICK_PERIOD_MS);

    Serial.println("Entrando na tarefa de controle da bomba");

    Serial.print("umidadeBaixa: ");
    Serial.print(umidadeBaixa);
    Serial.print(", bombaLigada: ");
    Serial.println(bombaLigada);

    // Verifica se a umidade está baixa, a bomba está desligada e a bomba não foi ligada recentemente
    if (umidadeBaixa && !bombaLigada && !bombaRecente && nivelAdequado) {
      Serial.println("Ligando bomba...");
      digitalWrite(pinBomba, HIGH);  // Liga a bomba (relé/LED)
      bombaLigada = true;

      // Mantém a bomba ligada por 10 segundos
      vTaskDelay(10000 / portTICK_PERIOD_MS);

      Serial.println("Desligando bomba...");
      digitalWrite(pinBomba, LOW);  // Desliga a bomba (relé/LED)
      bombaLigada = false;

      // Marca que a bomba foi ligada recentemente
      bombaRecente = true;

      // Aguarda um tempo antes de permitir que a bomba seja ligada novamente (ex: 30 segundos)
      vTaskDelay(30000 / portTICK_PERIOD_MS);
      bombaRecente = false;
    } else {
      Serial.println("Condições para ligar a bomba não atendidas");
    }
  }
}

void setup() {
  // Inicializa a comunicação serial
  Serial.begin(9600);

  // Configura os pinos dos LEDs
  pinMode(ledBaixa, OUTPUT);
  pinMode(ledModerada, OUTPUT);
  pinMode(ledAlta, OUTPUT);

  // Configura o pino da bomba (relé/LED)
  pinMode(pinBomba, OUTPUT);
  digitalWrite(pinBomba, LOW);  // O relé começa desligado

  // Configura o pino do sensor de chuva
  pinMode(pinSensorChuva, INPUT);

  // Configura o pino do sensor de nível
  pinMode(pinSensorNivel, INPUT);

  // Criar tarefas
  xTaskCreate(tarefaLeituraUmidade, "Leitura Umidade", 128, NULL, 1, &xHandleTarefaLeituraUmidade);
  xTaskCreate(tarefaControleBomba, "Controle Bomba", 128, NULL, 1, &xHandleTarefaControleBomba);
  xTaskCreate(tarefaSensorChuva, "Sensor Chuva", 128, NULL, 1, &xHandleTarefaSensorChuva);
  xTaskCreate(tarefaSensorNivel, "Sensor Nivel", 128, NULL, 1, &xHandleTarefaSensorNivel);
}

void loop() {
  // A função loop não faz nada neste exemplo, pois estamos usando FreeRTOS para gerenciar as tarefas.
}
