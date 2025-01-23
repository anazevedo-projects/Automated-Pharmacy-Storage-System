#include<conio.h>
#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include <windows.h> //for Sleep function
#include <iostream>
#include "my_interaction_functions.h"

extern "C" {
	#include <FreeRTOS.h>
	#include <task.h>
	#include <timers.h>
	#include <semphr.h>
	#include <interface.h>	
	#include <interrupts.h>
}

#define INCLUDE_vTaskDelete 1

#define mainREGION_1_SIZE   8201
#define mainREGION_2_SIZE   29905
#define mainREGION_3_SIZE   7607

xSemaphoreHandle sem_X_finished;
xSemaphoreHandle sem_Z_finished;
xSemaphoreHandle sem_XZ_finished;
xSemaphoreHandle sem_lamp_start;

xQueueHandle mbx_X;						//for goto_X
xQueueHandle mbx_Z;						//for go to Z
xQueueHandle mbx_XZ;					//for go to XZ
xQueueHandle mbx_inter;					//for intermediate XZ

xTaskHandle pharmacy_storage;
xTaskHandle goto_X;
xTaskHandle goto_Z;
xTaskHandle goto_XZ;
xTaskHandle intermediate_XZ;
xTaskHandle check_exp_drugs;
xTaskHandle switch1_flashing;
xTaskHandle flashing_emergency;


//struct of drug
typedef struct drug {
	char name[50];
	char laboratory[50];
	char nationalCode[10];
	int year;
	int month;
	int day;
} Drug;

Drug * storage[3][3];				//matriz que representa o estado do armazem

int position_expiry_drugs[20][2];	//matriz que contem a posicao dos medicamentos encontrados fora de prazo

Drug* expiry_drugs[20];				//matriz que tera o historico dos medicamentos que foram eliminados por estarem fora de prazo

bool flashing_control = FALSE;		//variavel que controla o comeco e o fim do piscar do LED1 por ter sido encontrado algum medicamento fora de prazo
bool flashing_faster = FALSE;		//variavel que controla o comeco e o fim do piscar dos LEDs 1 e 2 durante uma situacao de emergencia

bool stopTasks = FALSE;				//variavel de controlo para inicializar a suspensao de todas as tasks a excecao da task flashing_emergency_task
bool resumeOperation = FALSE;		//variavel de controlo usada quando o utilizador numa de situacao de emergencia decide fazer RESUME Oeration
bool resetOperation = FALSE;
uInt8 p22;							//guardamos nesta variavel os bits do p2 antes de suspendermos as tasks



void vAssertCalled(unsigned long ulLine, const char* const pcFileName)
{
	static BaseType_t xPrinted = pdFALSE;
	volatile uint32_t ulSetToNonZeroInDebuggerToContinue = 0;
	/* Called if an assertion passed to configASSERT() fails.  See
	http://www.freertos.org/a00110.html#configASSERT for more information. */
	/* Parameters are not used. */
	(void)ulLine;
	(void)pcFileName;
	printf("ASSERT! Line %ld, file %s, GetLastError() %ld\r\n", ulLine, pcFileName, GetLastError());

	taskENTER_CRITICAL();
	{
		/* Cause debugger break point if being debugged. */
		__debugbreak();
		/* You can step out of this function to debug the assertion by using
		   the debugger to set ulSetToNonZeroInDebuggerToContinue to a non-zero
		   value. */
		while (ulSetToNonZeroInDebuggerToContinue == 0)
		{
			__asm { NOP };
			__asm { NOP };
		}
	}
	taskEXIT_CRITICAL();
}

static void  initialiseHeap(void)
{
	static uint8_t ucHeap[configTOTAL_HEAP_SIZE];
	/* Just to prevent 'condition is always true' warnings in configASSERT(). */
	volatile uint32_t ulAdditionalOffset = 19;
	const HeapRegion_t xHeapRegions[] =
	{
		/* Start address with dummy offsetsSize */
		{ ucHeap + 1,mainREGION_1_SIZE },
		{ ucHeap + 15 + mainREGION_1_SIZE,mainREGION_2_SIZE },
		{ ucHeap + 19 + mainREGION_1_SIZE +
				mainREGION_2_SIZE,mainREGION_3_SIZE },
		{ NULL, 0 }
	};


	configASSERT((ulAdditionalOffset +
		mainREGION_1_SIZE +
		mainREGION_2_SIZE +
		mainREGION_3_SIZE) < configTOTAL_HEAP_SIZE);
	/* Prevent compiler warnings when configASSERT() is not defined. */
	(void)ulAdditionalOffset;
	vPortDefineHeapRegions(xHeapRegions);
}

void inicializarPortos() {
	printf("\nwaiting for hardware simulator...");
	printf("\nReminding: gotoXZ requires kit calibration first...");
	createDigitalInput(0);
	createDigitalInput(1);
	createDigitalOutput(2);
	writeDigitalU8(2, 0);
	printf("\ngot access to simulator...");
}


