/*********************************************************************************
*
*  AUTOMAÇÃO EM TEMPO REAL - ELT012
*  Prof. Luiz T. S. Mendes - 2018/2
*
*  Atividade em classe - 10/09/2018
*
*  Este programa deve ser completado com as linhas de programa necessárias
*  para solucionar o "problema da ponte estreita" ("The one-lane bridge",
*  G. Andrew, "Multithread, Parallel and Distributed Computing",
*  Addison-Wesley, 2000).
*
* O programa é composto de uma thread primária e 20 threads secundárias. A thread
* primária cria os objetos de sincronização e as threads secundárias. As threads
* secundárias correspondem a 10 carros que trafegam no sentido Norte-Sul (NS) e a
* 10 carros no sentido Sul-Norte (SN).
*
* A sinalização de término de programa é feita através da tecla ESC. Leituras da
* última tecla digitada devem ser feitas em pontos apropriados para que as threads
* detectem esta tecla.
*
**********************************************************************************/

#define WIN32_LEAN_AND_MEAN 
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>	
#include <conio.h>						//_getch
#define HAVE_STRUCT_TIMESPEC
#include <pthread.h>
#include <semaphore.h>

#define	ESC				0x1B			//Tecla para encerrar o programa
#define N_NorteSul		10				//Número de carros no sentido Norte-Sul
#define N_SulNorte		10				//Idem, Sul-Norte

#define NS 0
#define SN 1

#define WHITE   FOREGROUND_RED   | FOREGROUND_GREEN | FOREGROUND_BLUE
#define HLGREEN FOREGROUND_GREEN | FOREGROUND_INTENSITY
#define HLRED   FOREGROUND_RED   | FOREGROUND_INTENSITY

/* Declaracao dos prototipos de funcoes correspondetes aas threads secundarias*/
/* Atencao para o formato conforme o exemplo abaixo! */
void* Thread_SN(void* arg);
void* Thread_NS(void* arg);
void* Thread_Controladora(void* arg);

/* Declaração dos objetos de sincronização */
pthread_mutexattr_t MutexAttr;
pthread_mutex_t mutex_NS;        //Mutex para proteger acesso ao contador cont_NS
pthread_mutex_t mutex_SN;        //Mutex para proteger acesso ao contador cont_SN
sem_t PonteLivre;				 //Semáforo para sinalizar ponte livre/ocupada

int nTecla;						 //Variável que armazena a tecla digitada para sair
int cont_NS = 0, cont_SN = 0;	 //Contadores de carros atravessando a ponte
int vez;

int fila_NS = 0, fila_SN = 0;

HANDLE hOut;					 //Handle para a saída da console

time_t tempo_NS;
time_t tempo_SN;
pthread_mutex_t mutex_fila_SN;
pthread_mutex_t mutex_fila_NS;
pthread_cond_t Sentido_NS;				 //Semáforo para sinalizar sentido impedido
pthread_cond_t Sentido_SN;				 //Semáforo para sinalizar sentido impedido

/*===============================================================================*/
/* Corpo das funções auxiliares Wait(), Signal(), LockMutex e UnLockMutex. Estas */
/* funções assumem que o semaforo [Wait() e Signal()] ou o mutex [LockMutex() e  */
/* UnLockMutex()] recebido como parametro ja´ tenha sido previamente criado.     */
/*===============================================================================*/

bool Vez_NS() {
	time_t tempo_atual;
	time(&tempo_atual);
	return ((long long)(tempo_atual + 5)) > ((long long)(tempo_NS + 5));
}

bool Vez_SN() {
	time_t tempo_atual;
	time(&tempo_atual);
	return ((long long)(tempo_atual + 5)) > ((long long)(tempo_SN + 5));
}

void Wait(sem_t* Semaforo) {
	int status;
	status = sem_wait(Semaforo);
	if (status != 0) {
		printf("Erro na obtencao do semaforo! Codigo = %x\n", errno);
		exit(0);
	}
}

