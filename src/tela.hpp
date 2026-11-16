#pragma once

#include <string>

#include "dia.hpp"
#include "estado.hpp"

// Toda a interface em ncurses.
//
// O ncurses não é seguro para várias threads: em cada momento só UMA thread
// chama métodos desta classe. Durante o dia é a thread de desenho; no menu e
// nas telas entre os dias é a principal (quando as outras já levaram join).
class Tela {
public:
    Tela();
    ~Tela();
    Tela(const Tela&) = delete;
    Tela& operator=(const Tela&) = delete;

    int menu(bool demo);  // devolve a tecla escolhida
    void manual();
    void desenharDia(const Quadro& q);
    int fimDoDia(const ResultadoDia& r);  // devolve a tecla escolhida
    void derrota(const ResultadoDia& r);

private:
    int esperarTecla();
};