void goto_X_task(void* pvParameters) {
	int x;
	while (TRUE) {
		xQueueReceive(mbx_X, &x, portMAX_DELAY);	//recebe a mbx que contem a posicao X para onde qeuremos enviar o robo				
		gotoX(x);									//funcao responsavel por enviar o robo para a posicao pretendida
		xSemaphoreGive(sem_X_finished);				// signals task completion
	}
}

void goto_Z_task(void* pvParameters) {
	int z;
	while (TRUE) {
		xQueueReceive(mbx_Z, &z, portMAX_DELAY);	//recebe a mbx que contem a posicao Z para onde qeuremos enviar o robo	
		gotoZ(z);									//funcao responsavel por enviar o robo para a posicao pretendida
		xSemaphoreGive(sem_Z_finished);				// signals task completion
	}
}

void goto_XZ_task(void* pvParameters) {
	int x = 0, z = 0;
	
	while (TRUE) {
		xQueueReceive(mbx_XZ, &x, portMAX_DELAY);			//recebe a mbx que contem a posicao X para onde qeuremos enviar o robo
		xQueueReceive(mbx_XZ, &z, portMAX_DELAY);			//recebe a mbx que contem a posicao Z para onde qeuremos enviar o robo

		xQueueSend(mbx_X, &x, portMAX_DELAY);				//envio da posicao X atraves da mbx_X para a task goto_X_task
		xQueueSend(mbx_Z, &z, portMAX_DELAY);				//envio da posicao Z atraves da mbx_Z para a task goto_Z_task
		xQueueSemaphoreTake(sem_X_finished, portMAX_DELAY);	//sinaliza o fim da task goto_X_task
		xQueueSemaphoreTake(sem_Z_finished, portMAX_DELAY);	//sinaliza o fim da task goto_Z_task
		xSemaphoreGive(sem_XZ_finished);					// signals task completion
	}

}


void intermediate_XZ_task(void* pvParameters) {
	int check_move, x, z;

	while (TRUE) {

		xQueueReceive(mbx_inter, &x, portMAX_DELAY);				//recebe na mbx_inter que contem a posicao X para onde queremos enviar o robo
		xQueueReceive(mbx_inter, &z, portMAX_DELAY);				//recebe na mbx_inter que contem a posicao Z para onde queremos enviar o robo
		xQueueReceive(mbx_inter, &check_move, portMAX_DELAY);		//recebe na a veriavel que controla se estamos colocar um medicamento(check=1) ou se estamos a retirar um medicamento(check=2)

			if (check_move == 1) {									//if responsavel por colocar um medicamento no armazem 

				xQueueSend(mbx_XZ, &x, portMAX_DELAY);				//enviamos a posicao X atraves da mbx_XZ para a task goto_XZ_task
				xQueueSend(mbx_XZ, &z, portMAX_DELAY);				//enviamos a posicao Z atraves da mbx_XZ para a task goto_XZ_task
				xSemaphoreTake(sem_XZ_finished, portMAX_DELAY);		//sinaliza o fim da task goto_XZ_task
				putPartInCell();									//funcao responsavel por colocar o medicamento na celula pretendida



			}
			if (check_move == 2) {

				xQueueSend(mbx_XZ, &x, portMAX_DELAY);				//enviamos a posicao X atraves da mbx_XZ para a task goto_XZ_task
				xQueueSend(mbx_XZ, &z, portMAX_DELAY);				//enviamos a posicao Z atraves da mbx_XZ para a task goto_XZ_task
				xSemaphoreTake(sem_XZ_finished, portMAX_DELAY);		//sinaliza o fim da task goto_XZ_task
				takePartInCell();									//funcao responsavel por retirar o medicamento na celula pretendida
			}

			//este processo abaixo e usado para colocar o robo na posicao (1,1)(X,Z) e Y=1
			x = 1;													
			z = 1;
			xQueueSend(mbx_XZ, &x, portMAX_DELAY);
			xQueueSend(mbx_XZ, &z, portMAX_DELAY);
			xSemaphoreTake(sem_XZ_finished, portMAX_DELAY);
			gotoY(1);

	}
}

int pos = 0;

/*
* OBJETIVO: Task responsavel guardar as posicoes XZ do medicamento referenciado como fora de prazo
* INPUT: m - posicao X do medicamento
*		n - posicao Z do medicamento
*/
void positionOfExpDrug(int m, int n) {

	position_expiry_drugs[pos][0] = m;	//guarda a posicao X
	position_expiry_drugs[pos][1] = n;	//guarda a posicao z
	pos++;								//para que na proxima vez guardemos na proxima linha do buffer position_expiry_drugs

}


