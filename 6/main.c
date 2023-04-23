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
#include <sys/sem.h>
#include <sys/shm.h>

#define SHM_KEY 1001
#define SEMR_KEY 1234
#define SEMW_KEY 4321

int sem_read, sem_write, shmid;
struct sembuf sem_buf;

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
    semctl(sem_read, 0, IPC_RMID, 0);
    semctl(sem_write, 0, IPC_RMID, 0);

    shmdt(ptr);
    shmctl(shmid, IPC_RMID, NULL);
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
    shmid = shmget(SHM_KEY, sizeof(Book), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    // получаем указатель на разделяемую память
    ptr = (Book*) shmat(shmid, NULL, 0);
    if (ptr == (Book*) -1) {
        perror("shmat");
        exit(1);
    }

    int M = atoi(argv[1]);
    int N = atoi(argv[2]);
    int K = atoi(argv[3]);

    int nbook = N * M * K;

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
                    sem_buf.sem_num = 0;
                    sem_buf.sem_op = -1;
                    sem_buf.sem_flg = 0;
                    semop(sem_write, &sem_buf, 1);
                    Book book;
                    book.M = j;
                    book.N = i;
                    book.K = k;
                    srand(time(NULL));
                    book.id = rand() % 10000;
                    *ptr = book;
                    printf("Студент с %d ряда присвоил книге с места %d шкафа %d номер %d\n", j + 1, i + 1, k + 1, book.id + 1);
                    sleep(1);
                    sem_buf.sem_op = 1;
                    semop(sem_read, &sem_buf, 1);
                    // освобождаем семафор
                }
            }
            exit(EXIT_SUCCESS);
        }
    }

    signal(SIGINT, sigfunc);

    // родительский процесс
    for (int i = 0; i < nbook; i++) {
        sem_buf.sem_num = 0;
        sem_buf.sem_op = -1;
        sem_buf.sem_flg = 0;
        semop(sem_read, &sem_buf, 1);
        Book book = *ptr;
        books[(book.M * N + book.N) * K + book.K].M = book.M;
        books[(book.M * N + book.N) * K + book.K].N = book.N;
        books[(book.M * N + book.N) * K + book.K].K = book.K;
        books[(book.M * N + book.N) * K + book.K].id = book.id;
        sem_buf.sem_op = 1;
        semop(sem_write, &sem_buf, 1);
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
    semctl(sem_read, 0, IPC_RMID, 0);
    semctl(sem_write, 0, IPC_RMID, 0);

    // отсоединяем разделяемую память
    shmdt(ptr);
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}