void Signal(sem_t* Semaforo) {
	int status;
	status = sem_post(Semaforo);
	if (status != 0) {
		printf("Erro na liberacao do semaforo! Codigo = %x\n", errno);
		exit(0);
	}
}

void LockMutex(pthread_mutex_t* Mutex) {
	int status;
	status = pthread_mutex_lock(Mutex);
	if (status != 0) {
		printf("Erro na conquista do mutex! Codigo = %d\n", status);
		exit(0);
	}
}

void UnLockMutex(pthread_mutex_t* Mutex) {
	int status;
	status = pthread_mutex_unlock(Mutex);
	if (status != 0) {
		printf("Erro na liberacao do mutex! Codigo = %d\n", status);
		exit(0);
	}
}

/*=====================================================================================*/
/* Thread Primaria                                                                     */
/*=====================================================================================*/

int main() {
	pthread_t hThreads[N_NorteSul + N_SulNorte];
	pthread_t threadControladora;
	void* tRetStatus;
	int i, status;

	// --------------------------------------------------------------------------
	// Obtém um handle para a saída da console
	// --------------------------------------------------------------------------

	hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hOut == INVALID_HANDLE_VALUE)
		printf("Erro ao obter handle para a saída da console\n");

	// --------------------------------------------------------------------------
	// Criação dos mutexes
	// --------------------------------------------------------------------------

	pthread_mutexattr_init(&MutexAttr); //sempre retorna 0
	status = pthread_mutexattr_settype(&MutexAttr, PTHREAD_MUTEX_ERRORCHECK);
	if (status != 0) {
		printf("Erro nos atributos do Mutex ! Codigo = %d\n", status);
		exit(0);
	}
	status = pthread_mutex_init(&mutex_NS, &MutexAttr);
	if (status != 0) {
		printf("Erro na criação do Mutex NS! Codigo = %d\n", status);
		exit(0);
	}
	status = pthread_mutex_init(&mutex_SN, &MutexAttr);
	if (status != 0) {
		printf("Erro na criação do Mutex SN! Codigo = %d\n", status);
		exit(0);
	}
	status = pthread_mutex_init(&mutex_fila_NS, &MutexAttr);
	if (status != 0) {
		printf("Erro na criação do Mutex fila NS! Codigo = %d\n", status);
		exit(0);
	}
	status = pthread_mutex_init(&mutex_fila_SN, &MutexAttr);
	if (status != 0) {
		printf("Erro na criação do Mutex fila SN! Codigo = %d\n", status);
		exit(0);
	}

	// --------------------------------------------------------------------------
	// Criação do semáforo binário
	// --------------------------------------------------------------------------

	status = sem_init(&PonteLivre, 0, 1); //sempre retorna 0
	if (status != 0) {
		printf("Erro na inicializacao do semaforo ! Codigo = %d\n", errno);
		exit(0);
	}
	status = sem_init(&Sentido_NS, 0, 1); //sempre retorna 0
	if (status != 0) {
		printf("Erro na inicializacao do semaforo ! Codigo = %d\n", errno);
		exit(0);
	}
	status = sem_init(&Sentido_SN, 0, 1); //sempre retorna 0
	if (status != 0) {
		printf("Erro na inicializacao do semaforo ! Codigo = %d\n", errno);
		exit(0);
	}

	// --------------------------------------------------------------------------
	// Criação das threads secundárias
	// --------------------------------------------------------------------------

	for (i = 0; i < N_NorteSul; i++) {
		status = pthread_create(&hThreads[i], NULL, Thread_NS, (void*)i);
		SetConsoleTextAttribute(hOut, WHITE);
		if (status == 0) printf("Thread Norte-Sul %d criada com Id= %0d \n", i, (int)&hThreads[i]);
		else {
			printf("Erro na criacao da thread Norte-Sul %d! Codigo = %d\n", i, status);
			exit(0);
		}
	}// end for

	for (i = 0; i < N_SulNorte; i++) {
		SetConsoleTextAttribute(hOut, WHITE);
		status = pthread_create(&hThreads[i + N_NorteSul], NULL, Thread_SN, (void*)i);
		if (status == 0) printf("Thread Sul-Norte %d criada com Id= %0d \n", i, (int)&hThreads[i + N_NorteSul]);
		else {
			printf("Erro na criacao da thread Sul-Norte %d! Codigo = %d\n", i, status);
			exit(0);
		}
	}// end for

	//Thread controladora
	/*status = pthread_create(&threadControladora, NULL, Thread_Controladora, (void*) 1);
	if (status == 0) printf("Thread Controladora criada!\n");
	else {
		printf("Erro na criacao da thread Controladora\n");
		exit(0);
	}*/

	// --------------------------------------------------------------------------
	// Leitura do teclado
	// --------------------------------------------------------------------------

	do {
		printf("Tecle <Esc> para terminar\n");
		nTecla = _getch();
	} while (nTecla != ESC);

	// --------------------------------------------------------------------------
	// Aguarda termino das threads secundarias
	// --------------------------------------------------------------------------

	for (i = 0; i < N_NorteSul + N_SulNorte; i++) {
		SetConsoleTextAttribute(hOut, WHITE);
		printf("Aguardando termino da thread %d...\n", (int)&hThreads[i]);
		status = pthread_join(hThreads[i], &tRetStatus);
		SetConsoleTextAttribute(hOut, WHITE);
		if (status != 0) printf("Erro em pthread_join()! Codigo = %d\n", status);
		else printf("Thread %d: status de retorno = %d\n", i, (int)tRetStatus);
	}

	// --------------------------------------------------------------------------
	// Elimina os objetos de sincronização criados
	// --------------------------------------------------------------------------

	SetConsoleTextAttribute(hOut, WHITE);
	status = pthread_mutex_destroy(&mutex_NS);
	if (status != 0) printf("Erro na remocao do mutex! i = %d valor = %d\n", i, status);

	status = pthread_mutex_destroy(&mutex_SN);
	if (status != 0) printf("Erro na remocao do mutex! i = %d valor = %d\n", i, status);

	status = sem_destroy(&PonteLivre);
	if (status != 0) printf("Erro na remocao do semaforo AcordaPai! Valor = %d\n", errno);

	CloseHandle(hOut);

	return EXIT_SUCCESS;

}//end main