/*
* OBJETIVO: Task responsavel por verificar se o medicamento ja foi ou nao referenciado como fora de prazo
* INPUT: m - posicao X do medicamento
*		n - posicao Z do medicamento
* Output: 1- ja foi referenciado como fora de prazo
*		  0- ainda nao tinha sido referenciado
*/
int check_rep_expiry(int m, int n) {
	int i = 0;

	while (position_expiry_drugs[i][0] != -1) {													//para percorrer o buffer que guarda as posicoes dos medicamentos encontrados fora de prazo
		if ((m == position_expiry_drugs[i][0]) && (n == position_expiry_drugs[i][1])) {			//se a posicao do medicamento ja tiver guardada neste buffer
			return 1;																			//retornamos 1 

		}
		
		i++;
	}

	return 0;																				//caso as suas posicoes(m, n) ainda tenham sido guardadas retornamos 0


}

/*
* OBJETIVO: Task responsavel por estar constantemente a verificar se existem medicamentos fora de prazo dentro do armazem
*/
void check_exp_drugs_task(void* pvParameters) {
	SYSTEMTIME t;

	while (1) {
		GetLocalTime(&t);
		//os dois for's vao percorrer a matriz do storage
		for (int m = 0; m < 3; m++) {
			for (int n = 0; n < 3; n++) {
				if ((storage[m][n] != NULL) && (!check_rep_expiry(m, n))) {		//temos que garantir que a posicao do storage tem algum medicamento(!=NULL) e detetar se esse medicamento ja foi ou nao referenciado como estando em fora de validade
					

					if (storage[m][n]->year < t.wYear) {						//primeiro verificamos se o ano do medicamento ja passou. Se sim entao ja podemos concluir que esta fora de prazo
						flashing_control = TRUE;								//para ativar o LED1
						//xSemaphoreGive(sem_lamp_start);							//semaforo de controlo da task switch1_flashing_task
						positionOfExpDrug(m, n);								//funcao que vai guardar no buffer position_expiry_drugs a posicao do medicamento


					}
					else {														
						if (storage[m][n]->year == t.wYear) {					//caso o ano seja igual ao atual temos que verificar se o mes ja passou
							if (storage[m][n]->month < t.wMonth) {				//se ja passou vamos fazer o mesmo processo que fizemos para o ano
								flashing_control = TRUE;
								//xSemaphoreGive(sem_lamp_start);
								positionOfExpDrug(m, n);
							}
							else {
								if (storage[m][n]->month == t.wMonth) {			//caso o mes seja igual temos que verificar se o mes ja passou
									if (storage[m][n]->day < t.wDay) {
										flashing_control = TRUE;
										//xSemaphoreGive(sem_lamp_start);
										positionOfExpDrug(m, n);
									}
								}

							}

						}

					}
				}
			}
		}
	}
}


void switch1_flashing_task(void* pvParameters) {


	while (TRUE) {
		//xSemaphoreTake(sem_lamp_start, portMAX_DELAY);		//semaforo de controlo do LED1 quando se deteta na task check_exp_drugs_task medicamentos fora de validade
		while (flashing_control && !flashing_faster) {		//So ligamos apenas o LED1 quando garantimos que nao estamos em situacao de emrgencia <=> flashing_faster = false
			switch1ON();									//ligamos o LED1
			vTaskDelay(1000);								//momento de espera antes de desligarmos
			switch1OFF();									//desligamos o LED1
			vTaskDelay(1000);								//momento de espera antes de ligarmos
		}
	}
}


