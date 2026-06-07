## 1. Compilação
**g++ main.cpp -o main -pthread**


## 2. Execução (largura da imagem, altura da imagem, quantidade de quadrantes)
**./main.exe [qnt_linhas] [qnt_colunas] [quadrantes_por_linha]**

### Exemplo Prático:
#### Para gerar uma imagem em resolução Full HD (1920x1080) dividida em uma grade de 5x5 quadrantes (25 tarefas no total), execute:

**./main.exe 1080 1920 5**