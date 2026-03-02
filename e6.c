#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define SIZE 20

typedef struct {
    int datos[SIZE];
} shared_memory;

shared_memory shm; // Memoria compartida directamente sin IPC (hilos en mismo proceso).

pthread_mutex_t mutex_a, mutex_b; // Declaración de mutex globales
bool sigterm = false;             // Control de terminación de hilos
bool saltear = false;

// Funciones de cifrado.
int cifrar(int x) { return (4 * x + 5) % 27; }
int descifrar(int x) { return (7 * x + 19) % 27; }

// Manejador de señal solo para finalizar.
void handle_sigterm(int signum) {
    sigterm = true;
    printf("\n\033[0;32mHilo A\033[0m detectó SIGTERM. Notificando a hilo B y finalizando luego de completar tareas...\n\n");
}

void *dinamicaA(void *arg) {
    signal(SIGTERM, handle_sigterm);

    while (!sigterm) {
        // Escribir en primeros 10 elementos
        pthread_mutex_lock(&mutex_a);
        printf("\n\033[0;32mHilo A\033[0m comenzó a escribir los primeros 10 elementos\n\n");
        sleep(2);
        for (int i = 0; i < SIZE / 2; i++) {
            int rnum = rand() % 27;
            shm.datos[i] = cifrar(rnum);
            printf("\033[0;32mHilo A\033[0m escribió \033[0;36m%d\033[0m (C: %d), en shm.\033[0;32mdatos[%d]\033[0m \n", rnum, shm.datos[i], i);
            sleep(1);
        }
        pthread_mutex_unlock(&mutex_a);

        // Chequeo intermedio por si se recibe sigterm en el medio de la tarea.
        if (sigterm) {
            saltear = true;
        }

        if (!saltear) {
            // Leer últimos 10 elementos
            pthread_mutex_lock(&mutex_b);
            printf("\n\033[0;32mHilo A\033[0m comenzó a leer últimos 10 elementos\n\n");
            sleep(2);
            for (int i = SIZE / 2; i < SIZE; i++) {
                int rnum_d = descifrar(shm.datos[i]);
                printf("\033[0;32mHilo A\033[0m leyó %d (D: \033[0;36m%d\033[0m) en shm.\033[0;32mdatos[%d]\033[0m \n", shm.datos[i], rnum_d, i);
            }
            pthread_mutex_unlock(&mutex_b);
        }
    }
    pthread_exit(NULL);
}

void *dinamicaB(void *arg) {
    while (!sigterm) {
        // Escribir en últimos 10 elementos
        pthread_mutex_lock(&mutex_b);
        printf("\n\033[0;31mHilo B\033[0m comenzó a escribir los últimos 10 elementos\n\n");
        sleep(2);
        for (int i = SIZE / 2; i < SIZE; i++) {
            int rnum = rand() % 27;
            shm.datos[i] = cifrar(rnum);
            printf("\033[0;31mHilo B\033[0m escribió \033[0;36m%d\033[0m (C: %d), en shm.\033[0;31mdatos[%d]\033[0m \n", rnum, shm.datos[i], i);
            sleep(1);
        }
        pthread_mutex_unlock(&mutex_b);

        // Chequeo intermedio por si se recibe sigterm en el medio de la tarea.
        if (sigterm) {
            saltear = true;
        }

        if (!saltear) {
            // Leer primeros 10 elementos
            pthread_mutex_lock(&mutex_a);
            printf("\033[0;31mHilo B\033[0m comenzó a leer primeros 10 elementos\n\n");
            sleep(2);
            for (int i = 0; i < SIZE / 2; i++) {
                int rnum_d = descifrar(shm.datos[i]);
                printf("\033[0;31mHilo B\033[0m leyó %d (D: \033[0;36m%d\033[0m), en shm.\033[0;31mdatos[%d]\033[0m \n", shm.datos[i], rnum_d, i);
            }
            pthread_mutex_unlock(&mutex_a);
        }
    }
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {

    // Semilla para números aleatorios, que seran hasta el 26 (letras).
    srand(time(NULL));

    // Identificacion.
    printf("\nSoy el \033[0;30mproceso\033[0m y mi PID es \033[0;36m%d\033[0m \n\n", getpid());

    pthread_t hiloA, hiloB;

    // Inicializar mutex
    pthread_mutex_init(&mutex_a, NULL);
    pthread_mutex_init(&mutex_b, NULL);

    // Crear hilos
    if (pthread_create(&hiloA, NULL, dinamicaA, NULL) < 0)
        perror("Hilo A");
    if (pthread_create(&hiloB, NULL, dinamicaB, NULL) < 0)
        perror("Hilo B");

    // Espera a que los hilos terminen su ejecucion.
    if (pthread_join(hiloA, NULL) < 0)
        perror("Hilo A");
    if (pthread_join(hiloB, NULL) < 0)
        perror("Hilo B");

    // Liberar recursos
    if (pthread_mutex_destroy(&mutex_a) == EBUSY)
        perror("mutex_a");
    if (pthread_mutex_destroy(&mutex_b) == EBUSY)
        perror("mutex_b");

    printf("\n\033[33mHilos destruidos.\033[0m\n");

    printf("\n\033[33mFinalizando programa...\033[0m\n");
    return 0;
}