void pharmacy_storage_task(void* pvParameters) {


	int x = 1, z = 1;
	char cmd = NULL;
	int verify = 0;
	int check_move;

	printf("starting MENU\n");

	//as 4 linhas abaixo sao usadas para que o robo seja colocado na posicao de inicio sempre que termine uma opcao do MENU
	xQueueSend(mbx_XZ, &x, portMAX_DELAY);
	xQueueSend(mbx_XZ, &z, portMAX_DELAY);
	xSemaphoreTake(sem_XZ_finished, portMAX_DELAY);
	gotoY(1);

	while (TRUE) {

		printf("\n\n\t\tMenu\n\n");
		printf("1- Store a drug in the storage in the cell (x,z)\n");
		printf("2- Store a drug in the storage randomly\n");
		printf("3- Deliver drug from cell (x,z)\n");
		printf("4- Deliver drug with a specific national drug code\n");
		printf("5- Show list of stored drugs\n");
		printf("6- Show products by national drug code\n");
		printf("7- Show the record of all drugs removed\n");
		printf("8- Show status of the pharmacy storage\n\n");
		printf("M- Manual Control\n");
		printf("F- Fim\n\n\n");
		printf("Insert your choice: ");

		scanf(" %c", &cmd);

		//1 - Store a drug in the storage in the cell(x, z)
		if (cmd == '1') {

			Drug * d = (Drug*) malloc (sizeof(struct drug));
			char laboratory [50];
			char name[50];
			char drug_code[10];

			printf("Insert drug name: ");
			scanf("%s", name);
			strcpy_s(d->name, name);
			printf ("Insert laboratory: ");
			scanf("%s", laboratory);
			strcpy_s(d->laboratory, laboratory);
			printf ("Insert national drug code: ");
			scanf("%s", drug_code);
			strcpy_s(d->nationalCode, drug_code);
			printf ("Insert expiry date(dd/mm/yyyy): ");
			scanf("%d/%d/%d", &d->day, &d->month, &d->year); 

			printf("\nInsert X: ");
			scanf("%d", &x);
			printf("Insert Z: ");			
			scanf("%d", &z);	

			if(storage[x-1][z-1] == NULL)	//se a celula para onde o utilizador queria enviar o medicamento estiver vazia
				storage [x-1][z-1] = d;		//guardamos o medicamento
			else
			{
				while (storage[x-1][z-1] != NULL) {										//enquanto o utilizador escolher uma celula que nao esteja vazia  
					printf("cell [%d][%d] is not empty. Choose another one", x, z);		//vamos estar contantemente a informa-lo disso
					printf("\nInsert X: ");												//e voltamos a pedir para ele introduzir a posicao da celula
					scanf_s("%d", &x);
					printf("Insert Z: ");
					scanf_s("%d", &z);
				}
				storage[x-1][z-1] = d;													//por fim quando escolher uma posicao vazia entao guardamos o medicamento
			}

			gotoY(2);																	//coloca o medicamento dentro da cage
			while (getYPos() != 2) {													//enquanto nao estiver dentro da cage ficamos dentro deste ciclo

			}
			if (!check_drug_cage()) {													//caso o sensor vertical que esta dentro da cage nao detete nenhuma caixa  
				do {																	//vamos informar o utilizador que nao colocou nenhuma caixa
					printf("There is no drug at the cage.\nPlease insert it!\n");
					gotoY(1);
					vTaskDelay(3000);
					gotoY(2);
					while (getYPos() != 2) {

					}
				} while (!check_drug_cage());											//vamos ficar neste ciclo ate que seja colocada uma caixa
			}

			check_move = 1;																//check_move = 1 porque estamos a querer guardar um medicamento
			xQueueSend(mbx_inter, &x, portMAX_DELAY);									//envio da posicao x para a task intermedia
			xQueueSend(mbx_inter, &z, portMAX_DELAY);									//envio da posicao z para a task intermedia
			xQueueSend(mbx_inter, &check_move, portMAX_DELAY);							//envio da variavel check_move para a task intermedia

			verify = 1;
		}


		//2- Store a drug in the storage radomly
		if (cmd == '2') {
			Drug* d = (Drug*)malloc(sizeof(struct drug));
			char laboratory[50];
			char name[50];
			char drug_code[9];
			int check_loop = 0;

			
			printf("Insert drug name: ");
			scanf (" %s", name);
			strcpy_s(d->name, name);
			printf("Insert laboratory: ");
			scanf(" %s", laboratory);
			strcpy_s(d->laboratory, laboratory);
			printf("Insert national drug code: ");
			scanf(" %s", drug_code);
			strcpy_s(d->nationalCode, drug_code);
			printf("Insert expiry date(dd/mm/yyyy): ");
			scanf("%d/%d/%d", &d->day, &d->month, &d->year);

			//vamos percorrer o storage
			for (int m = 0; m < 3; m++) {

					for (int n = 0; n < 3; n++) {

						if (storage[m][n] == NULL) {	//quando encontrarmos uma posicao vazia
							storage[m][n] = d;			//guardamos o medicamento
							x = m + 1;					//posicao X para onde queeremos colocar o medicamento
							z = n + 1;					//posicao Z para onde queeremos colocar o medicamento
							gotoY(2);					//coloca o medicamento dentro da cage 
							while (getYPos() != 2) {	//enquanto nao estiver dentro da cage ficamos dentro deste ciclo

							}
							if (!check_drug_cage()) {	//caso o sensor vertical que esta dentro da cage nao detete nenhuma caixa 
								do {					//vamos informar o utilizador que nao colocou nenhuma caixa
									printf("There is no drug at the cage.\nPlease insert it!\n");
									gotoY(1);
									vTaskDelay(3000);
									gotoY(2);
									while (getYPos() != 2) {

									}
								} while (!check_drug_cage());	//vamos ficar neste ciclo ate que seja colocada uma caixa
							}

							check_move = 1;																//check_move = 1 porque estamos a querer guardar um medicamento
							xQueueSend(mbx_inter, &x, portMAX_DELAY);									//envio da posicao x para a task intermedia
							xQueueSend(mbx_inter, &z, portMAX_DELAY);									//envio da posicao z para a task intermedia
							xQueueSend(mbx_inter, &check_move, portMAX_DELAY);							//envio da variavel check_move para a task intermedia

							check_loop = 1;																//colocamos a variavel a 1 para pararmos o ciclo for
							break;
						}

					}

					if (check_loop == 1)
						break;
			}

			verify = 1;
		}


		//3 - Deliver drug from cell(x, z)
		if (cmd == '3') {

			printf("\nInsert X: ");
			scanf_s("%d", &x);
			printf("Insert Z: ");
			scanf_s("%d", &z);

			if (storage[x - 1][z - 1] != NULL) {						//verifica se a celula tem medicamento
					
				storage[x - 1][z - 1] = NULL;							//eliminamos o medicamento do nosso buffer que contem os medicamentos em loja
				check_move = 2;											//para a task intermedia saber que vai retirar um medicamento
				xQueueSend(mbx_inter, &x, portMAX_DELAY);				//envio da posicao x para a task intermedia
				xQueueSend(mbx_inter, &z, portMAX_DELAY);				//envio da posicao z para a task intermedia
				xQueueSend(mbx_inter, &check_move, portMAX_DELAY);		//envio da variavel check_move para a task intermedia

				
			}
			else
				printf("Empty Cell\n\n");								//informa o utilizador que a celula de onde ele queria retirar o medicamento esta vazia

			verify = 1;
		}

		//4 - Deliver drug with a specific national drug code
		if (cmd == '4') {

			char drug_code[10];
			int check_loop = 0;
			int x, z;
			Drug* d=NULL;


			printf("\nInsert national code: ");
			std::cin >> drug_code;

			//vamos percorrer a matriz storage para procurar o medicamento que tenha o mesmo codigo nacional que o que o utilizador inseriu
			for (int m = 0; m < 3; m++) {

				for (int n = 0; n < 3; n++) {
					d = storage[m][n];
					if (d != NULL) {	
						if (strcmp(d->nationalCode, drug_code) == 0) {				//quando encontrar o medicamento estamos prontos a entrega-lo
							
							storage[m][n] = NULL;									//elimina-o da loja(memoria)
							x = m + 1;												//ppsicao X do medicamento
							z = n + 1;												//ppsicao Z do medicamento
							check_move = 2;											//para a task intermedia saber que vai retirar um medicamento
							xQueueSend(mbx_inter, &x, portMAX_DELAY);				//envio da posicao x para a task intermedia
							xQueueSend(mbx_inter, &z, portMAX_DELAY);				//envio da posicao z para a task intermedia
							xQueueSend(mbx_inter, &check_move, portMAX_DELAY);		//envio da variavel check_move para a task intermedia
							check_loop = 1;
							break;
						}
					}
				}

				if (check_loop == 1)
					break;
			}

			if (check_loop == 0)											//Percorremos o storage todo e nao encontramos nenhum medicamento com o mesmo codigo nacional
				printf("There is no Drug with that national code\n\n");
		
			verify = 1;
		}


		//5- Show list of stored drugs
		if (cmd == '5') {

			char  buffer_national[10][10] = {"\0", "\0", "\0", "\0", "\0", "\0", "\0", "\0", "\0", "\0"};
			Drug* d = NULL;
			int check_national = 0;
			int k = 0;

			for (int m = 0; m < 3; m++) {

				for (int n = 0; n < 3; n++) {

					if (storage[m][n] != NULL) {
						d = storage[m][n];

						//Ciclo que nos vai asegurar que nao imprimimos informacao de medicamentos iguais, ou seja, que tenham o mesmo codigo nacional
						while (strcmp (buffer_national[k], "\0")) {
							if (strcmp(buffer_national[k], d->nationalCode) == 0) {
								check_national = 1;
							}
							k++;
						}
						
						if (check_national == 0) {												//se a variavel check nacional estiver a zero quer dizer que as informacoes do medicamento ainda nao foram impressas
							printf("Drug Name: %s\n", d->name);
							printf("Drug laboratory: %s\n", d->laboratory);
							printf("Drug national drug code: %s\n", d->nationalCode);
							printf("Expiry date: %d/%d/%d\n\n", d->day, d->month, d->year);
							strcpy(buffer_national[k], d->nationalCode);

						}

						if (check_national == 1)	
							check_national = 0;

						k = 0;
					}
				}
			}

			verify = 1;
		}

		//6- Show products by national drug code
		if (cmd == '6') {

			Drug* d = NULL;
			char sub_cmd [10];
			int x, z, check_code = 0;
			
			printf("Insert national code: ");
			scanf (" %s", sub_cmd);

			//Neste ciclo vamos listar todos os medicamentos que contenham o codigo nacional que o utilizador introduziu
			for (int m = 0; m < 3; m++) {

				for (int n = 0; n < 3; n++) {
					if (storage[m][n] != NULL) {
						d = storage[m][n];

						x = m + 1;
						z = n + 1;

						if (strcmp(d->nationalCode, sub_cmd) == 0) {									//verifica se o medicamento tem o mesmo codigo nacional
							printf("Cell [%d][%d] -> %d:%d:%d\n", x, z, d->day, d->month, d->year);		//caso encontre entao vamos imprimir informacao sobre a posicao do medicamento e sobre a sua data de validade
							check_code = 1;

						}


					}

				}
			}
	
			//Se nao encontrar nenhum medicamento entao informamos o utilizador que nao foi encontrado nenhum
			if (check_code == 0) {

				printf("National code not found\n");	

			}
	
			check_code = 0;
			verify = 1;
		}


		//7- Show the record of all drugs removed
		if (cmd == '7') {
			int i = 0;

			printf("\t\tExpired Drugs:\n\n");

			while (expiry_drugs[i] != NULL) {																					//vamos percorrer o buffer que contem todo o historico de medicamentos ja removidos
				printf("Name: %s\n", expiry_drugs[i]->name);																	//imprimimos o nome do medicamento
				printf("Expiry Date: %d/%d/%d\n\n", expiry_drugs[i]->day, expiry_drugs[i]->month, expiry_drugs[i]->year);		//e imprimimos tambem a data de validade dd/mm/yy
				i++;
			}


			verify = 1;
		}


		//8 - Show status of the pharmacy storage
		if (cmd == '8') {
			int x, z;

			//vamos percorrer cada posicao da matriz storage
			for (int m = 0; m < 3; m++) {

				for (int n = 0; n < 3; n++) {
					x = m + 1;
					z = n + 1;
					printf("Cell [%d][%d]: ", x, z);																			//imprime a posicao em estamos																	
					
					if (storage[m][n] != NULL) {																				//caso tenha medicamento entao imprimimos toda a informacao referente a este
						printf("\nDrug Name: %s\n", storage[m][n]->name);														//NOME
						printf("Drug laboratory: %s\n", storage[m][n]->laboratory);												//NOME DO LABORATORIO
						printf("Drug national drug code: %s\n", storage[m][n]->nationalCode);									//CODIGO NACIONAL
						printf("Expiry date: %d/%d/%d\n\n", storage[m][n]->day, storage[m][n]->month, storage[m][n]->year);		//DATA DE VALIDADE

					}
					else {
						printf("\nEmpty\n\n");																					//quando a celula nao tem medicamento
					}
				}

			}


			verify = 1;
		}

		//M - Manual Control
		if (cmd == 'M') {
			char sub_cmd=NULL;
			int x, y, z;

			taskENTER_CRITICAL();															//entramos em situacao critica para que o sistema bloqueie enquanto estamos no contrlo manual
			
			do  {

				printf("1- Control XZ\n2-Control Y\n3-STOP\nE- Manual control exit\n");
				printf("\n\nInsert your choice: ");
				scanf(" %c", &sub_cmd);
				
				
				switch (sub_cmd) {
					case '1':																//vamos colocar o robo na posicao XZ inserida pelo uilizador
						printf("\nInsert X: ");
						scanf(" %d", &x);
						printf("Insert Z: ");
						scanf(" %d", &z);
						gotoX(x);
						gotoZ(z);
						break;

					case '2':																//colocamos o robo na posicao Y inserida pelo utilizador
						printf("\nInsert Y: ");
						scanf(" %d", &y);
						gotoY(y);
						break;

					case '3':																//STOP de todos os motores
						stopX();
						stopY();
						stopZ();
				}


			} while (sub_cmd != 'E');
						
			taskEXIT_CRITICAL();															//quando terminamos o controlo manual queremos sair da zona critica para continuar o resto das tasks

			verify = 1;
		}

		//F - fim -> termina com o programa
		if (cmd == 'F') {
			exit(0);
		}


		if (verify == 0) {

			printf("Comando inexistente.\n");
		}
	}
}


