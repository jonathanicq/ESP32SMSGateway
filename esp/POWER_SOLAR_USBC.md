# Alimentação Solar + USB-C + Bateria (ESP32 + SIM800L)

Sistema de energia autónomo para o gateway SMS (ver [`wiring.md`](wiring.md) para o wiring base do ESP32+SIM800L). Substitui o bloco "5V Power Supply" do wiring original por um sistema de bateria carregada por painel solar ou por USB-C.

**Pressupostos de dimensionamento** (instalação exterior, exposta ao tempo, 3-5 dias de autonomia sem sol nem USB):

- Consumo médio: ~150 mA (WiFi ativo a maior parte do tempo) → ~5.7 Ah/dia descarregados da bateria (contando perdas de conversão).
- Picos de envio de SMS (1.5-2 A) são breves e de baixo duty-cycle — não pesam significativamente na média diária, mas exigem corrente instantânea elevada da bateria.
- Autonomia alvo de 4 dias → capacidade de bateria necessária ≈ 5.7 Ah × 4 ≈ **23 Ah**.

> **Redução de consumo (opcional, grátis):** ativar `WiFi.setSleep(true)` no ESP32 (modem-sleep) pode cortar o consumo médio 30-50%, reduzindo proporcionalmente o tamanho/custo do banco de baterias necessário. Não implementado neste documento (é firmware, não hardware) — dizer se queres que eu adicione.

## Lista de Componentes

| Componente | Quantidade | Notas |
|---|---|---|
| Painel solar 6V ~10W, policristalino, selado/IP65 | 1 | Tipo "câmara de fototrampagem" ou farolim de jardim — já vem estanque de fábrica |
| Díodo Schottky 1N5822 | 1 | Em série painel→carregador; proteção extra contra retorno noturno (a maioria dos módulos CN3065 já tem isto embutido) |
| Módulo carregador solar CN3065 | 1 | Entrada 4.4-6V, carrega Li-ion de 1 célula, CC/CV |
| Módulo carregador Li-ion TP4056 com entrada USB-C | 1 | 1A, com proteção de sobrecarga/descarga |
| Células 18650 Li-ion **protegidas** (com PCB de proteção individual) | 6-7 | ~3400-3500mAh cada, todas em paralelo → ~21-25 Ah totais |
| Suporte/holder para 6-7x 18650 em paralelo | 1 | Ou caixa de baterias já montada com barramento comum |
| Fusível PTC resetável 3A em linha | 1 | Entre bateria e distribuição — proteção contra curto-circuito |
| Conversor buck-boost 3.3V (ex. baseado em TPS63020) | 1 | Entrada 2.5-5.5V, saída 3.3V fixa, ≥1A — só para o ESP32 |
| Condensadores 1000-2200µF 16V | 2 | Junto ao pino VCC do SIM800L, para absorver os picos de 2A |
| Condensador 100nF | 2 | Desacoplamento de alta frequência junto ao VCC do SIM800L |
| Resistências 100kΩ | 2 | Divisor resistivo para monitorização de tensão da bateria (opcional) |
| Caixa IP65/67 + prensa-cabos (x3) | 1 | Eletrónica; ver secção de estanqueidade abaixo |
| Conector USB-C IP67 de painel (ou bucim estanque) | 1 | Para permitir carregar por USB-C sem abrir a caixa |
| Saqueta de sílica gel + válvula de ventilação (tipo Gore-vent) | 1 | Gestão de condensação dentro da caixa |

## Diagrama de blocos

```
┌───────────────────┐        ┌───────────────────┐
│  Painel Solar 6V   │        │   USB-C (5V)      │
│   ~10W, IP65       │        │  (conector/cabo)  │
└─────────┬──────────┘        └─────────┬─────────┘
          │[1N5822]                     │
          ▼                             ▼
   ┌─────────────┐              ┌──────────────┐
   │   CN3065    │              │  TP4056-C    │
   │ Solar Charger│              │ USB Charger  │
   └──────┬──────┘              └──────┬───────┘
          │         BAT+/BAT-          │
          └───────────┬─────────────────┘
                       ▼
        ┌──────────────────────────────┐
        │  6-7x 18650 protegidas       │
        │  em paralelo (~21-25 Ah)     │
        └──────────────┬───────────────┘
                        │
              [Fusível PTC 3A]
                        │
          ┌─────────────┴──────────────┐
          │                             │
          ▼                             ▼
  Alimentação direta            Buck-Boost 3.3V
  do SIM800L VCC                (TPS63020 ou similar)
  (3.4-4.4V, dentro do              │
   range nativo do módulo)          ▼
          │                    ESP32 pino 3V3
   [2200µF + 100nF]            (NÃO ligar ao VIN/5V
    junto ao VCC                ao mesmo tempo)
          │
          ▼
     SIM800L GND ── comum ── ESP32 GND ── comum ── Bateria GND
```

