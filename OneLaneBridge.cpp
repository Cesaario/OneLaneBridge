/*********************************************************************************
*
*  AUTOMA��O EM TEMPO REAL - ELT012
*  Prof. Luiz T. S. Mendes - 2018/2
*
*  Atividade em classe - 10/09/2018
*
*  Este programa deve ser completado com as linhas de programa necess�rias
*  para solucionar o "problema da ponte estreita" ("The one-lane bridge",
*  G. Andrew, "Multithread, Parallel and Distributed Computing",
*  Addison-Wesley, 2000).
*
* O programa � composto de uma thread prim�ria e 20 threads secund�rias. A thread
* prim�ria cria os objetos de sincroniza��o e as threads secund�rias. As threads
* secund�rias correspondem a 10 carros que trafegam no sentido Norte-Sul (NS) e a
* 10 carros no sentido Sul-Norte (SN).
*
* A sinaliza��o de t�rmino de programa � feita atrav�s da tecla ESC. Leituras da
* �ltima tecla digitada devem ser feitas em pontos apropriados para que as threads
* detectem esta tecla.
*
**********************************************************************************/

#define WIN32_LEAN_AND_MEAN 
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>						//_getch
#define HAVE_STRUCT_TIMESPEC
#include <pthread.h>
#include <semaphore.h>

#define	ESC				0x1B			//Tecla para encerrar o programa
#define N_NorteSul		10				//N�mero de carros no sentido Norte-Sul
#define N_SulNorte		10				//Idem, Sul-Norte

#define NS 0
#define SN 1

#define WHITE		FOREGROUND_RED   | FOREGROUND_GREEN     | FOREGROUND_BLUE
#define HLGREEN		FOREGROUND_GREEN | FOREGROUND_INTENSITY
#define HLRED		FOREGROUND_RED   | FOREGROUND_INTENSITY
#define HLBLUE		FOREGROUND_BLUE  | FOREGROUND_INTENSITY
#define HLYELLOW	FOREGROUND_BLUE  | FOREGROUND_GREEN     | FOREGROUND_INTENSITY
#define HLPURPLE	FOREGROUND_BLUE  | FOREGROUND_RED     | FOREGROUND_INTENSITY

/* Declaracao dos prototipos de funcoes correspondetes aas threads secundarias*/
/* Atencao para o formato conforme o exemplo abaixo! */
void* Thread_SN(void* arg);
void* Thread_NS(void* arg);
void* Thread_Controladora(void* arg);

/* Declara��o dos objetos de sincroniza��o */
pthread_mutexattr_t MutexAttr;
pthread_mutex_t mutex_NS;        //Mutex para proteger acesso ao contador cont_NS
pthread_mutex_t mutex_SN;        //Mutex para proteger acesso ao contador cont_SN
sem_t PonteLivre;				 //Sem�foro para sinalizar ponte livre/ocupada

int nTecla;						 //Vari�vel que armazena a tecla digitada para sair
int cont_NS = 0, cont_SN = 0;	 //Contadores de carros atravessando a ponte

HANDLE hOut;					 //Handle para a sa�da da console

time_t tempo_NS;
time_t tempo_SN;

pthread_cond_t controle_NS;             //Vari�vel de condi��o de controle do sentido NS
pthread_cond_t controle_SN;				//Vari�vel de condi��o de controle do sentido SN
pthread_mutex_t mutex_controle_NS;		//Mutex de controle do sentido NS
pthread_mutex_t mutex_controle_SN;		//Mutex de controle do sentido SN
pthread_mutex_t mutex_fila_NS;			//Mutex para o contador da fila do sentido NS
pthread_mutex_t mutex_fila_SN;			//Mutex para o contador da fila do sentido SN
pthread_mutex_t mutex_print;			//Mutex para controlar as cores da fun��o printf
int fila_NS = 0, fila_SN = 0;			//Contadores de carros na fila
int vez = NS;							//Vari�vel controladora do turno

/*===============================================================================*/
/* Corpo das fun��es auxiliares Wait(), Signal(), LockMutex e UnLockMutex. Estas */
/* fun��es assumem que o semaforo [Wait() e Signal()] ou o mutex [LockMutex() e  */
/* UnLockMutex()] recebido como parametro ja� tenha sido previamente criado.     */
/*===============================================================================*/

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

