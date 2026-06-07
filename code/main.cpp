#include <bits/stdc++.h>
#include <pthread.h> 

using namespace std;

int qnt_linhas;
int qnt_colunas;
int quadrantes_por_linha;
int total_threads; 

struct cor{
    int r, g, b;
};

vector<vector<cor>> imagem; // utilizar valores de 0 - 255, para representar cores

void init_variables(char* argv[]){
    cout << "INIT_VARIABLES\n";
    cout << "Tentando inicializar variaveis no metodo (init_variables)..." << "\n";
    try{
        qnt_linhas = stoi(argv[1]); // representa quantos pixels tem a "altura" da imagem
        qnt_colunas = stoi(argv[2]); // representa quantos pixels tem o "comprimento" da imagem
        quadrantes_por_linha = stoi(argv[3]);
        
        total_threads = quadrantes_por_linha * quadrantes_por_linha; 
        
        // inicializar a imagem com todas as cores [0, 0, 0]
        imagem.resize(qnt_linhas);
    
        for(auto & linha : imagem){ 
            linha.resize(qnt_colunas);
            for(auto & celula : linha){
                celula.r = 0;
                celula.g = 0;
                celula.b = 0;
            }
        }
        cout << "Finalizando a inicializacao com sucesso!" << "\n\n";
    }
    catch (const exception & e){
        cerr << "Erro ao tentar inicializar as variaveis no metodo (init_variables)!" << "\n";
        cerr << "Informacoes adicionais sobre o erro: " << e.what() << "\n";
        exit(1); // encerra o programa inteiro
    }
}

void salvar_imagem_ppm() {
    ofstream arquivo("resultado.ppm");
    
    arquivo << "P3\n" << qnt_colunas << " " << qnt_linhas << "\n255\n";    
    
    for(const auto & linha : imagem) {
        for(const auto & pixel : linha) {
            arquivo << '[' << pixel.r << " " << pixel.g << " " << pixel.b << "]  ";
        }
        arquivo << "\n";
    }
    
    arquivo.close();
    cout << "Arquivo 'resultado.ppm' gerado com sucesso na pasta do projeto!" << endl;
}

/*
DECISOES A FAZER:
Metodo de pintura:

1 - para pintar a imagem vamos usar um metodo que randomize a cor entre 0 - 255
2 - para pintar a imagem vamos utilizar o ID da respectiva thread que esteja pintando o pixel / celula
3 - fazer de ambas formas (2 resultados)
*/

int main(int argc, char* argv[]) {
    if (argc < 4) {
        cout << "Erro! Uso correto: " << argv[0] << " [qnt_linhas, qnt_colunas, numero_de_quadrantes_por_linha]" << endl;
        return 1; 
    }
    init_variables(argv);
    salvar_imagem_ppm();
    cout << "O programa foi configurado para uma grade de " << quadrantes_por_linha << "x" << quadrantes_por_linha << " quadrantes." << endl;
    cout << "Total de tarefas a serem geradas: " << total_threads << endl;
    cout << "Matriz inicializada com tamanho: " << qnt_linhas << "x" << qnt_colunas << endl;

    return 0;
}