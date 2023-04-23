#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdbool.h>

#define SHM_NAME "/myshm"
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
    int fd;
    sem_t *semafor_write, *semafor_read;

    shm_unlink(SHM_NAME);

    // создаем разделяемую память и отображаем ее в адресное пространство процессов
    fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(fd, sizeof(struct shared)) == -1) {
        perror("ftruncate");
        return 1;
    }

    struct shared *data = mmap(NULL, sizeof(struct shared), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

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

    data->curr_M = 0;
    sleep(1);
    // родительский процесс
    while (data->curr_M < 10) {
        sem_wait(semafor_read); // захватываем семафор
        for (int i = 0; i < 2; ++i) {
            for (int j = 0; j < 3; ++j) {
                books[(data->curr_M * 2 + i) * 3 + j] = data->table[i * 3 + j];
            }
        }
        printf("Библиотека получила подкаталог студента с %d ряда!\n", data->curr_M + 1);
        fflush(stdout);
        data->curr_M++;
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

    // отсоединяем разделяемую память
    munmap(data, sizeof(struct shared));
    shm_unlink(SHM_NAME);

    return 0;
}