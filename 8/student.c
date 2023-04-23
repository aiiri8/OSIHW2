#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <stdbool.h>
#include <time.h>
#include <signal.h>

#define SHM_KEY 1235
#define SEMR_KEY 5431
#define SEMW_KEY 2468

typedef struct {
    int M;
    int N;
    int K;
    int id;
} Book;

struct shared {
    Book table[6];
    int curr_M;
};

int shmid;
struct shared *data;
int sem_read, sem_write;
struct sembuf sem_buf;

int main(int argc, char **argv) {

    // создаем разделяемую память и отображаем ее в адресное пространство процессов
    shmid = shmget(SHM_KEY, sizeof(struct shared), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    data = (struct shared *) shmat(shmid, NULL, 0);
    if (data == (struct shared *) -1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }


    sem_read = semget(SEMR_KEY, 1, IPC_CREAT | 0666);
    if (sem_read == -1) {
        perror("semget() failed");
        exit(EXIT_FAILURE);
    }
    semctl(sem_read, 0, SETVAL, 0);


    sem_write = semget(SEMW_KEY, 1, IPC_CREAT | 0666);
    if (sem_write == -1) {
        perror("semget() failed");
        exit(EXIT_FAILURE);
    }
    semctl(sem_write, 0, SETVAL, 1);

    for (;;) {
        sleep(1);
        sem_buf.sem_num = 0;
        sem_buf.sem_op = -1;
        sem_buf.sem_flg = 0;
        semop(sem_write, &sem_buf, 1);
        if (data->curr_M >= 10) {
            sem_buf.sem_op = 1;
            semop(sem_write, &sem_buf, 1);

            // закрываем и удаляем семафор
            semctl(sem_read, 0, IPC_RMID, 0);
            semctl(sem_write, 0, IPC_RMID, 0);

            // отсоединяем разделяемую память
            shmdt(data);
            shmctl(shmid, IPC_RMID, NULL);
            exit(EXIT_SUCCESS);
        }
        sleep(1);
        srand(time(NULL));
        for (int i = 0; i < 2; i++) {
            for (int k = 0; k < 3; ++k) {
                Book book;
                book.M = data->curr_M;
                book.N = i;
                book.K = k;
                book.id = rand() % 10000;
                data->table[i * 3 + k] = book;
                printf("Студент с %d ряда присвоил книге с места %d шкафа %d номер %d\n", data->curr_M + 1, i + 1, k + 1, book.id + 1);
            }
        }
        printf("Студент с %d ряда закончил составлять подкаталог!\n", data->curr_M + 1);
        fflush(stdout);
        sem_buf.sem_op = 1;
        semop(sem_read, &sem_buf, 1);
    }
}