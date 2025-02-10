#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#define STEP 10

/* ф-ция doubleSpecial вызывается когда встретлись: &, | или >
и смотрит является ли данная команда &&(соот. || >>)
и возвращает команду. в ch кладет следующий символ после команды */
char* doubleSpecial(char *ch) {
    char *str = calloc(3, sizeof(char)); //можно и просто массив
    char ch1;
    str[0] = *ch;
    ch1 = getchar();
    if (ch1 == *ch) {
        str[1] = ch1;
        str[2] = '\0';
        ch1 = getchar();
    } else str[1] = '\0';
    *ch = ch1;
    return str;
}

/*ф-ция append добавляет в конец массива слов слово
и расширяет этот массив если необходимо*/
void append(char ***arr, char *str, int word_count) {
    if ((word_count % STEP) == (STEP - 1)) {      //оставляем место под NULL в конец
        *arr = realloc(*arr, (word_count + 1 + STEP) * sizeof(char*));
    }
    (*arr)[word_count] = str;
    (*arr)[word_count + 1] = NULL;  //конец вектора и метка для вывода
}

void prdir() {
    char curdir[PATH_MAX];//текущая директория

    if (getcwd(curdir, sizeof(curdir)) != NULL) {
       printf("%s * ", curdir);
    } else {
       fprintf(stderr, "getcwd() error");
    }
}

