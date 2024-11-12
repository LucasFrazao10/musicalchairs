#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <random>

constexpr int NUM_JOGADORES = 4;

std::condition_variable music_cv;
std::mutex music_mutex;
std::atomic<bool> musica_parada{false};
std::mutex cadeira_mutex;
int cadeiras_disponiveis;

class JogoDasCadeiras {
public:
    JogoDasCadeiras(int num_jogadores)
        : num_jogadores(num_jogadores), cadeiras(num_jogadores - 1) {}

    void iniciar_rodada() {
        cadeiras_disponiveis = cadeiras;
        std::cout << "Iniciando rodada com " << cadeiras_disponiveis << " cadeiras e "
                  << num_jogadores << " jogadores restantes.\n";
    }

    void parar_musica() {
        std::lock_guard<std::mutex> lock(music_mutex);
        musica_parada = true;
        music_cv.notify_all();
    }

    void eliminar_jogador(int jogador_id) {
        std::cout << "Jogador " << jogador_id << " foi eliminado.\n";
        num_jogadores--;
    }

    bool ainda_tem_jogo() const {
        return num_jogadores > 1;
    }

    void remover_cadeira() {
        cadeiras--;
    }

    int get_num_jogadores() const {
        return num_jogadores;
    }

private:
    int num_jogadores;
    int cadeiras;
};

class Jogador {
public:
    Jogador(int id, JogoDasCadeiras& jogo)
        : id(id), jogo(jogo), eliminado(false), sentou(false) {}

    void tentar_ocupar_cadeira() {
        std::unique_lock<std::mutex> lock(cadeira_mutex);
        if (cadeiras_disponiveis > 0 && !sentou) {
            cadeiras_disponiveis--;
            sentou = true;
            std::cout << "Jogador " << id << " conseguiu uma cadeira.\n";
        }
    }

    void joga() {
        while (jogo.ainda_tem_jogo() && !eliminado) {
            std::unique_lock<std::mutex> lock(music_mutex);
            music_cv.wait(lock, [] { return musica_parada.load(); });

            tentar_ocupar_cadeira();
            if (!sentou && !eliminado) {
                eliminado = true;
                jogo.eliminar_jogador(id);
            }

            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }

    bool foi_eliminado() const { return eliminado; }
    void reset_sentou() { sentou = false; }
    int get_id() const { return id; }

private:
    int id;
    JogoDasCadeiras& jogo;
    bool eliminado;
    bool sentou;
};

class Coordenador {
public:
    Coordenador(JogoDasCadeiras& jogo)
        : jogo(jogo) {}

    void iniciar_jogo() {
        while (jogo.ainda_tem_jogo()) {
            jogo.iniciar_rodada();
            musica_parada = false;

            std::this_thread::sleep_for(std::chrono::milliseconds(500 + rand() % 1000));
            jogo.parar_musica();

            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            for (auto& jogador : jogadores_objs) {
                jogador.get().reset_sentou();
            }

            jogo.remover_cadeira();
        }

        for (auto& jogador : jogadores_objs) {
            if (!jogador.get().foi_eliminado()) {
                std::cout << "O vencedor é o jogador " << jogador.get().get_id() << "!\n";
                break;
            }
        }
    }

    void adicionar_jogador(Jogador& jogador) {
        jogadores_objs.push_back(std::ref(jogador));
    }

private:
    JogoDasCadeiras& jogo;
    std::vector<std::reference_wrapper<Jogador>> jogadores_objs;
};

int main() {
    srand(static_cast<unsigned>(time(nullptr)));
    JogoDasCadeiras jogo(NUM_JOGADORES);
    Coordenador coordenador(jogo);
    std::vector<std::thread> threads_jogadores;

    std::vector<Jogador> jogadores;
    for (int i = 1; i <= NUM_JOGADORES; ++i) {
        jogadores.emplace_back(i, jogo);
    }

    for (auto& jogador : jogadores) {
        coordenador.adicionar_jogador(jogador);
        threads_jogadores.emplace_back(&Jogador::joga, &jogador);
    }

    std::thread coordenador_thread(&Coordenador::iniciar_jogo, &coordenador);

    for (auto& t : threads_jogadores) {
        if (t.joinable()) {
            t.join();
        }
    }

    if (coordenador_thread.joinable()) {
        coordenador_thread.join();
    }

    std::cout << "Jogo das Cadeiras finalizado.\n";
    return 0;
}














