# EduStream — Sistema de Transmissão de Aulas ao Vivo (CMP1103)

MVP de *live streaming* educacional desenvolvido em **C++17**, **GStreamer** e **GTK4** para a disciplina de Sistemas Multimídia (**CMP1103**). O sistema combina ingestão confiável via **SRT**, redistribuição centralizada com **MediaMTX** e recepção de baixa latência via **RTSP/RTP sobre UDP**, com instrumentação em tempo real do `rtpjitterbuffer` para avaliação quantitativa de Requisitos Não Funcionais (RNFs).

## 1. Arquitetura Geral

```text
+-----------------------------------+        SRT (UDP :8890)        +----------------------+
|  Professor (Transmissor)          | ----------------------------> |      MediaMTX        |
|  C++17 + GStreamer + GTK4         |   H.264 + Opus em MPEG-TS     |  (Servidor Central)  |
|  edustream_professor / CLI        |                               +----------+-----------+
+-----------------------------------+                                          |
                                                                               |
                                            +----------------------------------+----------------------------------+
                                            | RTSP (:8554) + RTP/UDP (:8000-8001)                                 | WebRTC (:8889 / :8189)
                                            v                                                                     v
                                  +-----------------------------------+                                 +----------------------+
                                  |  Aluno (Cliente Nativo)           |                                 |  Navegador (Plano B) |
                                  |  C++17 + GStreamer + GTK4         |                                 |  http://<IP>:8889/   |
                                  |  edustream_aluno / CLI + Métricas |                                 |  aula/               |
                                  +-----------------------------------+                                 +----------------------+
```

* **Codificação:** Vídeo **H.264** (`x264enc` na publicação / `openh264dec` na recepção) e áudio **Opus** (`opusenc` / `opusdec`).
* **Publicação (Professor -> Servidor):** MPEG-TS encapsulado em **SRT** (`srt://<host>:8890?streamid=publish:aula&latency=50`).
* **Distribuição (Servidor -> Aluno):** Sinalização **RTSP** (`rtsp://<host>:8554/aula`) com transporte de mídia **RTP/UDP** e controle configurável de latência no `rtpjitterbuffer`.
* **Contingência (Plano B):** Acesso direto via navegador em **WebRTC** (`http://<host>:8889/aula/`).

---

## 2. Executáveis Gerados

| Binário | Papel | Descrição e Uso |
|---|---|---|
| `edustream_professor` | Professor (GUI) | Painel desktop GTK4 para selecionar fonte (`Teste` ou `Webcam + Microfone`), definir URL SRT e iniciar/encerrar a transmissão. |
| `edustream_aluno` | Aluno (GUI) | Painel desktop GTK4 com URL RTSP, controle de latência do `rtpjitterbuffer` (ms) e painel de métricas ao vivo. O vídeo abre em janela nativa do GStreamer. |
| `edustream_transmissor_cli` | Professor (CLI) | `./build/edustream_transmissor_cli [url_srt] [test\|cam]` |
| `edustream_receptor_cli` | Aluno (CLI) | `./build/edustream_receptor_cli [url_rtsp] [latency_ms] [arquivo_csv]` — imprime `[STATS]`, `[LOSS]`, `[DROP]` e exporta CSV a cada 2 s. |

> **Modo somente CLI (sem GTK4):** compile passando `-DBUILD_GUI=OFF` ao CMake.

---

## 3. Estrutura do Repositório Público Final

```text
cmp1103-aed-edustream/
├── .gitignore
├── CMakeLists.txt
├── README.md
├── docs/
│   ├── diagramas/                              # Diagramas de arquitetura e pipeline
│   └── evidencias/                             # Prints selecionados de validação (GUI e rede real)
├── resultados/
│   ├── c1_normal_50ms.csv                      # Cenário 1: rede normal, buffer 50 ms
│   ├── c2_normal_200ms.csv                     # Cenário 2: rede normal, buffer 200 ms
│   ├── c3_jitter_50ms.csv                      # Cenário 3: jitter 50ms +/- 40ms, buffer 50 ms
│   ├── c4_jitter_200ms.csv                     # Cenário 4: jitter 50ms +/- 40ms, buffer 200 ms
│   ├── c5_perda20_50ms.csv                     # Cenário 5: perda 20% + jitter, buffer 50 ms
│   ├── c6_perda20_1000ms.csv                   # Cenário 6: perda 20% + jitter, buffer 1000 ms
│   ├── fig1_perda_decomposta_jitter.png        # Gráfico: perda decomposta (rede vs. atraso) e jitter
│   ├── fig2_evolucao_temporal.png              # Gráfico: evolução temporal de perda e jitter
│   ├── tabela1_comparativa_6_cenarios.csv      # Tabela consolidada (CSV)
│   ├── tabela1_comparativa_6_cenarios.png      # Tabela 1 renderizada (300 DPI)
│   ├── tabela2_principais_achados.png          # Tabela 2 renderizada (300 DPI)
│   ├── tabelas_resultados.md                   # Tabelas em Markdown
│   └── tabelas_resultados.tex                  # Tabelas em LaTeX (booktabs)
├── scripts/
│   ├── gerar_graficos.py                       # Gera as figuras em 300 DPI a partir dos CSVs
│   └── gerar_tabelas.py                        # Gera tabelas PNG, CSV, Markdown e LaTeX
├── server/
│   └── mediamtx.yml                            # Configuração do MediaMTX (baixar binário localmente)
└── src/
    ├── cli/
    │   ├── receptor_cli.cpp
    │   └── transmissor_cli.cpp
    ├── core/
    │   ├── config.hpp
    │   ├── receiver.cpp
    │   ├── receiver.hpp
    │   ├── transmitter.cpp
    │   └── transmitter.hpp
    └── ui/
        ├── aluno_gui.cpp
        └── professor_gui.cpp
```

