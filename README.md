# tech4parking-iot

Firmware do sensor de vaga do Tech4Parking: ESP32 + sensor ultrassônico HC-SR04 publicando a ocupação da vaga no AWS IoT Core via MQTT (TLS, porta 8883).

## Como funciona

1. No primeiro boot pede pela serial o Wi-Fi e o ID da vaga e salva na memória flash (NVS).
2. Nos boots seguintes usa a configuração salva. Para reconfigurar, digite `c` + Enter nos primeiros 5 segundos.
3. A cada 5 s mede a distância: menos de 20 cm = `ocupada`, senão `disponível`. Leituras sem eco são ignoradas.
4. Quando o status muda, publica no tópico `parking_sensor`:

```json
{"spot_id": "A-01", "status": "ocupada", "distance": 12.34}
```

## Ligação

| HC-SR04 | ESP32 |
|---------|-------|
| VCC | 5V |
| GND | GND |
| TRIG | GPIO 17 (TX2) |
| ECHO | GPIO 16 (RX2) ⚠️ use divisor de tensão (o ECHO sai em 5V) |

## Configuração

1. Crie um *Thing* no AWS IoT Core e baixe o certificado e a chave privada.
2. Copie `iot-core/parking-spots/secrets.example.h` para `secrets.h` (ignorado pelo git) e preencha o endpoint, o certificado e a chave.

## Compilar e gravar

Arduino IDE: placa **ESP32 Dev Module** (pacote `esp32` da Espressif) + biblioteca **PubSubClient**. Monitor serial em 9600.

Ou com `arduino-cli`:

```bash
arduino-cli core install esp32:esp32
arduino-cli lib install PubSubClient
arduino-cli compile --fqbn esp32:esp32:esp32 iot-core/parking-spots
arduino-cli upload  --fqbn esp32:esp32:esp32 -p /dev/ttyUSB0 iot-core/parking-spots
```

## Repositórios relacionados

- [tech4parking-infra](https://github.com/tech4parking-org/tech4parking-infra): regra IoT, Lambda e demais recursos AWS
- [tech4parking-back](https://github.com/tech4parking-org/tech4parking-back): Lambda que processa as mensagens
- [tech4parking-front](https://github.com/tech4parking-org/tech4parking-front): web app
