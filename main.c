#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "PmodOLED.h"
#include "PmodKYPD.h"
#include "sleep.h"
#include "xil_cache.h"
#include "xil_printf.h"
#include "xparameters.h"


void DemoInitialize();
void DemoRun();
void DemoCleanup();
void DemoSleep(u32 millis);

PmodOLED myDevice;
PmodKYPD myKYPD;


// To change between PmodOLED and OnBoardOLED is to change Orientation
const u8 orientation = 0x0; // Set up for Normal PmodOLED(false) vs normal
                            // Onboard OLED(true)
const u8 invert = 0x0; // true = whitebackground/black letters
                       // false = black background /white letters


int main(void) {
   DemoInitialize();
   DemoRun();
   DemoCleanup();
   return 0;
}

// keytable is determined as follows (indices shown in Keypad position below)
// 12 13 14 15
// 8  9  10 11
// 4  5  6  7
// 0  1  2  3
#define DEFAULT_KEYTABLE "0FED789C456B123A"

void DemoInitialize() {
   KYPD_begin(&myKYPD, XPAR_PMODKYPD_0_AXI_LITE_GPIO_BASEADDR);
   KYPD_loadKeyTable(&myKYPD, (u8*) DEFAULT_KEYTABLE);
   OLED_Begin(&myDevice, XPAR_PMODOLED_0_AXI_LITE_GPIO_BASEADDR,
           XPAR_PMODOLED_0_AXI_LITE_SPI_BASEADDR, orientation, invert);
}


