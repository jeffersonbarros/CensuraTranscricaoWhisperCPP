import os
import time
import argparse
import datetime
import whisper

# Caminho raiz onde as pastas com data (ddMMyyyy) estarão
PASTA_ROOT = "/mnt/dados/censura95fm/Midias/95fm/"
# Linguagem padrão (código ISO 639-1)
LINGUA = "pt"

# Carregar modelo Whisper (pode ser "base", "small", "medium", "large")
modelo = whisper.load_model("large")


def _format_timestamp(seconds: float) -> str:
    hours = int(seconds // 3600)
    minutes = int((seconds % 3600) // 60)
    secs = seconds % 60
    return f"{hours:02d}:{minutes:02d}:{secs:06.3f}"


def transcrever_arquivo(caminho_arquivo):
    print(f"Transcrevendo: {caminho_arquivo}")
    resultado = modelo.transcribe(caminho_arquivo, language=LINGUA)

    nome_base = os.path.splitext(caminho_arquivo)[0]
    arquivo_saida = nome_base + ".vtt"

    segments = resultado.get("segments", [])

    with open(arquivo_saida, "w", encoding="utf-8") as f:
        f.write("WEBVTT\n\n")

        if segments:
            for seg in segments:
                start = _format_timestamp(seg.get("start", 0.0))
                end = _format_timestamp(seg.get("end", 0.0))
                text = seg.get("text", "").strip()
                # escrever cue
                f.write(f"{start} --> {end}\n")
                f.write(f"{text}\n\n")
        else:
            # fallback: escrever todo o texto como único cue
            texto_completo = resultado.get("text", "").strip()
            f.write("00:00:00.000 --> 00:00:00.000\n")
            f.write(f"{texto_completo}\n")

    print(f"Transcrição salva em: {arquivo_saida}")


def monitorar_pasta(start_date=None):
    arquivos_processados = set()

    # determinar data inicial como objeto date
    if start_date:
        current_date = datetime.datetime.strptime(start_date, "%d%m%Y").date()
    else:
        current_date = datetime.date.today()

    while True:
        current_pasta = os.path.join(PASTA_ROOT, current_date.strftime("%d%m%Y"))

        # aguardar até que a pasta seja criada pelo outro programa
        while not os.path.isdir(current_pasta):
            print(f"Pasta não existe ainda, aguardando: {current_pasta}")
            time.sleep(10)

        # listar arquivos na pasta atual
        try:
            lista_arquivos = os.listdir(current_pasta)
        except FileNotFoundError:
            lista_arquivos = []

        # filtrar mp3
        mp3_files = [f for f in lista_arquivos if f.lower().endswith('.mp3')]

        if not mp3_files:
            # pasta existe mas ainda não há arquivos mp3: aguardar até que um seja criado
            print(f"Nenhum .mp3 na pasta {current_pasta}, aguardando por novos arquivos...")
            time.sleep(10)
            continue

        unprocessed = []
        for arquivo in mp3_files:
            caminho = os.path.join(current_pasta, arquivo)
            nome_base = os.path.splitext(caminho)[0]
            arquivo_vtt = nome_base + ".vtt"

            # se já existe .vtt correspondente, pular e marcar como processado
            if os.path.exists(arquivo_vtt):
                print(f"VTT já existe, pulando: {arquivo_vtt}")
                arquivos_processados.add(caminho)
                continue

            if caminho not in arquivos_processados:
                unprocessed.append(caminho)

        if unprocessed:
            for caminho in unprocessed:
                transcrever_arquivo(caminho)
                arquivos_processados.add(caminho)
            # após processar, continuar loop para checar se surgiram novos arquivos
            continue
        else:
            # não há mp3 não processados na pasta atual -> avançar 1 dia sequencialmente
            current_date = current_date + datetime.timedelta(days=1)
            next_pasta = os.path.join(PASTA_ROOT, current_date.strftime("%d%m%Y"))
            print(f"Avançando para a próxima pasta (aguardando se necessário): {next_pasta}")
            # o loop principal vai aguardar a existência da próxima pasta
            time.sleep(1)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--start-date", help="Data inicial no formato ddMMyyyy para iniciar a transcrição (opcional)", default=None)
    args = parser.parse_args()

    start_date = None
    if args.start_date:
        try:
            datetime.datetime.strptime(args.start_date, "%d%m%Y")
            start_date = args.start_date
        except ValueError:
            print("Formato de data inválido. Use ddMMyyyy.")
            exit(1)

    # Não criar pastas: assumir que outro processo cria-as; aguardar quando necessário
    monitorar_pasta(start_date=start_date)