## Porquê alimentar o SIM800L diretamente da bateria

O SIM800L aceita 3.4-4.4V nativamente — é o mesmo range de uma célula Li-ion. Ao ligá-lo direto ao barramento da bateria (em vez de subir para 5V e descer outra vez para 3.3V como no `wiring.md` original):

- Elimina-se a necessidade de um conversor boost capaz de aguentar 2A de pico (o elo mais caro/frágil da cadeia).
- A bateria (várias células em paralelo) fornece dezenas de amperes combinados sem esforço — os picos de transmissão deixam de ser um problema de conversão de tensão, só precisam do banco de condensadores local já recomendado no wiring original.
- O ESP32 fica isolado num conversor buck-boost dedicado e mais pequeno (só precisa de ~300-500mA), garantindo 3.3V estável mesmo quando a bateria descarrega até perto de 3.0V (coisa que um LDO simples não garantiria com margem suficiente).

**Importante:** com o 3.3V a entrar diretamente no pino `3V3` do devkit, não ligar simultaneamente o `VIN`/5V (USB de programação, por exemplo) sem desligar esta alimentação — os dois reguladores entrariam em conflito.

## Porquê duas células protegidas em paralelo em vez de BMS série

Como todo o sistema trabalha a tensão de célula única (3.0-4.2V), não há necessidade de um BMS de balanceamento série. Usar células **individualmente protegidas** (cada uma com o seu PCB de proteção interno) ligadas em paralelo é a forma mais simples e segura: se uma célula falhar, a sua proteção desliga-a sem afetar as restantes. Evita ter de desenhar/comprar um BMS multi-célula.

## Monitorização de bateria (opcional, precisa de firmware)

Divisor 100kΩ/100kΩ do `BAT+` para o pino `GPIO34` (ADC1 — funciona sempre, ao contrário dos pinos ADC2 que conflituam com o WiFi). Com 4.2V no `BAT+`, o pino vê ~2.1V, dentro do range seguro do ADC do ESP32. O firmware pode expor a percentagem de bateria no dashboard web já existente e disparar um aviso no Telegram quando a tensão cair abaixo de um limiar (ex. 3.4V). Não implementado aqui — diz se queres que eu adicione ao `main.cpp`/`WiFi_SMS_API.ino`.

## Estanqueidade (instalação exterior)

- Caixa da eletrónica com classificação **IP65 ou superior**, com prensa-cabos (glands) para: cabo do painel solar, cabo/conector USB-C, e cabo da antena GSM (se for externa).
- O painel solar em si já costuma vir selado de fábrica (painéis de câmaras de fototrampagem/farolins de jardim) — não precisa de caixa extra, só um bom prensa-cabos onde o fio entra na caixa principal.
- Preferir um **conector USB-C IP67 de painel** em vez de deixar uma porta USB-C comum exposta; alternativa mais barata: um pequeno alçapão/tampa com O-ring sobre a porta.
- Colocar uma saqueta de sílica gel dentro da caixa e considerar uma **válvula de ventilação tipo Gore-vent** — ajuda a equalizar pressão e evitar condensação nos ciclos dia/noite sem deixar entrar água.
- Se possível, manter o banco de baterias à sombra (dentro da caixa, não ao sol direto) — o calor acelera a degradação de células Li-ion; só o painel solar deve ficar exposto diretamente ao sol.
- Orientar o painel para sul (hemisfério norte, ex. Portugal) com inclinação ~35-40° para maximizar captação anual.

## Notas de segurança

- Nunca ligar células 18650 **não protegidas** diretamente em paralelo sem proteção externa — usar sempre células com PCB de proteção individual, ou adicionar uma proteção de célula única (ex. DW01+FS8205) por grupo.
- O fusível PTC entre a bateria e a distribuição é a única proteção contra curto-circuito no barramento principal — não omitir.
- Verificar que a soma das correntes de carga dos dois carregadores (solar + USB-C) nunca excede a capacidade de carga seguro do conjunto de células (tipicamente ~0.5C a 1C) — com 6-7 células de ~3.4Ah, isto está longe de ser um problema com os módulos aqui listados (CN3065 ~0.8-1A, TP4056 ~1A).
