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

pthread_t hiloA, hiloB;
pthread_mutex_t mutex_a, mutex_b; // Declaración de mutex globales
bool sigterm = false;             // Control de terminación de hilos
bool saltear = false;
bool lectA = true;
bool lectB = true;
bool sigstop, sigcont;

// Funciones de cifrado.
int cifrar(int x) { return (4 * x + 5) % 27; }
int descifrar(int x) { return (7 * x + 19) % 27; }

// Manejador de señal solo para finalizar.
void handle_sigterm(int signum) {
    sigterm = true;
    printf("\n\033[0;32mHilo A\033[0m detectó SIGTERM. Notificando a hilo B y finalizando luego de completar tareas...\n\n");
}
void handle_sigcont(int signum) { sigcont = true; }
void handle_sigstop(int signum) { sigstop = true; }

void *dinamicaA(void *arg) {
    signal(SIGTERM, handle_sigterm);

    while (!sigterm) {
        if (lectB) {
            // Escribir en primeros 10 elementos

            while (pthread_mutex_trylock(&mutex_a) == EBUSY) {
                if (sigterm) {
                    exit(EAGAIN);
                }
                pthread_mutex_trylock(&mutex_a);
            }
            printf("\n\033[0;32mHilo A\033[0m comenzó a escribir los primeros 10 elementos\n\n");
            sleep(2);
            for (int i = 0; i < SIZE / 2; i++) {
                int rnum = rand() % 27;
                shm.datos[i] = cifrar(rnum);
                printf("\033[0;32mHilo A\033[0m escribió \033[0;36m%d\033[0m (C: %d), en shm.\033[0;32mdatos[\033[0;32m%d\033[0m\033[0;32m]\033\n",
                       rnum, shm.datos[i], i);
                sleep(1);
            }
            pthread_mutex_unlock(&mutex_a);
            lectB = false;
        } else {
            printf("\n El \033[0;31mHilo B\033[0m no consumió los datos previamente escritos.\n");
        }
        if (sigterm)
            break;

        // Leer últimos 10 elementos

        while (pthread_mutex_trylock(&mutex_b) == EBUSY) {
            if (sigterm) {
                exit(EAGAIN);
            }
        }
        printf("\n\033[0;32mHilo A\033[0m comenzó a leer últimos 10 elementos\n\n");
        sleep(2);
        for (int i = SIZE / 2; i < SIZE; i++) {
            int rnum_d = descifrar(shm.datos[i]);
            printf("\033[0;32mHilo A\033[0m leyó %d (D: \033[0;36m%d\033[0m) en shm.\033[0;32mdatos[\033[0;32m%d\033[0m\033[0;32m]\033[0m\n",
                   shm.datos[i], rnum_d, i);
        }
        lectA = true;
        pthread_mutex_unlock(&mutex_b);
    }
    pthread_exit(NULL);
}

void *dinamicaB(void *arg) {
    signal(SIGUSR1, handle_sigstop);
    signal(SIGUSR2, handle_sigcont);

    while (!sigterm) {
        if (lectA) {
            // Escribir en últimos 10 elementos
            pthread_mutex_lock(&mutex_b);
            printf("\n\033[0;31mHilo B\033[0m comenzó a escribir los últimos 10 elementos\n\n");
            sleep(2);
            for (int i = SIZE / 2; i < SIZE; i++) {
                int rnum = rand() % 27;
                shm.datos[i] = cifrar(rnum);
                printf("\033[0;31mHilo B\033[0m escribió \033[0;36m%d\033[0m (C: %d), en shm.\033[0;31mdatos[\033[0;31m%d\033[0m\033[0;31m]\033[0m\n",
                       rnum, shm.datos[i], i);
                sleep(1);
                while (sigstop) {
                    if (sigcont) {
                        sigstop = false;
                        sigcont = false;
                        break;
                    }
                }
            }
            lectA = false;
            pthread_mutex_unlock(&mutex_b);
        } else {
            printf("\n\033[0;32mHilo A\033[0m no consumió mis datos previos. No escribiré nuevos hasta que lo haga.\n");
        }

        // Chequeo intermedio por si se recibe sigterm en el medio de la tarea.
        if (sigterm)
            break;
        while (sigstop) {
            if (sigcont) {
                sigstop = false;
                sigcont = false;
                break;
            }
        }

        // Leer primeros 10 elementos
        pthread_mutex_lock(&mutex_a);
        printf("\033[0;31mHilo B\033[0m comenzó a leer primeros 10 elementos\n");
        sleep(2);
        for (int i = 0; i < SIZE / 2; i++) {
            int rnum_d = descifrar(shm.datos[i]);
            printf("\033[0;31mHilo B\033[0m leyó %d (D: \033[0;36m%d\033[0m), en shm.\033[0;31mdatos[\033[0;31m%d\033[0m\033[0;31m]\033[0m\n",
                   shm.datos[i], rnum_d, i);
            while (sigstop) {
                if (sigcont) {
                    sigstop = false;
                    sigcont = false;
                    break;
                }
            }
        }
        lectB = true;
        pthread_mutex_unlock(&mutex_a);
    }
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {

    // Semilla para números aleatorios, que seran hasta el 26 (letras).
    srand(time(NULL));

    // Identificacion.
    printf("\nSoy el \033[0;30mproceso\033[0m y mi PID es \033[0;36m%d\033[0m\n\n", getpid());

    // Inicializar mutex
    pthread_mutex_init(&mutex_a, NULL);
    pthread_mutex_init(&mutex_b, NULL);

    // Crear hilos
    pthread_create(&hiloA, NULL, dinamicaA, NULL);
    pthread_create(&hiloB, NULL, dinamicaB, NULL);

    // Espera a que los hilos terminen su ejecucion.
    pthread_join(hiloA, NULL);
    pthread_join(hiloB, NULL);

    // Liberar recursos
    pthread_mutex_destroy(&mutex_a);
    if (errno == EBUSY)
        perror("mutex_a");
    pthread_mutex_destroy(&mutex_b);
    if (errno == EBUSY)
        perror("mutex_b");

    printf("\n Hilos destruidos.\n");

    printf("\nFinalizando programa...\n");
    return 0;
}