void WaitCondition(pthread_cond_t* Cond, pthread_mutex_t* Mutex) {
	int status;
	status = pthread_cond_wait(Cond, Mutex);
	if (status != 0) {
		printf("Erro na espera da vari�vel de condi��o! Codigo = %d\n", status);
		exit(0);
	}
}

void SignalCondition(pthread_cond_t* Cond) {
	int status;
	status = pthread_cond_signal(Cond);
	if (status != 0) {
		printf("Erro na sinaliza��o da vari�vel de condi��o! Codigo = %d\n", status);
		exit(0);
	}
}

/*=====================================================================================*/
/* Thread Primaria                                                                     */
/*=====================================================================================*/

int main() {
	pthread_t hThreads[N_NorteSul + N_SulNorte];
	pthread_t ThreadControladora;
	void* tRetStatus;
	int i, status;

	// --------------------------------------------------------------------------
	// Obt�m um handle para a sa�da da console
	// --------------------------------------------------------------------------

	hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hOut == INVALID_HANDLE_VALUE)
		printf("Erro ao obter handle para a sa�da da console\n");

	// --------------------------------------------------------------------------
	// Cria��o dos mutexes
	// --------------------------------------------------------------------------

	pthread_mutexattr_init(&MutexAttr); //sempre retorna 0
	status = pthread_mutexattr_settype(&MutexAttr, PTHREAD_MUTEX_ERRORCHECK);
	if (status != 0) {
		printf("Erro nos atributos do Mutex ! Codigo = %d\n", status);
		exit(0);
	}
	status = pthread_mutex_init(&mutex_NS, &MutexAttr);
	if (status != 0) {
		printf("Erro na cria��o do Mutex NS! Codigo = %d\n", status);
		exit(0);
	}
	status = pthread_mutex_init(&mutex_SN, &MutexAttr);
	if (status != 0) {
		printf("Erro na cria��o do Mutex SN! Codigo = %d\n", status);
		exit(0);
	}
	status = pthread_mutex_init(&mutex_controle_SN, &MutexAttr);
	if (status != 0) {
		printf("Erro na cria��o do Mutex Controle SN! Codigo = %d\n", status);
		exit(0);
	}
	status = pthread_mutex_init(&mutex_controle_NS, &MutexAttr);
	if (status != 0) {
		printf("Erro na cria��o do Mutex Controle NS! Codigo = %d\n", status);
		exit(0);
	}
	status = pthread_mutex_init(&mutex_fila_SN, &MutexAttr);
	if (status != 0) {
		printf("Erro na cria��o do Mutex Fila SN! Codigo = %d\n", status);
		exit(0);
	}
	status = pthread_mutex_init(&mutex_fila_NS, &MutexAttr);
	if (status != 0) {
		printf("Erro na cria��o do Mutex Fila NS! Codigo = %d\n", status);
		exit(0);
	}
	status = pthread_mutex_init(&mutex_print, &MutexAttr);
	if (status != 0) {
		printf("Erro na cria��o do Mutex Print! Codigo = %d\n", status);
		exit(0);
	}

	// --------------------------------------------------------------------------
	// Cria��o do sem�foro bin�rio
	// --------------------------------------------------------------------------

	status = sem_init(&PonteLivre, 0, 1); //sempre retorna 0
	if (status != 0) {
		printf("Erro na inicializacao do semaforo ! Codigo = %d\n", errno);
		exit(0);
	}

	// --------------------------------------------------------------------------
	// Cria��o das vari�veis de condi��o
	// --------------------------------------------------------------------------

	status = pthread_cond_init(&controle_NS, NULL); //sempre retorna 0
	if (status != 0) {
		printf("Erro na inicializacao da var�vel de condi��o NS ! Codigo = %d\n", errno);
		exit(0);
	}
	status = pthread_cond_init(&controle_SN, NULL); //sempre retorna 0
	if (status != 0) {
		printf("Erro na inicializacao da var�vel de condi��o SN ! Codigo = %d\n", errno);
		exit(0);
	}

	// --------------------------------------------------------------------------
	// Cria��o das threads secund�rias
	// --------------------------------------------------------------------------

	SetConsoleTextAttribute(hOut, WHITE);
	status = pthread_create(&ThreadControladora, NULL, Thread_Controladora, NULL);
	if (status == 0) printf("Thread Controladora criada!\n");
	else {
		printf("Erro na criacao da Thread Controladora\n");
		exit(0);
	}

	for (i = 0; i < N_NorteSul; i++) {
		status = pthread_create(&hThreads[i], NULL, Thread_NS, (void*)i);
		LockMutex(&mutex_print);
		SetConsoleTextAttribute(hOut, WHITE);
		if (status == 0) printf("Thread Norte-Sul %d criada com Id= %0d \n", i, (int)&hThreads[i]);
		else {
			printf("Erro na criacao da thread Norte-Sul %d! Codigo = %d\n", i, status);
			exit(0);
		}
		UnLockMutex(&mutex_print);
	}// end for

	for (i = 0; i < N_SulNorte; i++) {
		LockMutex(&mutex_print);
		SetConsoleTextAttribute(hOut, WHITE);
		status = pthread_create(&hThreads[i + N_NorteSul], NULL, Thread_SN, (void*)i);
		if (status == 0) printf("Thread Sul-Norte %d criada com Id= %0d \n", i, (int)&hThreads[i + N_NorteSul]);
		else {
			printf("Erro na criacao da thread Sul-Norte %d! Codigo = %d\n", i, status);
			exit(0);
		}
		UnLockMutex(&mutex_print);
	}// end for

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
	// Elimina os objetos de sincroniza��o criados
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

		long long tempo = (long long) tempo_atual;

		if (tempo % 10 == 0) {
			if (vez == NS) {
				if (fila_SN > 0) {
					vez = SN;
					SignalCondition(&controle_SN);
					LockMutex(&mutex_print);
					SetConsoleTextAttribute(hOut, HLBLUE);
					printf("Passando controle para o sentido SN\n");
					UnLockMutex(&mutex_print);
				} else {
					LockMutex(&mutex_print);
					SetConsoleTextAttribute(hOut, HLYELLOW);
					printf("Sem carros na fila SN, turno continua de NS...\n");
					UnLockMutex(&mutex_print);
				}
			}
			else {
				if (fila_NS > 0 || true) {
					vez = NS;
					SignalCondition(&controle_NS);
					LockMutex(&mutex_print);
					SetConsoleTextAttribute(hOut, HLBLUE);
					printf("Passando controle para o sentido NS\n");
					UnLockMutex(&mutex_print);
				} else {
					LockMutex(&mutex_print);
					SetConsoleTextAttribute(hOut, HLYELLOW);
					printf("Sem carros na fila NS, turno continua de SN\n");
					UnLockMutex(&mutex_print);
				}
			}
		}

		Sleep(1000);
	} while (nTecla != ESC);

	//Encerramento da thread
	LockMutex(&mutex_print);
	SetConsoleTextAttribute(hOut, HLPURPLE);
	printf("Thread Controladora encerrando execucao...\n");
	UnLockMutex(&mutex_print);
	pthread_exit(NULL);

	return(0);
}