void DemoRun() {
	// KYPD
	u16 keystate;
	XStatus status, last_status = KYPD_NO_KEY;
	u8 key, last_key = 'x';
	// Initial value of last_key cannot be contained in loaded KEYTABLE string

	// OLED
	int top, right, bottom, left, loc;
	u8 *pat;

	Xil_Out32(myKYPD.GPIO_addr, 0xF);
	Xil_Out32(myDevice.GPIO_addr, 0xAAAA);

	int randomAsteroids = 1;

	top = 10;
	right = 125;
	bottom = 17;
	left = 118;

	int shipTop = 11;
	int shipRight = 10;
	int shipBottom = 16;
	int shipLeft = 5;

	int count = 0;
	int mode = 1;

	int playing = 0;
	int training = 0;
	int score = 0;

	typedef struct
	{
		float w1[5][3];
		float b1[5];
		float w2[2][5];
		float b2[2];
		int fitness;
	} Ship;

	int numShips = 20;
	int currentShip = 0;
	int populationFitness;
	Ship *population;
	Ship *tempPopulation;

   int contact() {
	   if (left < shipRight + 6) {
		  if ((top > shipTop - 8) && (top < shipBottom + 1)) {
			  return 1;
		  }
	   }

	   return 0;
   }

   void draw_ready() {
	   // Choosing Fill pattern 0
		pat = OLED_GetStdPattern(0);
		OLED_SetFillPattern(&myDevice, pat);
		// Turn automatic updating off
		OLED_SetCharUpdate(&myDevice, 0);
		// Clear and ready drawing
		OLED_SetDrawMode(&myDevice, OledModeSet);
		OLED_ClearBuffer(&myDevice);
   }

   void draw_ship() {
	   pat = OLED_GetStdPattern(1);
		OLED_SetFillPattern(&myDevice, pat);
		OLED_MoveTo(&myDevice, shipLeft, shipBottom);
		OLED_FillRect(&myDevice, shipRight, shipTop);
		OLED_DrawRect(&myDevice, shipRight, shipTop);

		OLED_SetFillPattern(&myDevice, pat);
		OLED_MoveTo(&myDevice, shipLeft - 3, shipTop + 1);
		OLED_FillRect(&myDevice, shipLeft, shipTop);
		OLED_DrawRect(&myDevice, shipLeft, shipTop);

		OLED_SetFillPattern(&myDevice, pat);
		OLED_MoveTo(&myDevice, shipLeft - 3, shipBottom);
		OLED_FillRect(&myDevice, shipLeft, shipBottom - 1);
		OLED_DrawRect(&myDevice, shipLeft, shipBottom - 1);


		OLED_SetFillPattern(&myDevice, pat);
		OLED_MoveTo(&myDevice, shipRight, shipBottom - 1);
		OLED_FillRect(&myDevice, shipRight + 3, shipTop + 1);
		OLED_DrawRect(&myDevice, shipRight + 3, shipTop + 1);

		OLED_SetFillPattern(&myDevice, pat);
		OLED_MoveTo(&myDevice, shipRight + 3, shipBottom - 2);
		OLED_FillRect(&myDevice, shipRight + 5, shipTop + 2);
		OLED_DrawRect(&myDevice, shipRight + 5, shipTop + 2);
   }

   void draw_asteroid() {
		pat = OLED_GetStdPattern(7);
		OLED_SetFillPattern(&myDevice, pat);
		OLED_MoveTo(&myDevice, left, bottom);
		OLED_FillRect(&myDevice, right, top);
		OLED_DrawRect(&myDevice, right, top);
		OLED_Update(&myDevice);
   }

   void update_asteroid() {
		if (count == (10 / mode)) {
			left--;
			right--;
			count = 0;
		} else {
			count++;
		}
   }

   void asteroid_passed() {
		if (left < 1) {
			if (randomAsteroids) {
				loc = (rand() % 21) + 4;
				left = 118;
				right = 125;
				top = loc - 4;
				bottom = loc + 3;
			} else {
				left = 118;
				right = 125;
				top = shipTop - 1;
				bottom = shipBottom + 1;
			}
			score++;
		}
	}

	int game_over() {
		if (contact()) {
			loc = (rand() % 21) + 4;
			left = 118;
			right = 125;
			top = loc - 4;
			bottom = loc + 3;
			return 1;
		}
		return 0;
	}

	void lobby() {
		score = 0;
		draw_ready();
		for (int irow = 0; irow < OledRowMax; irow++) {
			OLED_ClearBuffer(&myDevice);
			OLED_SetCursor(&myDevice, 0, 0);
			OLED_PutString(&myDevice, "Choose mode");
			OLED_SetCursor(&myDevice, 0, 2);
			OLED_PutString(&myDevice, "Easy: 1, Hard: 2");
			OLED_Update(&myDevice);
		}
	}

	char * toArray(int number) {
		int n = 0;
		int temp = number;
		while (temp != 0) {
			temp /= 10;
			n += 1;
		}
		char *numberArray = calloc(n, sizeof(char));
		if (n == 0) {
			numberArray[0] = 0 + '0';
		} else {
			for (int i = n - 1; i >= 0; --i, number /= 10) {
				numberArray[i] = (number % 10) + '0';
			}
		}
		return numberArray;
	}

	void game_over_screen() {
		for (int irow = 0; irow < OledRowMax; irow++) {
			OLED_ClearBuffer(&myDevice);
			OLED_SetCursor(&myDevice, 0, 0);
			OLED_PutString(&myDevice, "Game Over");
			OLED_SetCursor(&myDevice, 0, 2);
			OLED_PutString(&myDevice, "Score: ");
			OLED_SetCursor(&myDevice, 8, 2);
			char* score_string = toArray(score);
			OLED_PutString(&myDevice, score_string);

			OLED_MoveTo(&myDevice, 0, irow);
			OLED_FillRect(&myDevice, 127, 31);
			OLED_MoveTo(&myDevice, 0, irow);
			OLED_LineTo(&myDevice, 127, irow);
			OLED_Update(&myDevice);
		}
		sleep(5);
	}

	int parsed_input() {
		// Capture state of each key
		keystate = KYPD_getKeyStates(&myKYPD);

		// Determine which single key is pressed, if any
		status = KYPD_getKeyPressed(&myKYPD, keystate, &key);

		int x = 0;
		// Print key detect if a new key is pressed or if status has changed
		if (status == KYPD_SINGLE_KEY && (status != last_status || key != last_key)) {
			char c = (char) key;
			x = (int) c;
			last_key = key;
		}
		last_status = status;
		return x;
	}

	void move() {
		int x = parsed_input();
		if (x == 65) {
			 if (shipTop > 3) {
				shipTop--;
				shipBottom--;
			 }
		 } else if (x == 66) {
			 if (shipBottom < 26) {
				shipTop++;
				shipBottom++;
			 }
		 }
	}



	void game() {
		move();
		draw_ready();
		draw_ship();
		draw_asteroid();
		update_asteroid();
		asteroid_passed();
		if (game_over()) {
			playing = 0;
			game_over_screen();
			lobby();
		}
		usleep(1);
	}


	/******************** Genetic Algorithm ***********************/

	void init_population() {
		// New population of ships
		population = malloc(sizeof(Ship) * numShips);
		currentShip = 0;
		// init weights1
		for (int i = 0; i < numShips; i++) {
			for (int j = 0; j < 5; j++) {
				for (int k = 0; k < 3; k++) {
					population[i].w1[j][k] = ((float)rand() / (float)(RAND_MAX)) - 0.5;
				}
			}
		}
		// init biases1
		for (int i = 0; i < numShips; i++) {
			for (int j = 0; j < 5; j++) {
				population[i].b1[j] = ((float)rand() / (float)(RAND_MAX)) - 0.5;
			}
		}
		// init weights2
		for (int i = 0; i < numShips; i++) {
			for (int j = 0; j < 2; j++) {
				for (int k = 0; k < 5; k++) {
					population[i].w2[j][k] = ((float)rand() / (float)(RAND_MAX)) - 0.5;
				}
			}
		}
		// init biases2
		for (int i = 0; i < numShips; i++) {
			for (int j = 0; j < 2; j++) {
				population[i].b2[j] = ((float)rand() / (float)(RAND_MAX)) - 0.5;
			}
		}
		for (int i = 0; i < numShips; i++) {
			population[i].fitness = 0;
		}
	}

	float relu(float z) {
		if (z > 0) {
			return z;
		} else {
			return 0;
		}
	}

	float softmax(int i, float z[2]) {
		// float total = (float)(exp(z[0]) + exp(z[1]));
		// return exp(z[i]) / total;
		return z[i];
	}

	int predict(int s, int data[3]) {
		float z1[5];
		float a1[5];
		float z2[2];
		float a2[2];

		for (int i = 0; i < 5; i++) {
			z1[i] = population[s].b1[i];
			for (int j = 0; j < 3; j++) {
				z1[i] += population[s].w1[i][j] * (float)data[j];
			}
		}
		for (int i = 0; i < 5; i++) {
			a1[i] = relu(z1[i]);
		}
		for (int i = 0; i < 2; i++) {
			z2[i] = population[s].b2[i];
			for (int j = 0; j < 5; j++) {
				z2[i] += population[s].w2[i][j] * (float)a1[j];
			}
		}
		a2[0] = softmax(0, z2);
		a2[1] = softmax(1, z2);
		if (a2[0] > a2[1]) {
			return 0;
		} else {
			return 1;
		}
	}

	void decision() {
		int pos[3] = { shipTop, top, bottom };
		int d = predict(currentShip, pos);
		if (d == 0) {
			 if (shipTop > 3) {
				shipTop--;
				shipBottom--;
			 }
		 } else if (d == 1) {
			 if (shipBottom < 26) {
				shipTop++;
				shipBottom++;
			 }
		 }
	}

	void selection() {
		tempPopulation = malloc(sizeof(Ship) * numShips);
		int tempI = 0;
		for (int z = 0; z < numShips; z++) {
			int i = 0;
			float place = ((float)rand() / (float)(RAND_MAX));
			while (place > 0) {
				place -= ((float)(population[i].fitness) / (float)populationFitness);
				i++;
			}
			i--;
			for (int j = 0; j < 5; j++) {
				for (int k = 0; k < 3; k++) {
					tempPopulation[tempI].w1[j][k] = population[i].w1[j][k];
				}
			}
			for (int j = 0; j < 5; j++) {
				tempPopulation[tempI].b1[j] = population[i].b1[j];
			}
			for (int j = 0; j < 2; j++) {
				for (int k = 0; k < 5; k++) {
					tempPopulation[tempI].w2[j][k] = population[i].w2[j][k];
				}
			}
			for (int j = 0; j < 2; j++) {
				tempPopulation[tempI].b2[j] = population[i].b2[j];
			}
			tempI++;
		}
		for (int i = 0; i < numShips; i++) {
			for (int j = 0; j < 5; j++) {
				for (int k = 0; k < 3; k++) {
					population[i].w1[j][k] = tempPopulation[i].w1[j][k];
				}
			}
			for (int j = 0; j < 5; j++) {
				population[i].b1[j] = tempPopulation[i].b1[j];
			}
			for (int j = 0; j < 2; j++) {
				for (int k = 0; k < 5; k++) {
					population[i].w2[j][k] = tempPopulation[i].w2[j][k];
				}
			}
			for (int j = 0; j < 2; j++) {
				population[i].b2[j] = tempPopulation[i].b2[j];
			}
			population[i].fitness = 0;
		}
		free(tempPopulation);
	}

	void mutate() {

	}

	void train() {
		decision();
		draw_ready();
		draw_ship();
		draw_asteroid();
		update_asteroid();
		asteroid_passed();
		if (game_over()) {
			population[currentShip].fitness = score;
			score = 0;
			if (currentShip == numShips - 1) {
				printf("currentShip: %d\n", currentShip);
				// calc population fitness
				populationFitness = 0;
				for (int i = 0; i < numShips; i++) {
					populationFitness += population[i].fitness;
				}
				// selection
				selection();
				// mutation
				mutate();
				currentShip = 0;
			} else {
				currentShip++;
			}
		}
		usleep(1);
	}





	/**************************************************************/



	void check_lobby() {
		int x = parsed_input();
		if (x == 49 || x == 50) {
			mode = x - 48;
			playing = 1;
		} else if (x == 67) {
			mode = 10;
			training = 1;
			init_population();
		}
		usleep(1000);
	}

	lobby();
	while (1) {
		if (playing) {
			game();
		} else if (training) {
			train();
		} else {
			check_lobby();
		}
	}

}

void DemoCleanup() {
	OLED_End(&myDevice);
}

