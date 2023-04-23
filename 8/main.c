#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>

#define SHM_KEY 1235
#define SEMR_KEY 5431
#define SEMW_KEY 2468

typedef struct
{
    int M;
    int N;
    int K;
    int id;
} Book;

Book books[60];

struct shared {
    Book table[6];
    int curr_M;
};

int shmid;
struct shared *data;
int sem_read, sem_write;
struct sembuf sem_buf;

int main(void) {

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

    data->curr_M = 0;
    sleep(1);
    // родительский процесс
    while (data->curr_M < 10) {
        sem_buf.sem_num = 0;
        sem_buf.sem_op = -1;
        sem_buf.sem_flg = 0;
        semop(sem_read, &sem_buf, 1);
        for (int i = 0; i < 2; ++i) {
            for (int j = 0; j < 3; ++j) {
                books[(data->curr_M * 2 + i) * 3 + j] = data->table[i * 3 + j];
            }
        }
        printf("Библиотека получила подкаталог студента с %d ряда!\n", data->curr_M + 1);
        fflush(stdout);
        data->curr_M++;
        sem_buf.sem_op = 1;
        semop(sem_write, &sem_buf, 1);
    }

    printf("\nВсе подкаталоги получены, начинается сортировка каталога!\n");
    sleep(1);

    Book tmp;
    int noSwap;

    for (int i = 60 - 1; i >= 0; i--) {
        noSwap = 1;
        for (int j = 0; j < i; j++) {
            if (books[j].id > books[j + 1].id) {
                tmp = books[j];
                books[j] = books[j + 1];
                books[j + 1] = tmp;
                noSwap = 0;
            }
        }
        if (noSwap == 1) {
            break;
        }
    }

    printf("\nСортировка завершена, результаты:\n");

    printf("ID\tM\tN\tK\n");
    for (int i = 0; i < 60; ++i) {
        printf("%d\t%d\t%d\t%d\n", books[i].id + 1, books[i].M + 1, books[i].N + 1, books[i].K + 1);
    }

    // закрываем и удаляем семафор
    semctl(sem_read, 0, IPC_RMID, 0);
    semctl(sem_write, 0, IPC_RMID, 0);

    // отсоединяем разделяемую память
    shmdt(data);
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}