void flashing_emergency_task(void* pvParameters) {

	int x, z, check_move;

	while (TRUE) {

		if (flashing_faster) {				//so entramos aqui quando os dois botoes foram pressioandos <=> paragem de emergencia
			if (stopTasks) {					//este if vai tratar da paragem das tasks
				p22 = readDigitalU8(2);		//vamos guardar o estado do p2(porto de movimento) porque podemos enventualmente precisar para fazer RESUME OPERATION
				vTaskSuspendAll();			//todas as tasks estao suspensas a excecao desta
				emergency_stop();			//funcao que trata da paragem -> stopX() stopY() stopZ()
				stopTasks = FALSE;			//colocamos a variavel a false para que este if nao esteja sempre a ser feito
			}

			while (flashing_faster) {		//enquanto estivermos em paragem de emergencia este ciclo vai ser efetuado
				switch1ON();				//ligamos o LED1
				switch2ON();				//ligamos o LED2
				vTaskDelay(500);			//momento de espera antes de desligarmos os LEDs
				switch1OFF();				//desligamos o LED1
				switch2OFF();				////desligamos o LED2
				vTaskDelay(500);			//momento de espera antes de ligarmos os LEDs

				//RESUME OPERATION
				if (resumeOperation) {			//ocorre quando durante o estado de emergencia o utilizador pressiona o botao1(RESUME OPERATION)
					restaure_bits(p22);			//vamos repor os bits do porto 2, guardados antes de se suspender todas as tasks 
					xTaskResumeAll();			//para voltar a todas as tasks que tinham sido suspensas pela situacao de emergencia
					resumeOperation = FALSE;	//para terminar o RESUME 
					flashing_faster = FALSE;	//termina com a situacao de emergencia porque o utilizador ja decidiu fazer RESUME OPERATION <=> CLICOU NO SWITCH1
				}

				if (resetOperation) {
					restaure_bits(p22);			//vamos repor os bits do porto 2, guardados antes de se suspender todas as tasks 
					xTaskResumeAll();			//para voltar a todas as tasks que tinham sido suspensas pela situacao de emergencia
					
					while((getXPos()!= 1) && (getZPos() != 1) && (getYPos() != 1)) {

					}

					taskENTER_CRITICAL();

					for (int m = 0; m < 3; m++) {
						for (int n = 0; n < 3; n++) {
							if (storage[m][n] != NULL) {
								storage[m][n] = NULL;									//eliminamos os medicamentos do storage
								x = m + 1;												//posicao fisica tem de ser igual a posicao da matriz mais um 
								z = n + 1;												//posicao fisica tem de ser igual a posicao da matriz mais um 
								
								gotoX(x);												//vai a posicao X 
								gotoZ(z);												//e para a posicao Z para retirar depois o medicamento
								takePartInCell();										//funcao responsavel por retirar o medicamento na celula pretendida

								//vai entregar o medicamento portanto vai ate a posicao inicial (x,y,z) = (1,1,1)
								gotoX(1);												
								gotoZ(1);
								gotoY(1);
							}
						}
					}

					//reset dos buffers
					for (int i = 0; i < 20; i++) {
						position_expiry_drugs[i][0] = -1;
						position_expiry_drugs[i][1] = -1;
						expiry_drugs[i] = NULL;
						pos = 0;
					}

					taskEXIT_CRITICAL();

					//DELETE dos semaforos
					vSemaphoreDelete(sem_X_finished);
					vSemaphoreDelete(sem_Z_finished);
					vSemaphoreDelete(sem_XZ_finished);
					vSemaphoreDelete(sem_lamp_start);

					//CRIACAO dos semaforos
					sem_X_finished = xSemaphoreCreateCounting(10, 0);													//criacao do semaforo X que sinaliza o fim da task gotoX_task
					sem_Z_finished = xSemaphoreCreateCounting(10, 0);													//criacao do semaforo Z que sinaliza o fim da task gotoZ_task
					sem_XZ_finished = xSemaphoreCreateCounting(10, 0);													//criacao do semaforo XZ que sinaliza o fim da task gotoXZ_task
					sem_lamp_start = xSemaphoreCreateCounting(10, 0);													//criacao do semaforo responsavel por iniciar o piscar do LED1 quando e detetado um medicamento fora de prazo

					//RESET das mbx's
					xQueueReset(mbx_X);
					xQueueReset(mbx_Z);
					xQueueReset(mbx_XZ);
					xQueueReset(mbx_inter);

					//Criacao das tasks
					xTaskCreate(pharmacy_storage_task, "Pharmacy Storage Task", 100, NULL, 0, &pharmacy_storage);
					xTaskCreate(goto_X_task, "GOTO X Task", 100, NULL, 0, &goto_X);
					xTaskCreate(goto_Z_task, "GOTO Z Task", 100, NULL, 0, &goto_Z);
					xTaskCreate(goto_XZ_task, "GOTO XZ Task", 100, NULL, 0, &goto_XZ);
					xTaskCreate(intermediate_XZ_task, "Intermediate XZ Task", 100, NULL, 0, &intermediate_XZ);
					xTaskCreate(check_exp_drugs_task, "Check Expiry Drugs Task", 100, NULL, 0, &check_exp_drugs);
					xTaskCreate(switch1_flashing_task, "Switch 1 Flashing Lamp Task", 100, NULL, 0, &switch1_flashing);

					resetOperation = FALSE;
					flashing_faster = FALSE;

				}
			}
		}
	}
}



