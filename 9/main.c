#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdbool.h>
#include <mqueue.h>
#include <string.h>

#define QUEUE_NAME "/my_queue"
#define SEM_WRITE_NAME "semwrite"
#define SEM_READ_NAME "semread"

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

int main(void) {
    mqd_t mq;
    char buffer[sizeof(struct shared)];
    struct mq_attr attr;

    // Определение атрибутов очереди сообщений
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(struct shared);
    attr.mq_curmsgs = 0;

    mq_unlink(QUEUE_NAME);

    // Создание очереди сообщений
    mq = mq_open(QUEUE_NAME, O_CREAT | O_RDWR, 0666, &attr);

    struct shared data;
    data.curr_M = 0;

    mq_send(mq, (char*)&data, sizeof(struct shared), 0);

    sem_t *semafor_write, *semafor_read;

    sem_unlink(SEM_WRITE_NAME);
    sem_unlink(SEM_READ_NAME);

    // создаем POSIX-семафор
    semafor_write = sem_open(SEM_WRITE_NAME, O_CREAT | O_RDWR, 0666, 1);
    if (semafor_write == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    semafor_read = sem_open(SEM_READ_NAME, O_CREAT | O_RDWR, 0666, 0);
    if (semafor_read == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    sleep(1);
    // родительский процесс
    while (data.curr_M < 10) {
        sem_wait(semafor_read); // захватываем семафор

        mq_receive(mq, buffer, sizeof(struct shared), NULL);
        memcpy(&data, buffer, sizeof(struct shared));

        for (int i = 0; i < 2; ++i) {
            for (int j = 0; j < 3; ++j) {
                books[(data.curr_M * 2 + i) * 3 + j] = data.table[i * 3 + j];
            }
        }
        printf("Библиотека получила подкаталог студента с %d ряда!\n", data.curr_M + 1);
        fflush(stdout);

        mq_send(mq, (char*)&data, sizeof(struct shared), 0);
        sem_post(semafor_write); // освобождаем семафор
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
    sem_close(semafor_write);
    sem_close(semafor_read);

    sem_unlink(SEM_WRITE_NAME);
    sem_unlink(SEM_READ_NAME);


    mq_close(mq);
    mq_unlink(QUEUE_NAME);

    return 0;
}