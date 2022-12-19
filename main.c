#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "PmodOLED.h"
#include "PmodKYPD.h"
#include "sleep.h"
#include "xil_cache.h"
#include "xil_printf.h"
#include "xparameters.h"

// for referencing Pmods throughout file
PmodOLED myDevice;
PmodKYPD myKYPD;

/********************************* from digilent *********************************/
// To change between PmodOLED and OnBoardOLED is to change Orientation
const u8 orientation = 0x0; // Set up for Normal PmodOLED(false) vs normal
                            // Onboard OLED(true)
const u8 invert = 0x0; // true = whitebackground/black letters
                       // false = black background /white letters

/*********************************************************************************/

// KYPD specs
#define DEFAULT_KEYTABLE "0FED789C456B123A"
// initialize the Pmods
void init() {
   KYPD_begin(&myKYPD, XPAR_PMODKYPD_0_AXI_LITE_GPIO_BASEADDR);
   KYPD_loadKeyTable(&myKYPD, (u8*) DEFAULT_KEYTABLE);
   OLED_Begin(&myDevice, XPAR_PMODOLED_0_AXI_LITE_GPIO_BASEADDR,
           XPAR_PMODOLED_0_AXI_LITE_SPI_BASEADDR, orientation, invert);
}
// use OLED lib for device cleanup
void cleanup() {
	OLED_End(&myDevice);
}

