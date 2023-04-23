//
// Created by makeitokay on 22.04.2023.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/random.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>

int getRandomInt(int exclusive_max) {
    int rnd;
    getrandom(&rnd, sizeof(int), GRND_NONBLOCK);
    return abs(rnd) % exclusive_max;
}

union semun {
    int              val;    /* Value for SETVAL */
    struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
    unsigned short  *array;  /* Array for GETALL, SETALL */
    struct seminfo  *__buf;  /* Buffer for IPC_INFO (Linux-specific) */
};


int main(int argc, char* argv[]) {
    if (argc != 10) {
        printf("Некорректное число аргументов");
        return 0;
    }

    union semun arg;

    int sum = atoi(argv[1]);
    float parts[8];
    for (int i = 0; i < 8; ++i) {
        parts[i] = atof(argv[i + 2]);
    }

    key_t shm_key;
    key_t sem_keys[8];
    for (int i = 0; i < 8; i++) {
        sem_keys[i] = ftok(".", i);
    }

    shm_key = ftok(".", 1337);
    int shared_sems[8];
    for (int i = 0; i < 8; i++) {
        shared_sems[i] = semget(sem_keys[i], 1, IPC_CREAT | 0666);
    }

    int shmid = shmget(shm_key, 32, IPC_CREAT | 0666);
    float *lawyer_distribution = shmat(shmid, NULL, 0);
    arg.val = 1;
    for (int i = 0; i < 8; i++) {
        lawyer_distribution[i] = -1;
        semctl(shared_sems[i], 0, SETVAL, arg);
    }

    for (int i = 0; i < 8; ++i) {
        pid_t pid = fork();
        if (pid == 0) {
            struct sembuf sb;
            sb.sem_num = 0;
            sb.sem_op = -1;
            sb.sem_flg = 0;
            semop(shared_sems[i], &sb, 1);
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

                    struct sembuf sb1;
                    sb1.sem_num = 0;
                    sb1.sem_op = -1;
                    sb1.sem_flg = 0;
                    semop(shared_sems[rnd_person], &sb1, 1);

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

                    sb1.sem_op = 1;
                    semop(shared_sems[rnd_person], &sb1, 1);
                }
            } else {
                // адвокат поступает честно с шансом 80%
                printf("[info]: Адвокат поступает честно и распределяет наследство %d наследнику\n",
                       i + 1);
                lawyer_distribution[i] = sum * parts[i];
            }

            sb.sem_op = 1;
            semop(shared_sems[i], &sb, 1);
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

    shmdt(lawyer_distribution);
    shmctl(shmid, IPC_RMID, NULL) == -1;
    for (int i = 0; i < 8; i++) {
        semctl(shared_sems[i], 0, IPC_RMID, arg);
    }

    return 0;
}