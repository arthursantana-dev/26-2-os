#pragma once

#include <atomic>
#include <chrono>
#include <semaphore>
#include <string>
#include <utility>

#include "config.hpp"

// Semáforo com os nomes da aula (down/up) por cima de std::counting_semaphore.
//
// std::counting_semaphore não deixa ler o valor atual, então guardamos um
// contador ao lado só para a tela. Ele pode ficar defasado por um instante,
// mas nunca é usado para decidir nada: quem decide é o semáforo de verdade.
class Semaforo {
public:
    Semaforo(std::string nome, int inicial)
        : nome_(std::move(nome)), sem_(inicial), valor_(inicial) {}

    Semaforo(const Semaforo&) = delete;
    Semaforo& operator=(const Semaforo&) = delete;

    // down() clássico: bloqueia até conseguir.
    void down() {
        ++bloqueados_;
        sem_.acquire();
        --bloqueados_;
        --valor_;
    }

    // down() que desiste se `cancelar` virar true (fim do dia ou jogador saiu).
    // Usa try_acquire_for para o jogo nunca congelar esperando um up que não vem.
    bool down(const std::atomic<bool>& cancelar) {
        ++bloqueados_;
        while (!sem_.try_acquire_for(cfg::PASSO)) {
            if (cancelar.load()) {
                --bloqueados_;
                return false;
            }
        }
        --bloqueados_;
        --valor_;
        return true;
    }

    // Tenta sem bloquear.
    bool try_down() {
        if (!sem_.try_acquire()) return false;
        --valor_;
        return true;
    }

    void up() {
        ++valor_;
        sem_.release();
    }

    int valor() const { return valor_.load(); }
    int bloqueados() const { return bloqueados_.load(); }
    const std::string& nome() const { return nome_; }

private:
    std::string nome_;
    std::counting_semaphore<> sem_;
    std::atomic<int> valor_;
    std::atomic<int> bloqueados_{0};
};

// Região crítica com RAII: down(mutex) no construtor, up(mutex) no destrutor.
class SecaoCritica {
public:
    explicit SecaoCritica(Semaforo& mutex) : mutex_(mutex) { mutex_.down(); }
    ~SecaoCritica() { mutex_.up(); }
    SecaoCritica(const SecaoCritica&) = delete;
    SecaoCritica& operator=(const SecaoCritica&) = delete;

private:
    Semaforo& mutex_;
};
