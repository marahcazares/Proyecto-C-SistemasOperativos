/*
Alumna: Hernandez Cazares Rosario Marah
Fecha: 15 de marzo de 2026
Programa: Exploracion de arbol con multihilos
Descripcion: El programa carga un arbol desde un archivo y busca un nodo
especifico utilizando hilos de pthread para explorar los hijos de cada nodo.
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define MAX_NODOS 1005

/* Representar los nodos en memoria */
typedef struct {
    int id;
    int padre;
} Nodo;

/* Variables globales */
Nodo nodos[MAX_NODOS];
int total_nodos = 0;
int nodo_buscado;
int nodos_revisados = 0;
int active_threads = 0;
int encontrado = 0; /* Bandera */

pthread_mutex_t mutex_revisados;
pthread_mutex_t mutex_threads;
pthread_mutex_t mutex_encontrado;

/* Cada instancia revisa unicamente los hijos inmediatos del nodo recibido. */
void *explorar(void *arg) {
    int id_actual = *(int *)arg;
    free(arg);

    for (int i = 0; i < total_nodos; i++) {
        /* Verificar si ya se encontro el nodo */
        pthread_mutex_lock(&mutex_encontrado);
        int ya_encontrado = encontrado;
        pthread_mutex_unlock(&mutex_encontrado);

        if (ya_encontrado) {
            break;
        }

        /* Buscar nodos cuyo padre sea el nodo que este hilo esta explorando */
        if (nodos[i].padre == id_actual && nodos[i].id != id_actual) {

            /* Registrar nodo revisado */
            pthread_mutex_lock(&mutex_revisados);
            nodos_revisados++;
            int current_total = nodos_revisados;
            pthread_mutex_unlock(&mutex_revisados);

            /* Verificar si este hijo es el nodo buscado */
            if (nodos[i].id == nodo_buscado) {
                pthread_mutex_lock(&mutex_encontrado);
                encontrado = 1;
                pthread_mutex_unlock(&mutex_encontrado);

                printf("\n========================================\n");
                printf("Busqueda exitosa! Nodo %d encontrado.\n", nodo_buscado);
                printf("Total de nodos revisados: %d\n", current_total);
                printf("========================================\n");
                break;
            } else {
                /* Crear hilo independiente para explorar los hijos de este nodo. */
                int *nuevo_param = (int *)malloc(sizeof(int));
                if (nuevo_param == NULL) {
                    fprintf(stderr, "Error: no se pudo reservar memoria para el parametro del hilo.\n");
                    continue;
                }
                *nuevo_param = nodos[i].id;

                pthread_mutex_lock(&mutex_threads);
                active_threads++;
                pthread_mutex_unlock(&mutex_threads);

                pthread_t hilo_hijo;
                if (pthread_create(&hilo_hijo, NULL, explorar, nuevo_param) != 0) {
                    /* Si falla la creacion, revertir el incremento y liberar memoria */
                    fprintf(stderr, "Error: no se pudo crear hilo para nodo %d.\n", nodos[i].id);
                    pthread_mutex_lock(&mutex_threads);
                    active_threads--;
                    pthread_mutex_unlock(&mutex_threads);
                    free(nuevo_param);
                } else {
                    pthread_detach(hilo_hijo);
                }
            }
        }
    }

    /* Al terminar de revisar todos sus hijos, el hilo decrementa el contador global */
    pthread_mutex_lock(&mutex_threads);
    active_threads--;
    pthread_mutex_unlock(&mutex_threads);

    return NULL;
}

int main(void) {
    FILE *archivo = fopen("definicion_arbol.txt", "r");
    if (archivo == NULL) {
        perror("Error al abrir el archivo");
        return 1;
    }

    int p;
    int indice = 0;

    /* Carga de datos con validacion de referencias hacia adelante */
    while (fscanf(archivo, "%d", &p) == 1 && indice < MAX_NODOS) {
        if (p > indice) {
            fprintf(stderr, "Error: referencia a nodo no creado (padre %d para nodo %d), se descarta.\n", p, indice);
            indice++;
            continue;
        }
        nodos[indice].id = indice;
        nodos[indice].padre = p;
        indice++;
    }
    total_nodos = indice;
    fclose(archivo);

    printf("Arbol cargado con %d nodos.\n", total_nodos);
    printf("Ingrese el nodo a buscar: ");
    if (scanf("%d", &nodo_buscado) != 1) {
        fprintf(stderr, "Error: entrada invalida.\n");
        return 1;
    }

    /* Inicializar mutexes */
    pthread_mutex_init(&mutex_revisados, NULL);
    pthread_mutex_init(&mutex_threads, NULL);
    pthread_mutex_init(&mutex_encontrado, NULL);

    /* Revisar el nodo raiz (0) antes de lanzar hilos */
    nodos_revisados++;
    if (nodo_buscado == 0) {
        printf("\n========================================\n");
        printf("Busqueda exitosa! Nodo 0 (raiz) encontrado.\n");
        printf("Total de nodos revisados: %d\n", nodos_revisados);
        printf("========================================\n");
        pthread_mutex_destroy(&mutex_revisados);
        pthread_mutex_destroy(&mutex_threads);
        pthread_mutex_destroy(&mutex_encontrado);
        return 0;
    }

    /* Crear el primer hilo para explorar los hijos de la raiz. */
    int *param_inicial = (int *)malloc(sizeof(int));
    if (param_inicial == NULL) {
        fprintf(stderr, "Error: no se pudo reservar memoria inicial.\n");
        pthread_mutex_destroy(&mutex_revisados);
        pthread_mutex_destroy(&mutex_threads);
        pthread_mutex_destroy(&mutex_encontrado);
        return 1;
    }
    *param_inicial = 0;

    pthread_mutex_lock(&mutex_threads);
    active_threads = 1;
    pthread_mutex_unlock(&mutex_threads);

    pthread_t hilo_raiz;
    if (pthread_create(&hilo_raiz, NULL, explorar, param_inicial) != 0) {
        fprintf(stderr, "Error: no se pudo crear el hilo raiz.\n");
        free(param_inicial);
        pthread_mutex_destroy(&mutex_revisados);
        pthread_mutex_destroy(&mutex_threads);
        pthread_mutex_destroy(&mutex_encontrado);
        return 1;
    }
    pthread_detach(hilo_raiz);

    /* Esperar a que todos los hilos terminen */
    while (1) {
        pthread_mutex_lock(&mutex_threads);
        int hilos_activos = active_threads;
        pthread_mutex_unlock(&mutex_threads);

        if (hilos_activos == 0) {
            break;
        }
        usleep(1000);
    }

    /* Verificar si se encontro o no el nodo */
    if (!encontrado) {
        pthread_mutex_lock(&mutex_revisados);
        int total_final = nodos_revisados;
        pthread_mutex_unlock(&mutex_revisados);

        printf("\n========================================\n");
        printf("Busqueda no exitosa. El nodo %d no existe en el arbol.\n", nodo_buscado);
        printf("Total de nodos revisados: %d\n", total_final);
        printf("========================================\n");
    }

    pthread_mutex_destroy(&mutex_revisados);
    pthread_mutex_destroy(&mutex_threads);
    pthread_mutex_destroy(&mutex_encontrado);

    return 0;
}
