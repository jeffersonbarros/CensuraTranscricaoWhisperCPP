// CensuraTranscricaoWhisperCPP.cpp
// Reescrita em C++ do script Python fornecido.
// Observações:
// - Usa `whisper-cli` (whisper.cpp) para transcrição. O executável e as opções
//   podem variar conforme a instalação; o programa tenta chamar `whisper-cli`
//   e redirecionar a saída VTT para o arquivo .vtt correspondente.

#include <filesystem>
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace fs = std::filesystem;

//pasta de testes
static const std::string PASTA_ROOT = "/mnt/censura95fm/censura95fm/Midias/95fm/";
//pasta de produção
//static const std::string PASTA_ROOT = "/mnt/dados/censura95fm/Midias/95fm/";
static const std::string DEFAULT_LANG = "pt";
// Caminho padrão do modelo (aponta diretamente para o arquivo .bin do modelo)
static const std::string DEFAULT_MODEL_PATH = "/home/jefferson/whisper.cpp/models/ggml-base.bin";

// Executa o comando shell e retorna o código de saída
int run_command(const std::string &cmd) {
    // usa /bin/sh -c para garantir redirecionamento e expansão funcionem
    std::string full = "/bin/sh -c '" + cmd + "'";
    return std::system(full.c_str());
}

// Chama whisper-cli para transcrever o arquivo de áudio e salvar .vtt
bool transcrever_arquivo(const fs::path &caminho_arquivo, const std::string &model, const std::string &lang) {
    std::cout << "Transcrevendo: " << caminho_arquivo << std::endl;

    fs::path nome_base = caminho_arquivo;
    nome_base.replace_extension();
    fs::path arquivo_saida = nome_base;
    arquivo_saida += ".vtt";
    // Monta comando. Tenta redirecionar a saída VTT diretamente para o arquivo
    // de saída no mesmo diretório do arquivo de entrada. Algumas versões do
    // CLI usam flags longas/curtas diferentes, então tentamos variantes.
    std::ostringstream cmd;
    // tentativa: whisper-cli escrevendo vtt em stdout -> redirecionar para arquivo
    cmd << "whisper-cli -m '" << model << "' -f '" << caminho_arquivo.string() << "' -l '" << lang << "' -ovtt > '" << arquivo_saida.string() << "'";
    // alternativa com flags longas
    cmd << " || whisper-cli --model '" << model << "' --file '" << caminho_arquivo.string() << "' --language '" << lang << "' --output-format vtt > '" << arquivo_saida.string() << "'";
    // outra alternativa (ex.: python-whisper) que escreve em um diretório de saída
    cmd << " || whisper --model '" << model << "' --language '" << lang << "' --output_format vtt -o '" << arquivo_saida.parent_path().string() << "' '" << caminho_arquivo.string() << "'";

    int rc = run_command(cmd.str());
    if (rc != 0) {
        std::cerr << "Aviso: execução do whisper-cli retornou codigo " << rc << " para arquivo " << caminho_arquivo << std::endl;
        // mesmo que rc != 0, pode ter gerado o arquivo; iremos verificar abaixo
    }

    // Se o arquivo não foi gerado no local esperado, procurar por possíveis
    // arquivos .vtt gerados em outros diretórios e mover para o diretório do
    // arquivo de entrada com o mesmo nome base.
    if (!fs::exists(arquivo_saida)) {
        // locais a procurar: diretório do arquivo de entrada e diretório atual
        std::vector<fs::path> candidatos;
        candidatos.push_back(caminho_arquivo.parent_path());
        candidatos.push_back(fs::current_path());

        bool moved = false;
        std::string stem = caminho_arquivo.stem().string();
        for (const auto &dir : candidatos) {
            try {
                if (!fs::is_directory(dir)) continue;
                for (auto &ent : fs::directory_iterator(dir)) {
                    if (!ent.is_regular_file()) continue;
                    if (ent.path().extension() != ".vtt") continue;
                    std::string fname = ent.path().stem().string();
                    // aceitar exatas ou que contenham o stem (caso alguma ferramenta
                    // concatene extensões)
                    if (fname == stem || fname.find(stem) != std::string::npos) {
                        // mover/renomear para o path desejado
                        try {
                            fs::rename(ent.path(), arquivo_saida);
                        } catch (const fs::filesystem_error &e) {
                            // se rename falhar (diferente device), copiar e remover
                            try {
                                fs::copy_file(ent.path(), arquivo_saida, fs::copy_options::overwrite_existing);
                                fs::remove(ent.path());
                            } catch (...) {
                                continue;
                            }
                        }
                        std::cout << "Transcrição encontrada em " << dir << " e movida para: " << arquivo_saida << std::endl;
                        moved = true;
                        break;
                    }
                }
            } catch (const fs::filesystem_error &e) {
                // ignorar erros de leitura de diretório
                continue;
            }
            if (moved) break;
        }

        if (!moved) {
            std::cerr << "Aviso: arquivo de saída não foi gerado: " << arquivo_saida << std::endl;
            return false;
        }
    }

    std::cout << "Transcrição salva em: " << arquivo_saida << std::endl;
    return true;
}