// run the game, called in main
void run() {
	// KYPD
	u16 keystate;
	XStatus status, last_status = KYPD_NO_KEY;
	u8 key, last_key = 'x';

	// Pmod config
	u8 *pat;
	Xil_Out32(myKYPD.GPIO_addr, 0xF);
	Xil_Out32(myDevice.GPIO_addr, 0xAAAA);

	// location constants
	int top, right, bottom, left, loc;
	top = 10;
	right = 125;
	bottom = 17;
	left = 118;
	int shipTop = 11;
	int shipRight = 10;
	int shipBottom = 16;
	int shipLeft = 5;

	// game constants
	int randomAsteroids = 0;
	int count = 0;
	int decisionCount = 0;
	int mode = 2;
	int playing = 0;
	int training = 0;
	int score = 0;

	// struct for the spaceship bots
	typedef struct
	{
		float w1[5][3];
		float b1[5];
		float w2[2][5];
		float b2[2];
		int fitness;
	} Ship;

	// constants for neuro population
	int numShips = 10;
	int currentShip = 0;
	int populationFitness;
	float mutationRate = 0.1;
	Ship *population;
	Ship *tempPopulation;

	// check if asteroid hit the spaceship
   int contact() {
	   if (left < shipRight + 6) {
		  if ((top > shipTop - 8) && (top < shipBottom + 1)) {
			  return 1;
		  }
	   }
	   return 0;
   }

   // enable oled environment
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

   // render spaceship on screen
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

   // render asteroid on screen
   void draw_asteroid() {
		pat = OLED_GetStdPattern(7);
		OLED_SetFillPattern(&myDevice, pat);
		OLED_MoveTo(&myDevice, left, bottom);
		OLED_FillRect(&myDevice, right, top);
		OLED_DrawRect(&myDevice, right, top);
		OLED_Update(&myDevice);
   }

   // update asteroids position on oled
   void update_asteroid() {
		if (count >= (10 / mode)) {
			left--;
			right--;
			count = 0;
		} else {
			count++;
		}
   }

   // check if asteroid is off screen, if so, generate new asteroid
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

   // check if game is over
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

	// show options in lobby - hide secret mode
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

	// Method to convert score to string
	char * scoreString(int num) {
		int n = 0;
		int temp = num;
		while (temp != 0) {
			temp /= 10;
			n += 1;
		}
		char *res = calloc(n, sizeof(char));
		if (n == 0) {
			res[0] = 0 + '0';
		} else {
			for (int i = n - 1; i >= 0; --i, num /= 10) {
				res[i] = (num % 10) + '0';
			}
		}
		return res;
	}

	// When game over, display score
	void game_over_screen() {
		for (int irow = 0; irow < OledRowMax; irow++) {
			OLED_ClearBuffer(&myDevice);
			OLED_SetCursor(&myDevice, 0, 0);
			OLED_PutString(&myDevice, "Game Over");
			OLED_SetCursor(&myDevice, 0, 2);
			OLED_PutString(&myDevice, "Score: ");
			OLED_SetCursor(&myDevice, 8, 2);
			char* score_string = scoreString(score);
			OLED_PutString(&myDevice, score_string);

			OLED_MoveTo(&myDevice, 0, irow);
			OLED_FillRect(&myDevice, 127, 31);
			OLED_MoveTo(&myDevice, 0, irow);
			OLED_LineTo(&myDevice, 127, irow);
			OLED_Update(&myDevice);
		}
		sleep(5);
	}

	// modularize parsing KYPD input
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

	// render players move
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

	// workflow for player game mode
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

	// initialize neural network weights and biases of population
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

	// reLU activation function
	float relu(float z) {
		if (z > 0) {
			return z;
		} else {
			return 0;
		}
	}

	// simplified softmax activation function
	float softmax(int i, float z[2]) {
		// float total = (float)(exp(z[0]) + exp(z[1]));
		// return exp(z[i]) / total;
		return z[i];
	}

	// use neural net for prediction
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

	// act on the prediction
	void decision() {
		if (decisionCount == 4) {
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
			decisionCount = 0;
		} else {
			decisionCount++;
		}
	}

	// selection, reproduction - genetic algorithm
	void new_generation() {
		tempPopulation = malloc(sizeof(Ship) * numShips);
		int tempI = 0;
		for (int z = 0; z < numShips; z++) {
			int i1 = 0;
			float place = ((float)rand() / (float)(RAND_MAX));
			while (place > 0) {
				place -= ((float)(population[i1].fitness) / (float)populationFitness);
				i1++;
			}
			i1--;
			int i2 = 0;
			place = ((float)rand() / (float)(RAND_MAX));
			while (place > 0) {
				place -= ((float)(population[i2].fitness) / (float)populationFitness);
				i2++;
			}
			i2--;
			for (int j = 0; j < 5; j++) {
				for (int k = 0; k < 3; k++) {
					if (((float)rand() / (float)(RAND_MAX)) < 0.5) {
						tempPopulation[tempI].w1[j][k] = population[i1].w1[j][k];
					} else {
						tempPopulation[tempI].w1[j][k] = population[i2].w1[j][k];
					}
				}
			}
			for (int j = 0; j < 5; j++) {
				if (((float)rand() / (float)(RAND_MAX)) < 0.5) {
					tempPopulation[tempI].b1[j] = population[i1].b1[j];
				} else {
					tempPopulation[tempI].b1[j] = population[i2].b1[j];
				}
			}
			for (int j = 0; j < 2; j++) {
				for (int k = 0; k < 5; k++) {
					if (((float)rand() / (float)(RAND_MAX)) < 0.5) {
						tempPopulation[tempI].w2[j][k] = population[i1].w2[j][k];
					} else {
						tempPopulation[tempI].w2[j][k] = population[i2].w2[j][k];
					}
				}
			}
			for (int j = 0; j < 2; j++) {
				if (((float)rand() / (float)(RAND_MAX)) < 0.5) {
					tempPopulation[tempI].b2[j] = population[i1].b2[j];
				} else {
					tempPopulation[tempI].b2[j] = population[i2].b2[j];
				}
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

	// mutate weights - genetic algorithm
	void mutate(float rate) {
		// mutate weights1
		for (int i = 0; i < numShips; i++) {
			for (int j = 0; j < 5; j++) {
				for (int k = 0; k < 3; k++) {
					if (((float)rand() / (float)(RAND_MAX)) < rate) {
						population[i].w1[j][k] = ((float)rand() / (float)(RAND_MAX)) - 0.5;
					}
				}
			}
		}
		// mutate biases1
		for (int i = 0; i < numShips; i++) {
			for (int j = 0; j < 5; j++) {
				if (((float)rand() / (float)(RAND_MAX)) < rate) {
					population[i].b1[j] = ((float)rand() / (float)(RAND_MAX)) - 0.5;
				}
			}
		}
		// mutate weights2
		for (int i = 0; i < numShips; i++) {
			for (int j = 0; j < 2; j++) {
				for (int k = 0; k < 5; k++) {
					if (((float)rand() / (float)(RAND_MAX)) < rate) {
						population[i].w2[j][k] = ((float)rand() / (float)(RAND_MAX)) - 0.5;
					}
				}
			}
		}
		// mutate biases2
		for (int i = 0; i < numShips; i++) {
			for (int j = 0; j < 2; j++) {
				if (((float)rand() / (float)(RAND_MAX)) < rate) {
					population[i].b2[j] = ((float)rand() / (float)(RAND_MAX)) - 0.5;
				}
			}
		}
	}

	// workflow for train mode
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
				xil_printf("new generation");
				// calc population fitness
				populationFitness = 0;
				for (int i = 0; i < numShips; i++) {
					populationFitness += population[i].fitness;
				}
				// selection
				new_generation();
				// mutation
				mutate(mutationRate);
				currentShip = 0;
			} else {
				currentShip++;
			}
		}
		usleep(1);
		int x = parsed_input();
		if (x == 48) {
			training = 0;
			free(population);
			game_over_screen();
			lobby();
		}
	}

	/**************************************************************/

	// wait for selection of game mode
	void check_lobby() {
		int x = parsed_input();
		if (x == 49 || x == 50) {
			mode = (x - 48) * 2;
			playing = 1;
		} else if (x == 67) {
			mode = 10;
			training = 1;
			init_population();
		}
		usleep(1000);
	}

	// start in lobby
	lobby();

	// active game
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

int main(void) {
   init();
   run();
   cleanup();

   return 0;
}