void* Thread_NS(void* arg) {  /* Threads representando carros no sentido Norte-Sul */

	int i = (int)arg;
	do {
		LockMutex(&mutex_fila_NS);
		fila_NS++;
		UnLockMutex(&mutex_fila_NS);

		if (vez == SN){
			LockMutex(&mutex_controle_NS);
			WaitCondition(&controle_NS, &mutex_controle_NS);
			UnLockMutex(&mutex_controle_NS);
		}
		if (vez == NS) {
			SignalCondition(&controle_NS);
		}
		
		// Verifica se j� h� carros atravessando a ponte no mesmo sentido N-S
		LockMutex(&mutex_NS);
		if (cont_NS == 0) {
			LockMutex(&mutex_print);
			SetConsoleTextAttribute(hOut, HLRED);
			printf("Primeiro carro a chegar na entrada Norte: aguarda a ponte ficar livre\n");
			UnLockMutex(&mutex_print);
			Wait(&PonteLivre);
			printf("Ponte livre!\n");
		}

		// Carro entra na ponte no sentido Norte-Sul
		cont_NS++;
		fila_NS--;
		UnLockMutex(&mutex_NS);

		LockMutex(&mutex_print);
		SetConsoleTextAttribute(hOut, HLRED);
		printf("Carro %d atravessando a ponte no sentido Norte-Sul...\n", i);
		UnLockMutex(&mutex_print);

		// Carro gasta um tempo aleatorio para atravessar a ponte
		Sleep(100 * (rand() % 10));

		// Carro sai da ponte
		LockMutex(&mutex_NS);
		cont_NS--;

		if (cont_NS == 0) {
			Signal(&PonteLivre);
		}
		UnLockMutex(&mutex_NS);

		LockMutex(&mutex_print);
		SetConsoleTextAttribute(hOut, HLRED);
		printf("Carro %d saiu da ponte no sentido Norte-Sul...\n", i);
		UnLockMutex(&mutex_print);

	} while (nTecla != ESC);

	//Encerramento da thread
	LockMutex(&mutex_print);
	SetConsoleTextAttribute(hOut, HLPURPLE);
	printf("Thread-carro N-S %d encerrando execucao...\n", i);
	UnLockMutex(&mutex_print);
	pthread_exit(NULL);
	// O comando "return" abaixo � desnecess�rio, mas presente aqui para compatibilidade
	// com o Visual Studio da Microsoft
	return(0);
}//Thread_NS