/*=====================================================================================*/
/* Threads secundarias                                                                 */
/*=====================================================================================*/

void* Thread_Controladora(void* arg) {
	do {
		time_t tempo_atual;
		time(&tempo_atual);
		if ((long long)tempo_atual > (long long)(tempo_NS + 5)) {
			printf("Passando a vez para Norte-Sul\n");
			Signal(&Sentido_NS);
			time(&tempo_NS);
			Wait(&Sentido_SN);
		}
		if ((long long)tempo_atual > (long long)(tempo_SN + 5) && false) {
			printf("Passando a vez para Sul-Norte\n");
			Signal(&Sentido_SN);
			time(&tempo_SN);
			Wait(&Sentido_NS);
		}
	} while (nTecla != ESC);

	printf("Thread controladora encerrando execucao...\n");
	pthread_exit(NULL);
	return(0);
}

void* Thread_NS(void* arg) {  /* Threads representando carros no sentido Norte-Sul */

	int i = (int)arg;
	do {

		// ACRESCENTE OS COMANDOS DE SINCRONIZACAO VIA SEMAFOROS ONDE NECESSARIO

		LockMutex(&mutex_fila_SN);
		if (Vez_SN() && fila_SN > 0) {
			Wait(&Sentido_NS);
		}

		// Verifica se já há carros atravessando a ponte no mesmo sentido N-S
		LockMutex(&mutex_fila_NS);
		fila_NS++;

		LockMutex(&mutex_NS);
		if (cont_NS == 0) {
			time(&tempo_NS);
			SetConsoleTextAttribute(hOut, HLRED);
			printf("Primeiro carro a chegar na entrada Norte: aguarda a ponte ficar livre\n");
			Wait(&PonteLivre);
			printf("Ponte livre!\n");
			vez = NS;
		}

		// Carro entra na ponte no sentido Norte-Sul
		cont_NS++;
		fila_NS--;

		UnLockMutex(&mutex_NS);

		UnLockMutex(&mutex_fila_NS);

		SetConsoleTextAttribute(hOut, HLRED);
		printf("Carro %d atravessando a ponte no sentido Norte-Sul...\n", i);

		// Carro gasta um tempo aleatorio para atravessar a ponte
		Sleep(100 * (rand() % 10));

		printf("Carro %d saindo da ponte no sentido Norte-Sul...\n", i);

		// Carro sai da ponte
		LockMutex(&mutex_NS);
		cont_NS--;

		if (cont_NS == 0) {
			Signal(&PonteLivre);
		}
		UnLockMutex(&mutex_NS);

		SetConsoleTextAttribute(hOut, HLRED);
		printf("Carro %d saiu da ponte no sentido Norte-Sul...\n", i);

	} while (nTecla != ESC);

	//Encerramento da thread
	SetConsoleTextAttribute(hOut, HLRED);
	printf("Thread-carro N-S %d encerrando execucao...\n", i);
	pthread_exit(NULL);
	// O comando "return" abaixo é desnecessário, mas presente aqui para compatibilidade
	// com o Visual Studio da Microsoft
	return(0);
}//Thread_NS

