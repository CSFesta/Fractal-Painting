#include <bits/stdc++.h>

#include <pthread.h> 

using namespace std;

void* minhaThread(void* arg) {
    cout << "Ola de dentro da nova thread!" << endl;
    
    pthread_exit(NULL); 
    return NULL;
}

int main() {
    pthread_t thread_id;

    cout << "Main: Criando a thread..." << endl;

    if (pthread_create(&thread_id, NULL, minhaThread, NULL) != 0) {
        cerr << "Erro ao criar a thread!" << endl;
        return 1;
    }

    pthread_join(thread_id, NULL);

    cout << "Main: Thread finalizada com sucesso. Fechando o programa." << endl;

    return 0;
}