#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdbool.h>
#include <mqueue.h>
#include <time.h>

#define QUEUE_NAME "/my_queue"

#define SEM_WRITE_NAME "semwrite"
#define SEM_READ_NAME "semread"

typedef struct {
    int M;
    int N;
    int K;
    int id;
} Book;

struct shared {
    int curr_M;
    Book table[6];
};

int main(int argc, char **argv) {
    sem_t *semafor_write, *semafor_read;

    //char *name = argv[1];

    mqd_t mq;
    char buffer[sizeof(struct shared)];
    struct mq_attr attr;

    // Определение атрибутов очереди сообщений
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(struct shared);
    attr.mq_curmsgs = 0;


    // Создание очереди сообщений
    mq = mq_open(QUEUE_NAME, O_CREAT | O_RDWR, 0666, &attr);

    struct shared data;

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


    // Процесс группы
    for (;;) {
        sleep(1);
        sem_wait(semafor_write); // захватываем семафор

        // Получение структуры из очереди
        mq_receive(mq, buffer, sizeof(struct shared), NULL);
        memcpy(&data, buffer, sizeof(struct shared));


        if (data.curr_M >= 10) {
            mq_send(mq, (char*)&data, sizeof(struct shared), 0);

            sem_post(semafor_write);

            sem_close(semafor_write);
            sem_close(semafor_read);

            sem_unlink(SEM_WRITE_NAME);
            sem_unlink(SEM_READ_NAME);

            // Закрытие очереди сообщений
            mq_close(mq);
            mq_unlink(QUEUE_NAME);

            exit(EXIT_SUCCESS);
        }

        sleep(1);
        srand(time(NULL));
        for (int i = 0; i < 2; i++) {
            for (int k = 0; k < 3; ++k) {
                Book book;
                book.M = data.curr_M;
                book.N = i;
                book.K = k;
                book.id = rand() % 10000;
                data.table[i * 3 + k] = book;
                printf("Студент с %d ряда присвоил книге с места %d шкафа %d номер %d\n", data.curr_M + 1, i + 1, k + 1, book.id + 1);
            }
        }
        printf("Студент с %d ряда закончил составлять подкаталог!\n", data.curr_M + 1);
        fflush(stdout);
        data.curr_M += 1;
        mq_send(mq, (char*)&data, sizeof(struct shared), 0);
        sem_post(semafor_read); // освобождаем семафор
    }
}