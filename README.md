<h1 align="center">
  Tech4Parking · IoT
</h1>

<p align="center">
  <img src="docs/arch.gif" alt="Arquitetura do Tech4Parking na AWS" />
</p>

<p align="center">
  <a href="https://skillicons.dev">
    <img src="https://skillicons.dev/icons?i=arduino,cpp,aws" alt="Stacks" />
  </a>
</p>

## Qual a finalidade do projeto?

Firmware do **sensor de vaga** do **Tech4Parking**. Um **ESP32** com um sensor ultrassônico **HC-SR04** fica instalado na vaga, mede a distância até o carro e publica no **AWS IoT Core**, via **MQTT sobre TLS**, sempre que a vaga muda de **disponível** para **ocupada** (ou o contrário).

A mensagem chega na **regra IoT** do tópico `parking_sensor`, que invoca a **Lambda** de vagas. Ela atualiza a tabela no **DynamoDB**, e o web app mostra o estado em tempo real.

## O que foi construído

### Firmware

| Recurso | Descrição |
|---|---|
| Configuração na primeira vez | Pede o Wi-Fi e o ID da vaga pela serial e salva na flash (NVS) |
| Reconfiguração | Digitar `c` + Enter nos primeiros 5 segundos após ligar |
| Medição | A cada 5 s; menos de 20 cm = `ocupada`, senão `disponível` |
| Leituras inválidas | Sem eco (timeout de 30 ms) são ignoradas |
| Publicação | Só quando o status muda, no tópico `parking_sensor` |
| Conexão | MQTT/TLS na porta 8883, com client ID único por vaga (`tech4parking-<spot_id>`) |

### Mensagem publicada

```json
{"spot_id": "A-01", "status": "ocupada", "distance": 12.34}
```

### Ligação

| HC-SR04 | ESP32 |
|---|---|
| VCC | 5V |
| GND | GND |
| TRIG | GPIO 17 (TX2) |
| ECHO | GPIO 16 (RX2) ⚠️ usar divisor de tensão (o ECHO sai em 5V) |

## Tecnologias utilizadas

- **ESP32 (Arduino / C++):** microcontrolador com Wi-Fi;
- **HC-SR04:** sensor ultrassônico de distância;
- **PubSubClient:** cliente MQTT;
- **WiFiClientSecure:** conexão TLS com certificado do dispositivo;
- **Preferences (NVS):** armazenamento do Wi-Fi e do ID da vaga;
- **AWS IoT Core:** broker MQTT e regra que encaminha para a Lambda.

## Estrutura do repositório

```text
tech4parking-iot/
├── iot-core/parking-spots/
│   ├── parking-spots.ino        # Firmware do sensor
│   └── secrets.example.h        # Modelo de endpoint, certificado e chave
├── docs/arch.gif                # Diagrama da arquitetura
└── README.md
```

## Fluxo de funcionamento

1. Ao ligar, o ESP32 carrega o Wi-Fi e o ID da vaga da memória (ou pede pela serial na primeira vez).
2. Conecta ao Wi-Fi e ao AWS IoT Core com o certificado do dispositivo.
3. A cada 5 segundos, o HC-SR04 mede a distância.
4. Quando a vaga muda de estado, o firmware publica a mensagem no tópico `parking_sensor`.
5. A regra IoT invoca a Lambda, que atualiza a tabela `ParkingSpots`.
6. O web app passa a mostrar a vaga como disponível ou ocupada.

## Configuração e gravação

1. Crie um *Thing* no AWS IoT Core e baixe o certificado e a chave privada (a infraestrutura em [tech4parking-infra](https://github.com/willtechdev/tech4parking-infra) já cria o Thing, o certificado e a policy).
2. Copie `iot-core/parking-spots/secrets.example.h` para `secrets.h` (ignorado pelo git) e preencha o endpoint, o certificado e a chave.
3. Compile e grave. No Arduino IDE: placa **ESP32 Dev Module** + biblioteca **PubSubClient**, monitor serial em 9600. Ou com `arduino-cli`:

```bash
arduino-cli core install esp32:esp32
arduino-cli lib install PubSubClient
arduino-cli compile --fqbn esp32:esp32:esp32 iot-core/parking-spots
arduino-cli upload  --fqbn esp32:esp32:esp32 -p /dev/ttyUSB0 iot-core/parking-spots
```

## Projeto Tech4Parking

| Repositório | Camada |
|---|---|
| [tech4parking-front](https://github.com/willtechdev/tech4parking-front) | Web app (Next.js) |
| [tech4parking-back](https://github.com/willtechdev/tech4parking-back) | Lambda de vagas (sensor + API) |
| [tech4parking-infra](https://github.com/willtechdev/tech4parking-infra) | Infraestrutura AWS (Terraform) |
| **tech4parking-iot** | Firmware do sensor (ESP32) |

## Autor

**William Alves Coelho** · [@willtechdev](https://github.com/willtechdev)
