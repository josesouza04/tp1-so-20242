#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <ctype.h>

#define MAX_PROCS 20
#define PROC_PATH "/proc"
#define STAT_FILE "stat"

// Estrutura para armazenar informações de cada processo
typedef struct {
    int pid;
    char user[32];
    char procname[64];
    char state;
} ProcessInfo;

void displayProcesses(ProcessInfo procs[], int num_procs);
int getProcesses(ProcessInfo procs[], int max_procs);
void *signalInputThread(void *arg);
int isProcessValid(ProcessInfo procs[], int num_procs, int pid);

int main() {
    ProcessInfo procs[MAX_PROCS];
    pthread_t input_thread;

    // Cria uma thread para ler entrada de sinais
    if (pthread_create(&input_thread, NULL, signalInputThread, procs) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    while (1) {
        int num_procs = getProcesses(procs, MAX_PROCS);

        // Limpa a tela para uma exibição mais limpa
        printf("\033[H\033[J");  // Outra forma de limpar a tela

        // Exibe a tabela
        displayProcesses(procs, num_procs);

        // Força a atualização do stdout
        fflush(stdout);

        // Atraso de 1 segundo antes da próxima atualização
        sleep(1);
    }

    return 0;
}

// Função para ler processos do diretório /proc e preencher o array de processos
int getProcesses(ProcessInfo procs[], int max_procs) {
    DIR *proc_dir = opendir(PROC_PATH);
    if (!proc_dir) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    struct dirent *entry;
    int count = 0;

    while ((entry = readdir(proc_dir)) && count < max_procs) {
        // Verifica se a entrada é um PID (somente números)
        if (!isdigit(entry->d_name[0])) continue;

        ProcessInfo *proc = &procs[count];

        // Obtém o PID do processo
        proc->pid = atoi(entry->d_name);

        // Obtém o nome do usuário a partir do UID do dono do processo
        struct stat st;
        char proc_path[512];
        snprintf(proc_path, sizeof(proc_path), "%s/%s", PROC_PATH, entry->d_name);
        if (stat(proc_path, &st) == -1) continue;

        struct passwd *pwd = getpwuid(st.st_uid);
        if (pwd) strncpy(proc->user, pwd->pw_name, sizeof(proc->user) - 1);
        else strncpy(proc->user, "unknown", sizeof(proc->user) - 1);

        // Lê o arquivo stat para obter o nome e estado do processo
        char stat_path[512];
        snprintf(stat_path, sizeof(stat_path), "%s/%s/%s", PROC_PATH, entry->d_name, STAT_FILE);

        FILE *stat_file = fopen(stat_path, "r");
        if (!stat_file) continue;

        fscanf(stat_file, "%*d (%[^)]) %c", proc->procname, &proc->state);
        fclose(stat_file);

        count++;
    }

    closedir(proc_dir);
    return count;
}

// Função para exibir os processos em uma tabela formatada
void displayProcesses(ProcessInfo procs[], int num_procs) {
    printf("PID\t| User\t\t| PROCNAME\t\t| Estado\n");
    printf("----\t| --------\t| ------------------\t| ------\n");

    for (int i = 0; i < num_procs; i++) {
        printf("%d\t| %-8s\t| %-16s\t| %c\n",
               procs[i].pid, procs[i].user, procs[i].procname, procs[i].state);
    }
}

// Função para verificar se o PID está na tabela
int isProcessValid(ProcessInfo procs[], int num_procs, int pid) {
    for (int i = 0; i < num_procs; i++) {
        if (procs[i].pid == pid) {
            return 1; // Processo encontrado
        }
    }
    return 0; // Processo não encontrado
}

// Thread para ler entrada do usuário e enviar sinais
void *signalInputThread(void *arg) {
    ProcessInfo *procs = (ProcessInfo *)arg;
    int pid, signal;

    while (1) {
        usleep(100000);  // 100ms
        printf("> ");
        if (scanf("%d %d", &pid, &signal) == 2) {
            // Verifica se o PID está na tabela antes de enviar o sinal
            if (isProcessValid(procs, MAX_PROCS, pid)) {
                if (kill(pid, signal) == -1) {
                    perror("kill");
                } else {
                    printf("Sinal %d enviado para o processo %d\n", signal, pid);
                }
            } else {
                printf("PID %d não encontrado na tabela.\n", pid);
            }
        }
    }
    return NULL;
}