void* Thread_SN(void* arg) {  /* Threads representando carros no sentido Sul-Norte */
	int i = (int)arg;
	do {

		// ACRESCENTE OS COMANDOS DE SINCRONIZACAO VIA SEMAFOROS ONDE NECESSARIO

		LockMutex(&mutex_fila_SN);
		fila_SN++;
		UnLockMutex(&mutex_fila_SN);

		if (vez == NS) {
			LockMutex(&mutex_controle_SN);
			WaitCondition(&controle_SN, &mutex_controle_SN);
			UnLockMutex(&mutex_controle_SN);
		}

		if (vez == SN) {
			SignalCondition(&controle_SN);
		}

		// Verifica se j� h� carros atravessando a ponte no sentido Sul-Norte
		LockMutex(&mutex_SN);
		if (cont_SN == 0) {
			LockMutex(&mutex_print);
			SetConsoleTextAttribute(hOut, HLGREEN);
			printf("Primeiro carro a chegar na entrada Sul: aguarda a ponte ficar livre\n");
			UnLockMutex(&mutex_print);
			Wait(&PonteLivre);
			printf("Ponte livre!\n");
		}

		fila_SN--;

		// Carro atravessa a ponte no sentido Sul-Norte
		cont_SN++;
		UnLockMutex(&mutex_SN);

		LockMutex(&mutex_print);
		SetConsoleTextAttribute(hOut, HLGREEN);
		printf("Carro %d atravessando a ponte no sentido Sul-Norte...\n", i);
		UnLockMutex(&mutex_print);

		// Carro gasta um tempo aleatorio para atravessar a ponte
		Sleep(100 * (rand() % 10));

		// Carro sai da ponte
		LockMutex(&mutex_SN);
		cont_SN--;

		if (cont_SN == 0) {
			Signal(&PonteLivre);
		}
		UnLockMutex(&mutex_SN);

		LockMutex(&mutex_print);
		SetConsoleTextAttribute(hOut, HLGREEN);
		printf("Carro %d saiu da ponte no sentido Sul-Norte...\n", i);
		UnLockMutex(&mutex_print);

	} while (nTecla != ESC);

	//Encerramento da thread
	LockMutex(&mutex_print);
	SetConsoleTextAttribute(hOut, HLPURPLE);
	printf("Thread-carro S-N %d encerrando execucao...\n", i);
	UnLockMutex(&mutex_print);
	pthread_exit(NULL);
	// O comando "return" abaixo � desnecess�rio, mas presente aqui para compatibilidade
	// com o Visual Studio da Microsoft
	return(0);
}//Thread_SN
