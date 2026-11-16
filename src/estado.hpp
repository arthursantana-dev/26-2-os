#pragma once

#include <chrono>
#include <deque>
#include <string>
#include <vector>

#include "config.hpp"

using Relogio = std::chrono::steady_clock;

// O que passa de um dia para o outro.
struct Progresso {
    int dia = 1;
    int caixa = 0;
    int reputacao = cfg::REPUTACAO_MAX;
    int cadeiras = cfg::CADEIRAS_INICIAIS;
    int contratados = 0;
};

enum class EstadoBarbeiro { Dormindo, Chamando, Cortando };

struct Cadeira {
    // Normalmente 0 ou 1. No modo demonstração pode passar de 1:
    // é o erro que o mutex evita.
    std::vector<int> clientes;
};

struct Barbeiro {
    bool jogador = false;
    EstadoBarbeiro estado = EstadoBarbeiro::Dormindo;
    int cliente = -1;
    std::string sequencia;  // teclas do corte (só o jogador)
    int acertos = 0;
    double progresso = 0;   // 0..1
    bool errou = false;
};

struct Saida {
    int cliente;
    bool irritado;  // foi embora sem ser atendido
    Relogio::time_point quando;
};

// Estado compartilhado do dia. Toda leitura e escrita acontece dentro do
// semáforo `mutex`. A thread de desenho copia este struct e desenha a cópia.
struct Estado {
    Progresso prog;
    std::vector<Cadeira> cadeiras;
    std::vector<Barbeiro> barbeiros;
    std::vector<Saida> saidas;
    std::deque<std::string> log;

    int atendidos = 0;
    int foramEmbora = 0;
    int sobreposicoes = 0;     // modo demonstração
    int clientesAtivos = 0;    // threads de cliente vivas
    int proximoCliente = 1;
    double segundosDormindo = 0;
    double segundosRestantes = 0;
    bool portaAberta = true;
    bool demo = false;

    void registrar(std::string linha) {
        log.push_back(std::move(linha));
        while (log.size() > 4) log.pop_front();
    }
};

struct ValorSemaforo {
    std::string nome;
    int valor;
    int maximo;      // quantos quadradinhos desenhar
    int bloqueados;  // threads esperando no down agora
};

// Tudo o que a thread de desenho precisa para um quadro.
struct Quadro {
    Estado estado;
    std::vector<ValorSemaforo> semaforos;
    int threadsVivas = 0;
};