> **Nota:** o diretório `build/`, ambientes virtuais `.venv/` e o binário executável do `mediamtx` não são versionados no repositório público.

---

## 4. Preparação e Execução no Fedora Linux (Professor e Aluno)

Esta seção reúne **todos os comandos necessários** para preparar do zero tanto a máquina do **Professor (Servidor + Transmissor)** quanto a máquina do **Aluno (Receptor)** no Fedora Linux.

### 4.1. Clonar o repositório (Professor e Aluno)

```bash
git clone git@github.com:conan0028/cmp1103-aed-edustream.git
cd cmp1103-aed-edustream
```

### 4.2. Instalar dependências (Professor e Aluno)

1. **Habilitar o repositório OpenH264** (necessário para o decodificador `openh264dec` no Aluno):

```bash
sudo dnf config-manager setopt fedora-cisco-openh264.enabled=1
```

2. **Habilitar o RPM Fusion Free** (necessário para o codificador `x264enc` no Professor):

```bash
sudo dnf install -y \
  https://mirrors.rpmfusion.org/free/fedora/rpmfusion-free-release-$(rpm -E %fedora).noarch.rpm
```

3. **Instalar compilador, CMake, GTK4 e plugins GStreamer:**

* **Instalação completa (serve para Professor e Aluno na mesma máquina):**

```bash
sudo dnf install -y \
  git gcc-c++ cmake pkgconf-pkg-config \
  gtk4-devel \
  gstreamer1-devel gstreamer1-plugins-base gstreamer1-plugins-base-devel \
  gstreamer1-plugins-good gstreamer1-plugins-bad-free gstreamer1-plugins-bad-free-extras \
  gstreamer1-plugins-ugly gstreamer1-plugin-openh264
```

* **Somente máquina do Aluno (caso não vá transmitir a partir dela):** o mesmo comando acima pode ser usado removendo `gstreamer1-plugins-ugly` (não exige RPM Fusion).

4. **Validar os plugins críticos do GStreamer:**

```bash
# Validação do lado do Professor (Transmissor)
gst-inspect-1.0 x264enc opusenc mpegtsmux srtsink > /dev/null && echo "Plugins Professor OK"

# Validação do lado do Aluno (Receptor)
gst-inspect-1.0 rtspsrc rtpjitterbuffer openh264dec opusdec > /dev/null && echo "Plugins Aluno OK"
```

### 4.3. Compilar o projeto (Professor e Aluno)

```bash
cmake -S . -B build -DBUILD_GUI=ON
cmake --build build
```

*(Para compilar apenas os executáveis de linha de comando sem GTK4, utilize `-DBUILD_GUI=OFF`.)*

### 4.4. Configurar o Servidor MediaMTX e Firewall (Somente máquina do Professor/Servidor)

