#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>

#define SHM_NAME "/myshm"
#define SEMR_NAME "/mysemread"
#define SEMW_NAME "/mysemwrite"

sem_t *mutex_read, *mutex_write;

typedef struct
{
    int M;
    int N;
    int K;
    int id;
} Book;
Book *ptr;
Book books[10000];

void sigfunc(int sig) {
    sem_close(mutex_read);
    sem_unlink(SEMR_NAME);

    sem_close(mutex_write);
    sem_unlink(SEMW_NAME);

    // отсоединяем разделяемую память
    munmap(ptr, sizeof(Book));
    shm_unlink(SHM_NAME);
    exit(0);
}

int main(int argc, char** argv) {
    int fd, *counter;
    pid_t pid;

    if (argc != 4) {
        printf("Некорректное число аргументов (требуются M, N, K)!\n");
        exit(-1);
    }

    // создаем разделяемую память и отображаем ее в адресное пространство процессов
    fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    ftruncate(fd, sizeof(Book));
    ptr = (Book*) mmap(NULL, sizeof(Book), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    int M = atoi(argv[1]);
    int N = atoi(argv[2]);
    int K = atoi(argv[3]);

    int nbook = N * M * K;

    mutex_read = sem_open(SEMR_NAME, O_CREAT | O_RDWR, 0666, 0);
    if (mutex_read == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    mutex_write = sem_open(SEMW_NAME, O_CREAT | O_RDWR, 0666, 1);
    if (mutex_write == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    for (int j = 0; j < M; ++j) {
        // создаем дочерний процесс
        pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // дочерний процесс - студент
            signal(SIGINT, sigfunc);
            for (int i = 0; i < N; i++) {
                for (int k = 0; k < K; ++k) {
                    sem_wait(mutex_write); // захватываем семафор
                    Book book;
                    book.M = j;
                    book.N = i;
                    book.K = k;
                    srand(time(NULL));
                    book.id = rand() % 10000;
                    *ptr = book;
                    printf("Студент с %d ряда присвоил книге с места %d шкафа %d номер %d\n", j + 1, i + 1, k + 1, book.id + 1);
                    sleep(1);
                    sem_post(mutex_read);
                    // освобождаем семафор
                }
            }
            exit(EXIT_SUCCESS);
        }
    }

    signal(SIGINT, sigfunc);

    // родительский процесс
    for (int i = 0; i < nbook; i++) {
        sem_wait(mutex_read);
        Book book = *ptr;
        books[(book.M * N + book.N) * K + book.K].M = book.M;
        books[(book.M * N + book.N) * K + book.K].N = book.N;
        books[(book.M * N + book.N) * K + book.K].K = book.K;
        books[(book.M * N + book.N) * K + book.K].id = book.id;
        sem_post(mutex_write);
    }

    for (int j = 0; j < M; ++j) {
        wait(NULL);
    }

    printf("\nВсе подкаталоги получены, начинается сортировка каталога!\n");
    sleep(1);

    Book tmp;
    int noSwap;

    for (int i = nbook - 1; i >= 0; i--) {
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
    for (int i = 0; i < nbook; ++i) {
        printf("%d\t%d\t%d\t%d\n", books[i].id + 1, books[i].M + 1, books[i].N + 1, books[i].K + 1);
    }

    // закрываем и удаляем семафор
    sem_close(mutex_read);
    sem_unlink(SEMR_NAME);

    sem_close(mutex_write);
    sem_unlink(SEMW_NAME);

    // отсоединяем разделяемую память
    munmap(ptr, sizeof(Book));
    shm_unlink(SHM_NAME);

    return 0;
}