void switch1_falling_isr(ULONGLONG lastTime) {

	ULONGLONG  time = GetTickCount64();
	uInt8 p1 = readDigitalU8(1);
	uInt8 p2 = readDigitalU8(2);
	int i = 0, check_move, x, z;


	if (getBitValue(p1, 6)) {
		flashing_faster = TRUE;					//variavel de controlo para acender as luzes de emergencia
		stopTasks = TRUE;						//variavel de controlo para para todas as tasks que estavam a ser executadas
		printf("\n\t\tEMERGENCY!\n");

	}
	else {
		if (flashing_faster) {					//switch1 pressionado durante situacao de emergencia => o tulizador quer continuar onde estava 	
			printf("Resuming Operation\n");
			resumeOperation = TRUE;				//variavel de controlo para realizar a paragem das tasks
		}
		else {							//switch1 pressionado para eliminar medicamentos fora de validade
			printf("Starting eliminating the expired drugs!\n");

			while ((position_expiry_drugs[i][0] != -1) && (position_expiry_drugs[i][1] != -1)) {		//percorremos todas as posicoes do vetor que tem as posicoes dos medicamentos

				expiry_drugs[i] = storage[position_expiry_drugs[i][0]][position_expiry_drugs[i][1]];	//guardamos o apontador do medicamento a ser removido
				storage[position_expiry_drugs[i][0]][position_expiry_drugs[i][1]] = NULL;				//eliminamos o medicamento da loja
				x = position_expiry_drugs[i][0] + 1;													//posicao X do medicamento
				z = position_expiry_drugs[i][1] + 1;													//posicao Z do medicamento
				i++;
				gotoY(2);
				check_move = 2;																			//chech_move = 2 significa que queremos retirar um medicamento da parteleira
				xQueueSend(mbx_inter, &x, portMAX_DELAY);												//enviamos as posicoes XZ e o valor da variavel check move por mbx para a task intermedia
				xQueueSend(mbx_inter, &z, portMAX_DELAY);
				xQueueSend(mbx_inter, &check_move, portMAX_DELAY);
			}

			i = 0;
			while ((position_expiry_drugs[i][0] != -1) && (position_expiry_drugs[i][1] != -1)) {	//RESET ao vetor que contem as posiçóes dos medicamentos que estavam fora de prazo
				position_expiry_drugs[i][0] = -1;
				position_expiry_drugs[i][1] = -1;
				pos = 0;																			//para que da proxima vez que encontre um medicamento fora de prazo comecemos a guardar do inicio do buffer
				i++;
			}

			flashing_control = FALSE;				//colocando esta variavel a false a luz do LED1 que indica que existem medicamentos fora de prazo vai desligar
		}
	}

}

