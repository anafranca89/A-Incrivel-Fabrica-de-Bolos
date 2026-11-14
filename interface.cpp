#include <iostream>
#include <vector>
#include <thread>
using namespace std;

int vida; // se chega a 0 interrompe o jogo na hora (tem que verificar cada vez q perde se é =0)
int pontuacao;
int faseAtual;
int full[N]; // um estoque pra cada tipo (n maximo q vamos decidir)
int empty;


_ _ _ _ _ _ _ _ _ 

b chocolate r chocolate - - - - 
b morango r morango - - - -
b cenoura r chocolare -

condicao pra produzir X: empty > 0
down empty
up full[X]

condicao pra comprar X: full[X] > 0
down full[X]
up empty


void fase(){
    int nroMaxEstoque; // fixo
    int duracaoFase; //fixo

    // parametros q determinam a dificuldade da fase
    int paciencia;
    int nroMaxConsumidores;
    int nroMaxMassas;
    int nroMaxRecheios;
    int nroMaxCoberturas;

    

    gerarDificuldade(faseAtual, &paciencia, &nroMaxConsumidores
                    &nroMaxMassas, &nroMaxRecheios, &nroMaxCoberturas);
        // a depender do numero de faseAtual, essa funcao precisará gerar valores
        // (ou atribuir pre determinado pos nós) a todos esses parâmetros de dificuldade


    gerarMenu(nroMaxMassas, nroMaxRecheios, nroMaxCoberturas); // fernanda
        // essa função precisará gerar o menu atual da fase
        // escolher massas disponiveis
        // escolher recheios
        // escolher cobertura
        // poe tudo em um vetor de strings e embaralha. mantem vetor original
        // pra passar pro consumidor pra ele decidir (porque sabemos os intervalos)

        // bolo de chocolate
        // bolo de coco
        // bolo de morango
        // bolo de laranja
        // bolo de cenoura
        // ...

        // recheio de brigadeiro
        // recheio de nutella
        // recheio de coco 
        // recheio de ninho
        // ...

        // cobertura de confete
        // cobertura de pasta americana
        // cobertura de ganache
        // ...

        // numeros unicos atribuidos aleatoriamente por rodada 
        // player digita de 1 a 3 numeros. massa sempre é obrigatório
    

    // menu
    // receita

    // criar threads de produtor e consumidor

    // fechar todas a threads


}

void interface(){
    
    // reinicializar estoque;
    
    while(true){
        fase();
    }
}