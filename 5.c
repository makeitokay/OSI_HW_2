//
// Created by makeitokay on 22.04.2023.
//

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <string.h>
#include <time.h>
#include <sys/random.h>

int getRandomInt(int exclusive_max) {
    int rnd;
    getrandom(&rnd, sizeof(int), GRND_NONBLOCK);
    return abs(rnd) % exclusive_max;
}

int main(int argc, char* argv[]) {
    if (argc != 10) {
        printf("Некорректное число аргументов");
        return 0;
    }

    int sum = atoi(argv[1]);
    float parts[8];
    for (int i = 0; i < 8; ++i) {
        parts[i] = atof(argv[i + 2]);
    }

    int shm = shm_open("shm", O_CREAT | O_RDWR, 0666);
    ftruncate(shm, 32);
    float* lawyer_distribution = mmap(NULL, sizeof(sem_t*) * 8, PROT_READ | PROT_WRITE, MAP_SHARED, shm, 0);

    int sem_shm = shm_open("sem_shm", O_CREAT | O_RDWR, 0666);
    ftruncate(sem_shm, sizeof(sem_t*) * 8);
    sem_t** sem = mmap(NULL, sizeof(sem_t*) * 8, PROT_READ | PROT_WRITE, MAP_SHARED, sem_shm, 0);

    for (int i = 0; i < 8; ++i) {
        lawyer_distribution[i] = -1;
        sem[i] = malloc(sizeof(sem_t*));
        sem_init(sem[i], 1, 1);
    }

    for (int i = 0; i < 8; ++i) {
        pid_t pid = fork();
        if (pid == 0) {
            sem_wait(sem[i]);

            sleep(getRandomInt(2));

            int rnd = getRandomInt(10);
            if (rnd >= 8) {
                // адвокат решает поступить нечестно (с шансом 20%)
                int rnd2 = getRandomInt(2);
                if (rnd2 == 0) {
                    // адвокат часть денег себе
                    lawyer_distribution[i] = sum * parts[i] / 2;
                    printf("[info]: Адвокат забирает часть денег от %d наследника себе\n", i + 1);
                } else {
                    // адвокат отдает часть денег другому наследнику

                    int rnd_person = getRandomInt(8);
                    while (rnd_person == i) {
                        rnd_person = getRandomInt(8);
                    }

                    sem_wait(sem[rnd_person]);

                    // отдаем деньги другому наследнику только если ему уже распределено, если нет -
                    // то поступаем честно
                    if (lawyer_distribution[rnd_person] != -1) {
                        printf(
                            "[info]: Адвокат отсыпает часть денег от %d наследника %d наследнику\n",
                            i + 1, rnd_person + 1);
                        lawyer_distribution[rnd_person] += sum * parts[i] / 2;
                        lawyer_distribution[i] = sum * parts[i] / 2;
                    } else {
                        printf(
                            "[info]: Адвокат поступает честно и распределяет наследство %d "
                            "наследнику\n",
                            i + 1);
                        lawyer_distribution[i] = sum * parts[i];
                    }

                    sem_post(sem[rnd_person]);
                }
            } else {
                // адвокат поступает честно с шансом 80%
                printf("[info]: Адвокат поступает честно и распределяет наследство %d наследнику\n",
                       i + 1);
                lawyer_distribution[i] = sum * parts[i];
            }

            sem_post(sem[i]);

            return 0;
        }
    }

    for (int i = 0; i < 8; ++i) {
        wait(NULL);
    }

    printf("\nНаследники проверяют распределение наследства в соответствии со своими долями!\n");
    for (int i = 0; i < 8; ++i) {
        float expected = sum * parts[i];
        float actual = lawyer_distribution[i];
        if (expected != actual) {
            printf("%d-ый наследник: НЕВЕРНОЕ распределение. Ожидалось %.3f, получил %.3f.\n",
                   i + 1, expected, actual);
        } else {
            printf("%d-ый наследник: ВЕРНОЕ распределение\n", i + 1);
        }
    }

    munmap(lawyer_distribution, 32);
    close(shm);
    shm_unlink("shm");
    for (int i = 0; i < 8; ++i) {
        sem_destroy(sem[i]);
        free(sem[i]);
    }
    munmap(sem, sizeof(sem_t*) * 8);
    close(sem_shm);
    shm_unlink("sem_shm");

    return 0;
}