void switch2_falling_isr(ULONGLONG lastTime) {

	uInt8 p1 = readDigitalU8(1);

	if (!(getBitValue(p1, 5)) && flashing_faster) {

		flashing_control = FALSE;
		resumeOperation = FALSE;
		resetOperation = TRUE;

	}
}



void myDaemonTaskStartupHook(void) {

	inicializarPortos();

	//Semaforo
	sem_X_finished = xSemaphoreCreateCounting (10, 0);													//criacao do semaforo X que sinaliza o fim da task gotoX_task
	sem_Z_finished = xSemaphoreCreateCounting (10, 0);													//criacao do semaforo Z que sinaliza o fim da task gotoZ_task
	sem_XZ_finished = xSemaphoreCreateCounting (10, 0);													//criacao do semaforo XZ que sinaliza o fim da task gotoXZ_task
	sem_lamp_start = xSemaphoreCreateCounting (10, 0);													//criacao do semaforo responsavel por iniciar o piscar do LED1 quando e detetado um medicamento fora de prazo

	
	//MailBox
	mbx_X = xQueueCreate(10, sizeof(int));																//criacao da mbx_X
	mbx_Z = xQueueCreate(10, sizeof(int));																//criacao da mbx_Z
	mbx_XZ = xQueueCreate(10, sizeof(int));																//criacao da mbx_XZ
	mbx_inter = xQueueCreate(10, sizeof(int));															//criacao da mbx_inter


	//Criacao de todas as tasks que serao utilziadas no decorrer do programa
	xTaskCreate(pharmacy_storage_task, "Pharmacy Storage Task", 100, NULL, 0, &pharmacy_storage);
	xTaskCreate(goto_X_task, "GOTO X Task", 100, NULL, 0, &goto_X);
	xTaskCreate(goto_Z_task, "GOTO Z Task", 100, NULL, 0, &goto_Z);
	xTaskCreate(goto_XZ_task, "GOTO XZ Task", 100, NULL, 0, &goto_XZ);
	xTaskCreate(intermediate_XZ_task, "Intermediate XZ Task", 100, NULL, 0, &intermediate_XZ);
	xTaskCreate(check_exp_drugs_task, "Check Expiry Drugs Task", 100, NULL, 0, &check_exp_drugs);
	xTaskCreate(switch1_flashing_task, "Switch 1 Flashing Lamp Task", 100, NULL, 0, &switch1_flashing);
	xTaskCreate(flashing_emergency_task, "Flash Emergency Task", 100, NULL, 0, &flashing_emergency);

	attachInterrupt(1, 5, switch1_falling_isr, RISING);  //pressionar o botao esquerdo
	attachInterrupt(1, 6, switch2_falling_isr, RISING);

}


int main()
{
	initialiseHeap();
	vApplicationDaemonTaskStartupHook = &myDaemonTaskStartupHook;

	//inicializacao da matriz storage
	for (int m = 0; m < 3; m++) {
		for (int n = 0; n < 3; n++) {
			storage[m][n] = NULL;

		}
	}

	//inicializacao dos buffers position_expiry_drugs(contem a posicao dos medicamentos encontrados fora de prazo) e expiry_drugs(contem o apontador dos medicamentos ja eliminados)
	for (int i = 0; i < 20; i++) {
		position_expiry_drugs[i][0] = -1;
		position_expiry_drugs[i][1] = -1;
		expiry_drugs[i] = NULL;
	}

	vTaskStartScheduler();
	closeChannels();
}
