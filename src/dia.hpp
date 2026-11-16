#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <deque>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "config.hpp"
#include "estado.hpp"
#include "semaforo.hpp"

class Tela;

struct ResultadoDia {
    Progresso prog;
    int atendidos = 0;
    int foramEmbora = 0;
    int sobreposicoes = 0;
    double segundosDormindo = 0;
    bool abortou = false;  // jogador apertou Q
    bool faliu = false;    // reputação chegou a zero
};

// Um dia de barbearia. Cria todas as threads do jogo, espera o dia acabar e
// faz join em todas antes de devolver o resultado.
//
// Threads:
//   principal  coordena o dia (relógio, fim, join)
//   porta      cria as threads dos clientes, cada vez mais rápido
//   cliente    uma por cliente: senta, espera a vez, é atendido e sai
//   barbeiro   você (corte com as teclas) e os contratados (corte sozinho)
//   entrada    lê o teclado
//   desenho    a única que desenha a tela
//
// Semáforos (os três do barbeiro sonolento dos slides):
//   clientes   = 0  clientes esperando; o barbeiro dorme no down
//   barbeiros  = 0  barbeiros livres; o cliente espera a vez no down
//   mutex      = 1  exclusão mútua das cadeiras e do resto do estado
// e mais dois por barbeiro para combinar o corte com o cliente:
//   sentou[i]      = 0  o cliente avisa que sentou na cadeira do barbeiro i
//   corte_feito[i] = 0  o barbeiro i avisa que o corte terminou
// e um para a entrada passar teclas ao seu barbeiro:
//   teclas     = 0  teclas de corte na fila
class Dia {
public:
    Dia(const Progresso& prog, bool demo, int duracaoSegundos, Tela& tela);
    ~Dia();

    ResultadoDia executar();

private:
    void porta();
    void cliente(int id);
    void barbeiro(int indice);
    void entrada();
    void desenho();

    std::optional<int> cortarComTeclas(int indice);
    std::optional<int> cortarSozinho(int indice);
    void contratar();
    void iniciarBarbeiro(int indice);  // chamar com threadsMtx_ travado

    // Chamar com o mutex já travado.
    int cadeiraLivre() const;
    void sentar(int cadeira, int id);
    void levantar(int cadeira, int id);
    void registrar(std::string linha);

    Quadro capturar();
    bool dormir(std::chrono::milliseconds duracao);

    Tela& tela_;
    const bool demo_;
    const int duracao_;

    Semaforo clientes_{"clientes", 0};
    Semaforo barbeiros_{"barbeiros", 0};
    Semaforo mutex_{"mutex", 1};
    Semaforo teclas_{"teclas", 0};
    std::array<std::unique_ptr<Semaforo>, cfg::MAX_BARBEIROS> sentou_;
    std::array<std::unique_ptr<Semaforo>, cfg::MAX_BARBEIROS> corteFeito_;

    Estado est_;                   // protegido por mutex_
    std::deque<char> filaTeclas_;  // protegido por mutex_

    std::atomic<bool> cancelar_{false};
    std::atomic<bool> pararEntrada_{false};
    std::atomic<bool> pararDesenho_{false};
    std::atomic<bool> abortou_{false};

    // Barbeiros podem ser contratados durante o dia pela thread de entrada.
    std::mutex threadsMtx_;
    std::vector<std::thread> threadsBarbeiros_;

    // Só a thread da porta mexe nesta lista.
    struct ThreadCliente {
        std::atomic<bool> terminou{false};
        std::thread t;
    };
    std::list<ThreadCliente> threadsClientes_;
};
