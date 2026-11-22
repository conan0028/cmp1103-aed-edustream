# Tabelas de Resultados — EduStream (`rtpjitterbuffer`)

## Tabela 1 — Comparativa dos 6 cenários

| Cenário | Rede (tc netem em enp37s0) | Latência (ms) | Duração (s) | Recebidos (pushed) | Perdidos (lost) | Atrasados (late) | Duplicados | Perda final (%) | Jitter médio ± DP (ms) | Jitter mediana [mín–máx] (ms) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| C1 | Normal (sem netem) | 50 | 64 | 15 270 | 2 | 0 | 0 | 0,01% | 9,06 ± 1,91 | 8,25 [7,86–15,65] |
| C2 | Normal (sem netem) | 200 | 66 | 15 839 | 0 | 0 | 0 | 0,00% | 8,62 ± 1,13 | 8,23 [8,08–12,84] |
| C3 | delay 50ms 40ms | 50 | 82 | 18 385 | 1 098 | 1 089 | 9 | 5,64%* | 25,25 ± 8,69 (29,34 ± 2,43*) | 28,59 [8,26–35,26] |
| C4 | delay 50ms 40ms | 200 | 67 | 15 821 | 5 | 3 | 0 | 0,03% | 29,60 ± 2,92 | 29,70 [24,10–35,15] |
| C5 | loss 20% delay 50ms 40ms | 50 | 74 | 12 283 | 5 063 | 1 549 | 5 | 29,19% | 29,46 ± 3,55 | 29,33 [22,75–38,66] |
| C6 | loss 20% delay 50ms 40ms | 1000 | 66 | 12 401 | 3 125 | 0 | 0 | 20,13% | 29,27 ± 3,52 | 29,32 [20,30–35,43] |

*Em C3, a regra tc netem foi ativada após o arranque do vídeo (aos 18 s, passando o jitter de 8,44 ms para 29,25 ms). Isolando apenas a janela de regime estacionário (18 s a 82 s), o jitter médio foi de 29,34 ± 2,43 ms e a taxa de perda efetiva foi de 6,84% (1 045 perdas, das quais 1 037 — ou 99,23% — por atraso).*

## Tabela 2 — Síntese dos principais achados

| Dimensão Avaliada | Cenários | Perda Real na Rede | Descarte por Atraso (late) | Perda Total Efetiva | Principal Achado |
| --- | --- | --- | --- | --- | --- |
| 1. Linha de base Ethernet → Wi-Fi | C1 vs. C2 (50 vs. 200 ms) | 0,01% → 0,00% | 0 → 0 (0,00%) | 0,01% → 0,00% | Jitter estável (8,62–9,06 ms) e integridade quase total sem degradação artificial (late = 0). |
| 2. Absorção de jitter puro (±40 ms) | C3 vs. C4 (50 vs. 200 ms) | 0,05% → 0,01% | 1 089 → 3 (-99,72%) | 5,64%* → 0,03% (-99,44%) | Em 50 ms, 99,2% das perdas foram por atraso; o buffer de 200 ms absorveu o jitter mantendo baixa latência. |
| 3. Perda 20% + jitter (±40 ms) | C5 vs. C6 (50 vs. 1000 ms) | 20,26% → 20,13% | 1 549 (8,93%) → 0 (0,00%) | 29,19% → 20,13% (-9,06 p.p.) | Buffer de 1 000 ms zerou descartes por atraso (late = 0), convergindo ao piso físico de 20% do RTP/UDP sem retransmissão. |