int main() {
    char fl1 = 0;         //флаг отображающий открыты ли кавычки
    char fl2 = 0;         //флаг равен 1 когда слово записывается
    char fl3 = 0;         //флаг для перенаправления вывода
    char fl4 = 0;         //флаг фонового режима
    int outF;             //выходной файл
    char ch;
    char* str = calloc(STEP, sizeof(char));
    char** arr = calloc(STEP, sizeof(char*));
    int word_count = 0;   //кол-во записаных слов
    int ch_count = 0;     //кол-во записаных в слово символов
    int status = 0;
    int pid;

    signal(SIGINT, SIG_IGN);
    prdir();

    ch = getchar();
    while (ch != EOF) {
        if (ch == '"') {

            if (fl1) fl1 = 0;
            else {
                fl1 = 1;
                if (!fl2) fl2 = 1;
            }
            ch = getchar();
            continue;
        }

        //обработка специальных символов
        if (ch == '\n' || !fl1 ) {    //\n вне зависимости от fl1 всегда спец символ
            if (ch == '&' || ch == '|' || ch == '>') {

                if (fl2) {
                    str[ch_count] = '\0';
                    ch_count = 0;
                    append(&arr, str, word_count++);
                    fl2 = 0;
                } else free(str);
                str = doubleSpecial(&ch);
                append(&arr, str, word_count++);
                str = calloc(STEP, sizeof(char));
                continue; //doubleSpecial считытывает доп символ

            } else if (ch == ';' || ch == '<' || ch == '(' || ch == ')') {

                if (fl2) {
                    str[ch_count] = '\0';
                    ch_count = 0;
                    append(&arr, str, word_count++);
                    fl2 = 0;
                } else free(str); /*под строку всегда выделяется память
                                    поэтому если указатель на эту память 
                                    никуда не присвоен отчищаем её*/
                str = calloc(2, sizeof(char));
                str[0] = ch;
                str[1] = '\0';
                append(&arr, str, word_count++);
                str = calloc(STEP, sizeof(char));
                ch = getchar();
                continue;

            } else if (ch == ' ' || ch == '\n') {
                if (fl2) {
                    str[ch_count] = '\0';
                    append(&arr, str, word_count++);
                    str = calloc(STEP, sizeof(char));
                    fl2 = 0;
                    ch_count = 0;
                }
                if (ch == '\n') {
                    //смотрим на фоновые процессы
                    int bgpid;
                    while ((bgpid = waitpid(-1, &status, WNOHANG)) > 0){
                        if (WIFSIGNALED(status)) {
                            printf("Process %d aborted by signal %d\n", bgpid, WTERMSIG(status));
                        } else if (WIFEXITED(status)){
                            printf("Process %d exited with code %d\n", bgpid, WEXITSTATUS(status));
                        }
                    }

                    if (fl1) {  //не закрыта кавычка
                        fprintf(stderr, "string error!\n");
                            
                    } else if (!arr[0]){
                        fprintf(stderr, "empty command\n");
                    } else {
                        if (strcmp(arr[0], "cd")) {   //if arr[0] != "cd"
                            //определяем фоновый ли конвейер
                            int i = -1;
                            while (arr[++i]);
                            if (!strcmp(arr[i - 1], "&")) {
                                fl4 = 1;
                                free(arr[i - 1]);
                                arr[i - 1] = NULL;
                            } else fl4 = 0;
                            if ((pid = fork()) == 0) { // конвеер
                                int fd[2];
                                if (fl4) { 
                                    int inpFd = open("/dev/null", O_RDONLY);
                                    dup2(inpFd, 0);
                                    close(inpFd);
                                    //signal(SIGINT, SIG_IGN);
                                }
                                //перенаправление ввода/вывода
                                i = 0;
                                if (!fl4 && arr[i] && !strcmp(arr[i], "<") && arr[i + 1]) {
                                    int inpFd = open(arr[i + 1], O_RDONLY);
                                    if (inpFd != -1) {
                                        dup2(inpFd, 0);
                                        close(inpFd);
                                    } else {
                                        perror("Error open file");
                                        return 1;
                                    }
                                    i += 2;
                                }
                                if (arr[i] && !strcmp(arr[i], ">")  && arr[i + 1]) {
                                    fl3 = 1;
                                    outF = open(arr[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0777);
                                    i += 2;
                                }
                                if (arr[i] && !strcmp(arr[i], ">>")  && arr[i + 1]) {
                                    fl3 = 2;
                                    outF = open(arr[i + 1], O_WRONLY | O_CREAT | O_APPEND, 0777);
                                    i += 2;
                                }
                                while (arr[i]) {
                                    pipe(fd);
                                    char** argum = calloc(STEP, sizeof(char*)); //аргументы команды конвеера
                                    
                                    int j = 0;
                                    
                                    while (arr[i] && strcmp(arr[i], "|")) {
                                        argum[j++] = arr[i++];
                                        if ((j % STEP) == (STEP - 1)) {      //оставляем место под NULL в конец
                                            argum = realloc(argum, (j + 1 + STEP) * sizeof(char*));
                                        }
                                    }
                                    argum[j] = NULL;
                                    switch (fork()) {
                                        case -1:
                                            perror("fork error\n");
                                            exit(1);
                                            break;
                                        case 0:
                                            if (arr[i]) dup2(fd[1], 1);
                                            else if (fl3) {
                                                dup2(outF, 1);
                                                close(outF);
                                            }
                                            close(fd[0]);
                                            close(fd[1]);
                                            if (!fl4) {
                                                signal(SIGINT, SIG_DFL);
                                            }
                                            execvp(argum[0], argum);
                                            perror("exec error");

                                            exit(2);
                                        default: free(argum);
                                    }
                                    dup2(fd[0], 0);
                                    close(fd[0]);
                                    close(fd[1]);
                                    if (arr[i]) i++;
                                }
                                int status1 = 0;
                                while (wait(&status) != -1){
                                    
                                    status1 = status;
                                }
                                if (WIFEXITED(status) && !fl4) {
                                    fprintf(stderr, "Exit Status %d\n", WEXITSTATUS(status));
                                }
                                exit(status1);
                            } else {
                                printf("else code\n");
                                if (!fl4) {
                                    waitpid(pid, &status, 0);
                                    
                                }
                            }
                        } else {
                            if (!arr[1]) chdir(getenv("HOME"));
                            else if (arr[2]) fprintf(stderr, "too many cd arguments!\n"); //у cd 1 аргумент
                            else if (chdir(arr[1]))  fprintf(stderr, "cd failled\n");
                        }
                    } 

                    prdir();

                    //сброс параметров
                    int i = 0;
                    while (arr[i] != NULL) {
                        free(arr[i]);
                        i++;
                            
                    }
                    free(arr);
                    free(str);
                    fl1 = fl2 = 0;
                    word_count = 0;
                    str = calloc(STEP, sizeof(char));
                    arr = calloc(STEP, sizeof(char*));

                    ch = getchar();
                    continue;
                }
                ch = getchar();
                continue;
            }
        }

        //обработка символа

        //выделяем память и оставляем место под '\0'
        if ((ch_count % STEP) == (STEP - 1)) {
            str = realloc(str, (ch_count + 1 + STEP) * sizeof(char));
        }

        if (!fl2) fl2 = 1;
        str[ch_count++] = ch;
        ch = getchar();
    }  
    //free
    int i = 0;
    while (arr[i] != NULL) {
        free(arr[i]);
        i++;
    }
    free(arr);
    free(str);
    printf("\n");

    int bgpid;
    while ((bgpid = waitpid(-1, &status, 0)) > 0){
        if (WIFSIGNALED(status)) {
            printf("Process %d aborted by signal %d\n", bgpid, WTERMSIG(status));
        } else if (WIFEXITED(status)){
            printf("Process %d exited with code %d\n", bgpid, WEXITSTATUS(status));
        }
    }

    //kill(0, SIGKILL);
    return 0;
}