#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdbool.h>
#include <time.h>

#define SHM_NAME "/myshm"
#define SEM_WRITE_NAME "semwrite"
#define SEM_READ_NAME "semread"

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

int main(int argc, char **argv) {
    int fd;
    sem_t *semafor_write, *semafor_read;

    //shm_unlink(SHM_NAME);

    // создаем разделяемую память и отображаем ее в адресное пространство процессов
    fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    struct shared
        *data = mmap(NULL, sizeof(struct shared), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    //sem_unlink(SEM_WRITE_NAME);
    //sem_unlink(SEM_READ_NAME);

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

        if (data->curr_M >= 10) {
            sem_post(semafor_write); // освобождаем семафор

            sem_close(semafor_write);
            sem_close(semafor_read);

            sem_unlink(SEM_WRITE_NAME);
            sem_unlink(SEM_READ_NAME);

            munmap(data, sizeof(struct shared));
            shm_unlink(SHM_NAME);

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
        sem_post(semafor_read); // освобождаем семафор
    }
}