1. **Baixar o binário do MediaMTX (Linux amd64):**
   * Acesse [bluenviron/mediamtx/releases](https://github.com/bluenviron/mediamtx/releases), baixe `mediamtx_v1.x.x_linux_amd64.tar.gz` e extraia apenas o executável `mediamtx` para a pasta `server/`.
   * Garanta permissão de execução:

```bash
chmod +x server/mediamtx
```

2. **Descobrir o IP local e o nome da interface de rede ativa:**

```bash
ip -4 -br addr
```

Anote o nome da interface `UP` (ex.: `enp37s0` para cabo ou `wlp...` para Wi-Fi) e o endereço IPv4 (ex.: `192.168.4.14`).

3. **Liberar as portas no `firewalld` do Fedora** (obrigatório quando o Aluno está em outra máquina):

Verifique qual zona está ativa na sua interface:

```bash
firewall-cmd --get-active-zones
```

Libere as portas temporariamente na zona padrão (as regras expiram no próximo reboot; adicione `--permanent` e depois `--reload` se quiser fixá-las):

```bash
sudo firewall-cmd \
  --add-port=8554/tcp \
  --add-port=8000-8001/udp \
  --add-port=8889/tcp \
  --add-port=8189/udp \
  --add-port=8890/udp
```

*(Se sua interface estiver em outra zona, como `public`, acrescente `--zone=public` ao comando acima.)*

### 4.5. Executar no Fedora Linux

#### A. Máquina do Professor (Servidor + Transmissor)

Abra dois terminais na raiz do repositório:

```bash
# Terminal 1 — Subir o servidor MediaMTX
./server/mediamtx server/mediamtx.yml

# Terminal 2 — Abrir interface gráfica do Professor
./build/edustream_professor
```

* Na janela **EduStream — Professor**, mantenha `srt://127.0.0.1:8890?streamid=publish:aula&latency=50`, escolha **`Teste (Barras + 440 Hz)`** ou **`Webcam + Microfone`** e clique em **Iniciar Aula**.
* **Alternativa via CLI:**

```bash
./build/edustream_transmissor_cli                                                        # modo test (padrão)
./build/edustream_transmissor_cli "srt://127.0.0.1:8890?streamid=publish:aula&latency=50" cam   # modo webcam
```

#### B. Máquina do Aluno (Receptor)

Substitua `<IP_SERVIDOR>` por `127.0.0.1` (teste local) ou pelo IPv4 do Professor na rede (ex.: `192.168.4.14`):

```bash
# Testar alcance de rede (em duas máquinas)
ping -c 3 <IP_SERVIDOR>

# Abrir interface gráfica do Aluno
./build/edustream_aluno
```

* Na janela **EduStream — Aluno**, informe `rtsp://<IP_SERVIDOR>:8554/aula`, ajuste a **Latência do jitterbuffer (ms)** (padrão `200`) e clique em **Acessar Aula**.
* **Alternativa via CLI (com ou sem exportação CSV):**

```bash
./build/edustream_receptor_cli "rtsp://<IP_SERVIDOR>:8554/aula" 200
./build/edustream_receptor_cli "rtsp://<IP_SERVIDOR>:8554/aula" 200 resultados/teste_200ms.csv
```

* **Plano B via Navegador (WebRTC):** abra `http://<IP_SERVIDOR>:8889/aula/`.

---

## 5. Preparação e Execução no Windows (MSYS2 UCRT64)

O projeto é portável para Windows via ambiente **MSYS2 UCRT64** (compilador MinGW-w64 + GTK4 + GStreamer nativos). Na captura de câmera no Windows, o código seleciona automaticamente `autovideosrc` no lugar de `v4l2src`.

### 5.1. Instalar o MSYS2 e dependências

1. Baixe e instale o MSYS2 em [https://www.msys2.org](https://www.msys2.org).
2. Abra o terminal **MSYS2 UCRT64** (não use o MSYS padrão nem o MINGW32).
3. Atualize a base e instale `git`, toolchain C++17, GTK4 e plugins GStreamer:

```bash
pacman -Syu
pacman -S --needed \
  git \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-pkgconf \
  mingw-w64-ucrt-x86_64-gtk4 \
  mingw-w64-ucrt-x86_64-gstreamer \
  mingw-w64-ucrt-x86_64-gst-plugins-base \
  mingw-w64-ucrt-x86_64-gst-plugins-good \
  mingw-w64-ucrt-x86_64-gst-plugins-bad \
  mingw-w64-ucrt-x86_64-gst-plugins-ugly \
  mingw-w64-ucrt-x86_64-openh264
```

4. Confirme se todos os plugins necessários foram encontrados:

```bash
gst-inspect-1.0 x264enc srtsink mpegtsmux openh264dec opusenc opusdec rtspsrc rtpjitterbuffer > /dev/null && echo "Plugins Windows OK"
```

### 5.2. Clonar e compilar no Windows

```bash
git clone git@github.com:conan0028/cmp1103-aed-edustream.git
cd cmp1103-aed-edustream
cmake -S . -B build -G Ninja -DBUILD_GUI=ON
cmake --build build
```

*(Caso não tenha chave SSH configurada no Windows, utilize a URL HTTPS equivalente do repositório.)*

### 5.3. Baixar o MediaMTX (somente se o Windows for atuar como Servidor/Professor)

* Baixe `mediamtx_v1.x.x_windows_amd64.zip` em [bluenviron/mediamtx/releases](https://github.com/bluenviron/mediamtx/releases) e extraia `mediamtx.exe` para dentro da pasta `server/`.
* Para descobrir o IP local do Windows (se ele for o servidor), execute `ipconfig`.

### 5.4. Executar no Windows (terminais MSYS2 UCRT64)

```bash
# Se o Windows rodar o servidor (Terminal 1)
./server/mediamtx.exe server/mediamtx.yml

# Painel do Professor (Terminal 2)
./build/edustream_professor.exe

# Painel do Aluno (Terminal 3)
./build/edustream_aluno.exe
```

* **Conectando a um servidor Linux na mesma rede:** no painel **EduStream — Aluno**, informe `rtsp://<IP_LINUX>:8554/aula` e clique em **Acessar Aula**.
* **Firewall do Windows:** na primeira execução, caso o Windows Defender Firewall exiba o alerta de segurança para `edustream_aluno.exe` ou `mediamtx.exe`, clique em **Permitir acesso** (redes privadas).
* **Plano B via Navegador (WebRTC):** abra `http://<IP_SERVIDOR>:8889/aula/`.

---

## 6. Referência de Portas em Rede Local (LAN / Wi-Fi)

| Porta / Protocolo | Direção | Finalidade |
|---|---|---|
| `8890/udp` | Professor -> MediaMTX | Publicação SRT (MPEG-TS) — dispensável se ambos rodam em `127.0.0.1` |
| `8554/tcp` | Aluno -> MediaMTX | Sinalização RTSP (`DESCRIBE`, `SETUP`, `PLAY`, `TEARDOWN`) |
| `8000-8001/udp` | MediaMTX <-> Aluno | Transporte de mídia RTP e controle RTCP |
| `8889/tcp` | Navegador -> MediaMTX | Página e sinalização HTTP do WebRTC (Plano B) |
| `8189/udp` | MediaMTX <-> Navegador | Fluxo de mídia ICE/WebRTC (Plano B) |

---

## 7. Coleta de Métricas (RNFs) e Emulação de Rede (`tc netem`)

O executável `edustream_receptor_cli` consulta as estatísticas internas do `rtpjitterbuffer` a cada 2 segundos e grava um CSV com `flush()` imediato por linha:

```bash
./build/edustream_receptor_cli "rtsp://<IP_SERVIDOR>:8554/aula" 200 resultados/ensaio.csv
```

**Colunas geradas:**
`elapsed_s,latency_ms,pushed,lost,late,duplicates,avg_jitter_ms,loss_rate_pct`

### Emulação de jitter e perda com `tc netem` (Linux)

O `tc netem` atua exclusivamente no tráfego de **saída (egress)** da interface de rede. Aplicá-lo na máquina do Aluno afetaria apenas pacotes enviados por ele (como RTCP), sem degradar o fluxo RTP/UDP recebido. Portanto, em ensaios com **duas máquinas**, aplique o `netem` na **interface de saída da máquina onde roda o MediaMTX** (ex.: `enp37s0`). Como a publicação SRT entre Professor e MediaMTX ocorre em `127.0.0.1` (`lo`), apenas o enlace servidor -> aluno é degradado.

```bash
# 1. Identificar a interface física do servidor
ip -br link

# 2. Aplicar jitter puro (50 ms +/- 40 ms) — Cenários C3 e C4
sudo tc qdisc add dev <interface> root netem delay 50ms 40ms

# 3. Substituir por perda de 20% + jitter — Cenários C5 e C6
sudo tc qdisc replace dev <interface> root netem loss 20% delay 50ms 40ms

# 4. LIMPEZA OBRIGATÓRIA ao encerrar os testes
sudo tc qdisc del dev <interface> root
tc qdisc show dev <interface>   # confirmar que 'netem' não aparece mais
```

> **Atenção:** nunca esqueça de executar `sudo tc qdisc del dev <interface> root` ao final dos testes, pois a degradação permanece ativa até ser removida ou a máquina reiniciar.

---

## 8. Reprodução dos Gráficos e Tabelas de Resultados

Os resultados experimentais dos 6 cenários (`C1` a `C6`, coletados entre Desktop Ethernet e Notebook Wi-Fi) estão armazenados em `resultados/` e podem ser recalculados e regenerados em 300 DPI via Python:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install pandas matplotlib

# Executar a partir da raiz do repositório
python3 scripts/gerar_graficos.py
python3 scripts/gerar_tabelas.py
```

Os scripts geram automaticamente:
* `resultados/fig1_perda_decomposta_jitter.png` e `resultados/fig2_evolucao_temporal.png`
* `resultados/tabela1_comparativa_6_cenarios.png` e `resultados/tabela2_principais_achados.png`
* `resultados/tabela1_comparativa_6_cenarios.csv`, `resultados/tabelas_resultados.md` e `resultados/tabelas_resultados.tex`