// Converte string ddMMyyyy para std::tm/ std::chrono::system_clock::time_point
bool parse_date_ddMMyyyy(const std::string &s, std::tm &out_tm) {
    if (s.size() != 8) return false;
    std::istringstream ss(s);
    ss >> std::setw(2) >> std::setfill('0');
    int d = std::stoi(s.substr(0,2));
    int m = std::stoi(s.substr(2,2));
    int y = std::stoi(s.substr(4,4));
    if (m < 1 || m > 12 || d < 1 || d > 31) return false;
    out_tm = std::tm();
    out_tm.tm_mday = d;
    out_tm.tm_mon = m - 1;
    out_tm.tm_year = y - 1900;
    return true;
}

std::string format_date_ddMMyyyy(const std::tm &t) {
    std::ostringstream ss;
    ss << std::setw(2) << std::setfill('0') << t.tm_mday
       << std::setw(2) << std::setfill('0') << (t.tm_mon + 1)
       << std::setw(4) << std::setfill('0') << (t.tm_year + 1900);
    return ss.str();
}

int main(int argc, char **argv) {
    std::string start_date_str;
    std::string model = "base"; // default model name (ou nome relativo)
    std::string model_dir = DEFAULT_MODEL_PATH; // default model path ou diretório (pode ser sobrescrito)
    std::string lang = DEFAULT_LANG;

    // Arg parsing simples
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--start-date" && i + 1 < argc) {
            start_date_str = argv[++i];
        } else if (a == "--model" && i + 1 < argc) {
            model = argv[++i];
        } else if (a == "--model-dir" && i + 1 < argc) {
            model_dir = argv[++i];
        } else if (a == "--language" && i + 1 < argc) {
            lang = argv[++i];
        } else if (a == "--help" || a == "-h") {
            std::cout << "Uso: " << argv[0] << " [--start-date ddMMyyyy] [--model MODEL] [--model-dir MODEL_DIR] [--language LANG]" << std::endl;
            return 0;
        }
    }

    std::tm current_tm{};
    if (!start_date_str.empty()) {
        if (!parse_date_ddMMyyyy(start_date_str, current_tm)) {
            std::cerr << "Formato de data invalido. Use ddMMyyyy." << std::endl;
            return 1;
        }
    } else {
        // hoje
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        current_tm = *std::localtime(&now);
    }

    std::set<std::string> arquivos_processados;

    while (true) {
        std::string pasta_nome = format_date_ddMMyyyy(current_tm);
        fs::path current_pasta = fs::path(PASTA_ROOT) / pasta_nome;

        // aguardar pasta
        while (!fs::is_directory(current_pasta)) {
            std::cout << "Pasta nao existe ainda, aguardando: " << current_pasta << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }

        std::vector<fs::path> mp3_files;
        try {
            for (auto &entry : fs::directory_iterator(current_pasta)) {
                if (!entry.is_regular_file()) continue;
                auto p = entry.path();
                std::string ext = p.extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".mp3") mp3_files.push_back(p);
            }
        } catch (const fs::filesystem_error &e) {
            mp3_files.clear();
        }

        if (mp3_files.empty()) {
            std::cout << "Nenhum .mp3 na pasta " << current_pasta << ", aguardando por novos arquivos..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(10));
            continue;
        }

        std::vector<fs::path> unprocessed;
        for (auto &p : mp3_files) {
            fs::path nome_base = p;
            nome_base.replace_extension();
            fs::path arquivo_vtt = nome_base;
            arquivo_vtt += ".vtt";

            if (fs::exists(arquivo_vtt)) {
                std::cout << "VTT ja existe, pulando: " << arquivo_vtt << std::endl;
                arquivos_processados.insert(p.string());
                continue;
            }

            if (arquivos_processados.find(p.string()) == arquivos_processados.end()) {
                unprocessed.push_back(p);
            }
        }

        if (!unprocessed.empty()) {
            for (auto &p : unprocessed) {
                // preparar paths
                fs::path nome_base = p;
                nome_base.replace_extension();
                fs::path arquivo_vtt = nome_base;
                arquivo_vtt += ".vtt";

                // Se o VTT já existe, pular
                if (fs::exists(arquivo_vtt)) {
                    std::cout << "VTT ja existe, pulando: " << arquivo_vtt << std::endl;
                    arquivos_processados.insert(p.string());
                    continue;
                }

                // Tentar criar um lock atomic usando create_directory em um diretório de lock.
                // create_directory é atomic e falhará se outro processo já tiver criado o lock.
                fs::path lock_dir = nome_base;
                lock_dir += ".transcribing.lock";
                bool lock_created = false;
                try {
                    lock_created = fs::create_directory(lock_dir);
                } catch (const fs::filesystem_error &e) {
                    lock_created = false;
                }

                if (!lock_created) {
                    std::cout << "Arquivo ja esta sendo transcrito por outro processo, pulando: " << p << std::endl;
                    continue;
                }

                // Verificar se o arquivo foi modificado recentemente (possivel gravação em andamento).
                // Se a ultima modificação for muito recente, consideramos o arquivo em uso e pulamos.
                try {
                    auto ftime = fs::last_write_time(p);
                    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
                    );
                    auto age = std::chrono::system_clock::now() - sctp;
                    if (age < std::chrono::seconds(10)) {
                        std::cout << "Arquivo parece estar em uso (modificado recentemente), pulando: " << p << std::endl;
                        // remover lock e pular
                        try { fs::remove_all(lock_dir); } catch (...) {}
                        continue;
                    }
                } catch (...) {
                    // em caso de erro ao checar tempo, prosseguir com o processamento
                }

                // Usar sempre o caminho de modelo padrão conforme solicitado
                std::string model_path = DEFAULT_MODEL_PATH;
                bool ok = transcrever_arquivo(p, model_path, lang);

                // remover lock independentemente do resultado
                try { fs::remove_all(lock_dir); } catch (...) {}

                // marcar como processado para nao tentar novamente
                arquivos_processados.insert(p.string());
            }
            continue; // checar novamente
        } else {
            // avancar um dia
            std::tm next_tm = current_tm;
            // converter to time_t, add 24h, back to tm
            std::time_t t = std::mktime(&next_tm);
            t += 24 * 3600;
            next_tm = *std::localtime(&t);
            current_tm = next_tm;
            fs::path next_pasta = fs::path(PASTA_ROOT) / format_date_ddMMyyyy(current_tm);
            std::cout << "Avancando para a proxima pasta (aguardando se necessario): " << next_pasta << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
    }

    return 0;
}