void* Thread_SN(void* arg) {  /* Threads representando carros no sentido Sul-Norte */

	int i = (int)arg;
	do {

		// ACRESCENTE OS COMANDOS DE SINCRONIZACAO VIA SEMAFOROS ONDE NECESSARIO


		if (Vez_SN() && vez == NS) {
			printf("Passando a vez para Norte-Sul\n");
			Signal(&Sentido_NS);
			Wait(&Sentido_SN);
		}

		// Verifica se já há carros atravessando a ponte no sentido Sul-Norte
		LockMutex(&mutex_SN);
		if (cont_SN == 0) {
			time(&tempo_SN);
			SetConsoleTextAttribute(hOut, HLGREEN);
			printf("Primeiro carro a chegar na entrada Sul: aguarda a ponte ficar livre\n");
			Wait(&PonteLivre);
			printf("Ponte livre!\n");
			vez = SN;
		}

		// Carro atravessa a ponte no sentido Sul-Norte
		cont_SN++;
		UnLockMutex(&mutex_SN);

		SetConsoleTextAttribute(hOut, HLGREEN);
		printf("Carro %d atravessando a ponte no sentido Sul-Norte...\n", i);

		// Carro gasta um tempo aleatorio para atravessar a ponte
		Sleep(100 * (rand() % 10));

		printf("Carro %d saindo da ponte no sentido Sul-Norte...\n", i);

		// Carro sai da ponte
		LockMutex(&mutex_SN);
		cont_SN--;

		if (cont_SN == 0) {
			Signal(&PonteLivre);
		}
		UnLockMutex(&mutex_SN);

		SetConsoleTextAttribute(hOut, HLGREEN);
		printf("Carro %d saiu da ponte no sentido Sul-Norte...\n", i);

	} while (nTecla != ESC);

	//Encerramento da thread
	SetConsoleTextAttribute(hOut, HLGREEN);
	printf("Thread-carro S-N %d encerrando execucao...\n", i);
	pthread_exit(NULL);
	// O comando "return" abaixo é desnecessário, mas presente aqui para compatibilidade
	// com o Visual Studio da Microsoft
	return(0);
}//Thread_SN
