# CensuraTranscricaoWhisperCPP

Este projeto contém uma versão em C++ de um script que monitora pastas com data (formato `ddMMyyyy`) e chama `whisper-cli` (whisper.cpp) para gerar transcrições no formato VTT a partir de arquivos `.mp3`.

A seguir estão os passos para instalar, compilar, executar e configurar como um serviço no Ubuntu.

## Requisitos
- Ubuntu 18.04+ (ou similar)
- Compilador C++ compatível com C++20 (g++ / clang)
- `cmake` (≥ 3.10)
- `ninja-build` (opcional se usar o gerador Ninja)
- `whisper-cli` (parte do projeto `whisper.cpp`) e modelos compatíveis (.bin)

Instalar dependências básicas no Ubuntu:

```sh
sudo apt update
sudo apt install -y build-essential cmake ninja-build git pkg-config
```

Observação: se preferir usar `make`/Unix Makefiles substitua os passos com `-G Ninja` por `-G "Unix Makefiles"` e use `make`.

## Obter/instalar `whisper-cli` e modelos
O programa chama um executável chamado `whisper-cli`. Esse binário faz parte do projeto `whisper.cpp` (ou outra build compatível). Instale/compile `whisper.cpp` conforme a documentação do projeto e deixe o executável disponível no `PATH` ou informe o caminho completo.

Baixe um modelo (por exemplo `ggml-small.bin`) e coloque-o em algum diretório acessível. No código existe uma constante `DEFAULT_MODEL_PATH` que aponta para um caminho padrão; você pode alterá-lo no código-fonte ou passar parâmetros ao executar.

## Compilar o projeto (Ubuntu shell)
No diretório raíz do projeto (onde está o `CMakeLists.txt`):

```sh
mkdir -p build
cd build
cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
# ou simplesmente: ninja
```

Ao final, o executável será gerado dentro de `build/` (ou conforme definido no `CMakeLists.txt`). Anote o caminho do binário gerado.

## Executar manualmente
Execute o binário diretamente para testes. Exemplos de argumentos suportados pelo programa:

- `--start-date ddMMyyyy` — pasta inicial no formato `ddMMyyyy` (opcional)
- `--model` — nome do modelo (se aplicável ao seu `whisper-cli`)
- `--model-dir` — caminho do modelo (pode sobrescrever `DEFAULT_MODEL_PATH`)
- `--language` — código de idioma (ex: `pt`)

Exemplo:

```sh
./censura_transcricao --start-date 01012024 --model-dir "/home/usuario/whisper_models/ggml-small.bin" --language pt
```

Se preferir instalar o binário globalmente:

```sh
sudo cp build/censura_transcricao /usr/local/bin/
sudo chmod +x /usr/local/bin/censura_transcricao
```

## Configurar como serviço systemd (iniciar automaticamente no boot)
Crie uma unidade systemd para executar a aplicação como serviço. Exemplo de arquivo de serviço `censura-transcricao.service`:

```ini
[Unit]
Description=Censura Transcricao (whisper.cpp watcher)
After=network.target

[Service]
Type=simple
User=youruser
WorkingDirectory=/home/youruser
ExecStart=/usr/local/bin/censura_transcricao --model-dir "/home/youruser/whisper_models/ggml-small.bin" --language pt
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

Substitua `youruser`, `WorkingDirectory` e o caminho para o binário / modelo conforme necessário.

Para instalar e habilitar o serviço:

```sh
# copie o arquivo de unidade (ex: usando sudo tee)
sudo tee /etc/systemd/system/censura-transcricao.service > /dev/null << 'EOF'
[Unit]
Description=Censura Transcricao (whisper.cpp watcher)
After=network.target

[Service]
Type=simple
User=youruser
WorkingDirectory=/home/youruser
ExecStart=/usr/local/bin/censura_transcricao --model-dir "/home/youruser/whisper_models/ggml-small.bin" --language pt
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable --now censura-transcricao.service
sudo systemctl status censura-transcricao.service
```

Ver logs do serviço:

```sh
sudo journalctl -u censura-transcricao.service -f
```

## Notas e solução de problemas
- Certifique-se de que `whisper-cli` seja executável e visível para o usuário do serviço.
- Verifique permissões de leitura das pastas monitoradas e do modelo `.bin`.
- Se o `whisper-cli` escrever saída para `stdout`, o programa tenta redirecionar para `arquivo.vtt`, mas diferentes builds podem ter opções distintas; adapte a chamada no código-fonte (`CensuraTranscricaoWhisperCPP.cpp`) se necessário.
- Ajuste `DEFAULT_MODEL_PATH` no código-fonte ou passe `--model-dir` ao executar.

---

Se precisar de ajuda específica para o build do `whisper.cpp` ou para ajustar o `CMakeLists.txt`, inclua os arquivos do projeto e eu posso orientar os passos seguintes.
