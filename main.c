#include <LPC17xx.h>
#include <RTL.h>
#include "GLCD.h"
#include <stdio.h>

//***************************************** DEFINING VARIABLES **************************************************//

#define jmask 127
#define jup 9
#define jright 17
#define jleft 65
#define jdown 33
#define jidle 1
#define jselect 2
#define up 0
#define down 1
#define left 2
#define right 3
#define bodsize 10
#define lbord 10
#define rbord 310
#define dbord 230

//Important definitions such as Semaphors and volatile definitions.
volatile int floc[3];
volatile short GameOver = 0, Paused = 0;
const unsigned char ledPosArray[8] = { 28, 29, 31, 2, 3, 4, 5, 6 };
OS_SEM chSem, LEDSem;

//Color BitMap for hiding tail's color
unsigned short clearbmp[10*10] = {Black, Black, Black, Black, Black, Black, Black, Black, Black, Black, 
																	Black, Black, Black, Black, Black, Black, Black, Black, Black, Black, 
																	Black, Black, Black, Black, Black, Black, Black, Black, Black, Black,  
																	Black, Black, Black, Black, Black, Black, Black, Black, Black, Black,
																	Black, Black, Black, Black, Black, Black, Black, Black, Black, Black, 
																	Black, Black, Black, Black, Black, Black, Black, Black, Black, Black, 
																	Black, Black, Black, Black, Black, Black, Black, Black, Black, Black,
																	Black, Black, Black, Black, Black, Black, Black, Black, Black, Black,
																	Black, Black, Black, Black, Black, Black, Black, Black, Black, Black,
																	Black, Black, Black, Black, Black, Black, Black, Black, Black, Black};

//Color Bitmap for "Game Over" and Pause.
unsigned short redBackground[200] = {Red, Red, Red, Red, Red, Red, Red, Red, Red, Red, 
																				Red, Red, Red, Red, Red, Red, Red, Red, Red, Red, 
																				Red, Red, Red, Red, Red, Red, Red, Red, Red, Red, 
																				Red, Red, Red, Red, Red, Red, Red, Red, Red, Red, 
																				Red, Red, Red, Red, Red, Red, Red, Red, Red, Red, 
																				Red, Red, Red, Red, Red, Red, Red, Red, Red, Red, 
																				Red, Red, Red, Red, Red, Red, Red, Red, Red, Red, 
																				Red, Red, Red, Red, Red, Red, Red, Red, Red, Red, 
																				Red, Red, Red, Red, Red, Red, Red, Red, Red, Red, 
																					Red, Red, Red, Red, Red, Red, Red, Red, Red, Red, 
																					Red, Red, Red, Red, Red, Red, Red, Red, Red, Red, 
																					Red, Red, Red, Red, Red, Red, Red, Red, Red, Red, 
																					Red, Red, Red, Red, Red, Red, Red, Red, Red, Red, 
																					Red, Red, Red, Red, Red, Red, Red, Red, Red, Red, 
																					Red, Red, Red, Red, Red, Red, Red, Red, Red, Red, 
																					Red, Red, Red, Red, Red, Red, Red, Red, Red, Red, 
																					Red, Red, Red, Red, Red, Red, Red, Red, Red, Red, 
																					Red, Red, Red, Red, Red, Red, Red, Red, Red, Red, 
																					Red, Red, Red, Red, Red, Red, Red, Red, Red, Red, 
																				Red, Red, Red, Red, Red, Red, Red, Red, Red, Red};
unsigned short vertbord[280*1];

//Structs
typedef struct bodyStruct {
	unsigned int x;
	unsigned int y;
	unsigned short bmp[bodsize*bodsize];
	unsigned short currdir;
	unsigned short nextdir;
	struct bodyStruct* nextBlock;
} body;

typedef struct {
	body* head;
	body* tail;
	unsigned short len;
} snake;

typedef struct {
	unsigned int x;
	unsigned int y;
	unsigned short bmp[bodsize*bodsize];
} food ;

typedef struct {
	snake* snakey;
} params;

volatile snake* sillySnake;

//*************************************** DEFINING VARIABLES END *************************************************//

//**************************************** LED INITIALISATION  ***************************************************//

void LEDInit( void ) {

	// LPC_SC is a general system-control register block, and PCONP referes
	// to Power CONtrol for Peripherals.
	//  - Power/clock control bit for IOCON, GPIO, and GPIO interrupts (Section 4.8.9)
	//    This can also be enabled from `system_LPC17xx.c'
	LPC_SC->PCONP     |= (1 << 15);            

	// The ports connected to p1.28, p1.29, and p1.31 are in mode 00 which
	// is functioning as GPIO (Section 8.5.5)
	LPC_PINCON->PINSEL3 &= ~(0xCF00);

	// The port connected to p2.2, p2.3, p2.4, p2.5, and p2.6 are in mode 00
	// which is functioning as GPIO (Section 8.5.5)
	LPC_PINCON->PINSEL4 &= (0xC00F);

	// LPC_GPIOx is the general control register for port x (Section 9.5)
	// FIODIR is Fast GPIO Port Direction control register. This register 
	// individually controls the direction of each port pin (Section 9.5)
	//
	// Set the LEDs connected to p1.28, p1.29, and p1.31 as output
	LPC_GPIO1->FIODIR |= 0xB0000000;           

	// Set the LEDs connected to p2.2, p2.3, p2.4, p2.5, and p2.6 as output port
	LPC_GPIO2->FIODIR |= 0x0000007C;           
}

// Turn on the LED inn a position within 0..7
void turnOnLED( unsigned char led ) {
	unsigned int mask = (1 << ledPosArray[led]);

	// The first two LEDs are connedted to the port 28, 29 and 30
	if ( led < 3 ) {
		// Fast Port Output Set register controls the state of output pins.
		// Writing 1s produces highs at the corresponding port pins. Writing 0s has no effect (Section 9.5)
		LPC_GPIO1->FIOSET |= mask;
	} else {
		LPC_GPIO2->FIOSET |= mask;
	}

}

// Turn off the LED in the position within 0..7
void turnOffLED( unsigned char led ) {
	unsigned int mask = (1 << ledPosArray[led]);

	// The first two LEDs are connedted to the port 28, 29 and 30
	if ( led < 3 ) {
		// Fast Port Output Clear register controls the state of output pins. 
		// Writing 1s produces lows at the corresponding port pins (Section 9.5)
		LPC_GPIO1->FIOCLR |= mask;
	} else {
		LPC_GPIO2->FIOCLR |= mask;
	}
}


//******************************************* LED END **************************************************//

//************************************* INT0 INITIALISATION ********************************************//

void INT0Init( void ) {

	// P2.10 is related to the INT0 or the push button.
	// P2.10 is selected for the GPIO 
	LPC_PINCON->PINSEL4 &= ~(3<<20); 

	// P2.10 is an input port
	LPC_GPIO2->FIODIR   &= ~(1<<10); 

	// P2.10 reads the falling edges to generate the IRQ
	// - falling edge of P2.10
	LPC_GPIOINT->IO2IntEnF |= (1 << 10);

	// IRQ is enabled in NVIC. The name is reserved and defined in `startup_LPC17xx.s'.
	// The name is used to implemet the interrupt handler above,
	NVIC_EnableIRQ( EINT3_IRQn );

}

//***************************************** INT0 END **************************************************//

//********************************** JOYSTICK INITIALISATION ******************************************//

void joystickInit(void) {
	LPC_PINCON->PINSEL3 &= ~(255 << 14);
	LPC_GPIO1->FIODIR &= ~(15 << 23);
}

uint32_t joystickGet() {
	uint32_t val;
	
	val = (LPC_GPIO1->FIOPIN >> 20) & jmask;
	val = (-val & jmask);
		
	return val;
}

//*************************************** JOYSTICK END ************************************************//

//**************************************** FUNCTIONS **************************************************//

//Attaches another body part on the Snake's Tail
void MakeBody ( unsigned short color ) {
	int i, w = bodsize*bodsize;
	body* bodyptr = (body*) malloc(sizeof(body));	
	bodyptr->nextdir = sillySnake->tail->currdir;
	bodyptr->currdir = bodyptr->nextdir;
	bodyptr->nextBlock = 0;	

	//Attaches new tail to position behind tail relative to tail's movement
	if ( bodyptr->currdir == up ) {
		bodyptr->x = sillySnake->tail->x;
		bodyptr->y = sillySnake->tail->y+bodsize;
	} else if ( bodyptr->currdir == down ) {
		bodyptr->x = sillySnake->tail->x;
		bodyptr->y = sillySnake->tail->y-bodsize;
	} else if ( bodyptr->currdir == left ) {
		bodyptr->x = sillySnake->tail->x+bodsize;
		bodyptr->y = sillySnake->tail->y;
	}	else {
		bodyptr->x = sillySnake->tail->x-bodsize;
		bodyptr->y = sillySnake->tail->y;
	}
	
	//Changes all the bitmap indices to the color
	for ( i = 0; i<w; i++ ) {
		bodyptr->bmp[i] = color;
	}
	
	sillySnake->tail->nextBlock = bodyptr;
	sillySnake->tail = bodyptr;
	sillySnake->len++;
}

//Makes the head of the snake for initialisation
void MakeHead ( unsigned short color ) {
	int i, w = bodsize*bodsize;
	body* bodyptr = (body*) malloc(sizeof(body));
	bodyptr->x = 160;
	bodyptr->y = 120;
	bodyptr->nextBlock = 0;	
	bodyptr->nextdir = up;
	bodyptr->currdir = up;
	for(i = 0; i<w; i++)
	{
		bodyptr->bmp[i] = color;
	}
	sillySnake->head = bodyptr;
	sillySnake->tail = bodyptr;
	sillySnake->len++;
}

//Assigns values from FoodSpawn() to food struct
food* MakeFood ( unsigned int x, unsigned int y, unsigned int color ) {
	int i, size = (10*10);
	
	food* foodVariable = (food*) malloc(sizeof(body));
	foodVariable->x = x;
	foodVariable->y = y;
	
	//Assigns the random color onto Food's Bitmap
	for ( i = 0; i<size; i++ ) {
		foodVariable->bmp[i] = color;
	}
	
	return foodVariable;
}

//Generates the random coordinates for food spawn
food* FoodSpawn() {
	int x, y, color;
	int prevColor;
	food *food1;
	
	//Creates random coordinates inside display screen coordinates
	x = (rand() % 28 + 2)*10;
	y = (rand() % 20 + 2)*10;
	
	//Changes color based on Yellow->Green->Blue pattern
	prevColor = sillySnake->tail->bmp[1];
	
	if (prevColor == Green){
		color = Blue;
	} else if (prevColor == Blue){
		color = Yellow;
	} else {
		color = Green;
	}
	
	food1 = MakeFood(x, y, color);
	GLCD_Bitmap(food1->x, food1->y, 10,10, (unsigned char*)food1->bmp);
	return food1;
}

//Puts Food's information into an array to compare with Snake's location to add to it's tail
short FoodMaker() {
	food* fptr = FoodSpawn();
	short color = fptr->bmp[1];
	
	//Assigns information into array
	floc[0] = fptr->x;
	floc[1] = fptr->y;
	floc[2] = color;
	free(fptr);
	return color;
}

//Creates Border lines
void drawBords() {
	int i = 0;
	
	//Creates White lines
	for ( i = 0; i<300; i++ ) {
		vertbord[i] = White;
	}
	
	//Displays the border onto the display
	GLCD_Bitmap(10, 10, 300, 1, (unsigned char*) vertbord);
	GLCD_Bitmap(10, 230, 300, 1, (unsigned char*) vertbord);
	GLCD_Bitmap(10, 10, 1, 220, (unsigned char*) vertbord);
	GLCD_Bitmap(310, 10, 1, 220, (unsigned char*) vertbord);
}

//************************************** FUNCTIONS END ************************************************//

//*************************************** INTERRUPTS **************************************************//

// INT0 interrupt handler, for Pause Menu
void EINT3_IRQHandler( void ) {
	int jval, i, isContinue;
	short bmp[100];
	
	// Check whether the interrupt is called on the falling edge. GPIO Interrupt Status for Falling edge.
	if ( LPC_GPIOINT->IO2IntStatF && (0x01 << 10) ) {
		LPC_GPIOINT->IO2IntClr |= (1 << 10); // clear interrupt condition 
		Paused = 1;
		
		//Creates background for Pause Menu
		for ( i = 0; i<140; i++ ) {
			GLCD_Bitmap(55, 50+i, 200, 1, (unsigned char*)redBackground);
		}
		//Displays Pause Menu information onto the screen
		GLCD_SetBackColor(Red);
		GLCD_DisplayString(12, 7, 1, "PAUSE");
		GLCD_DisplayString(7, 10, 0, "Help: Move JoyStick to control");
		GLCD_DisplayString(8, 10, 0, "the snake and eat the food");
		GLCD_DisplayString(10, 10, 0, "-Press select to proceed");
		GLCD_DisplayString(12, 10, 0, "Cool Facts: Snake Changes color");
		GLCD_DisplayString(13, 10, 0, "According to the food it eats");
		GLCD_DisplayString(17, 5, 1, "> Restart");
		GLCD_SetBackColor(Blue);
		GLCD_DisplayString(16, 5, 1, "> Continue");
		GLCD_SetBackColor(Black);
		isContinue = 1;
		
		//Checks for JoyStick movements
		while(1) {
			jval = joystickGet();
			
			//Scrolls to Continue when Joystick moves up
			if (jval == jup){
				GLCD_SetBackColor(Blue);
				GLCD_DisplayString(16, 5, 1, "> Continue");
				GLCD_SetBackColor(Red);
				GLCD_DisplayString(17, 5, 1, "> Restart");
				GLCD_SetBackColor(Black);
				isContinue = 1;
			//Scrolls to Restart when Joystick moves down
			} else if (jval == jdown){
				GLCD_SetBackColor(Blue);
				GLCD_DisplayString(17, 5, 1, "> Restart");
				GLCD_SetBackColor(Red);
				GLCD_DisplayString(16, 5, 1, "> Continue");
				GLCD_SetBackColor(Black);
				isContinue = 0;
			}
			
			//If JoyStick is clicked (select) on Continue, then return to game
			if (jval == jselect && isContinue == 1){
				break;
				
			//Otherwise reset menu pops up and reset will need to be pressed
			} else if (jval == jselect && isContinue == 0){
				
				GLCD_SetBackColor(White);
				GLCD_SetTextColor(Red);
				GLCD_DisplayString(13, 3, 1, "             ");
				GLCD_DisplayString(14, 3, 1, " PRESS RESET ");
				GLCD_DisplayString(15, 3, 1, "   TO RESET  ");
				GLCD_DisplayString(16, 3, 1, "             ");
				GLCD_SetBackColor(Red);
				GLCD_SetTextColor(Yellow);
			}
		}
		//Clears the Pause menu and goes back to the Game
		GLCD_Clear(Black);
		drawBords();
		for ( i = 0; i < 100; i++ ) {
			bmp[i] = floc[2];
		}
		
		GLCD_Bitmap(floc[0], floc[1], 10, 10, (unsigned char*) bmp);
		Paused = 0;
	}
}

//*************************************** INTERRUPT END ***********************************************//

//******************************************* TASKS ***************************************************//

//Control LED's according to Player's Score
__task void UpdateLEDS() {
	//Go through snake->len and make bitmap of active LEDs
	unsigned char bitmap;
	short i;
	bitmap = (sillySnake->len)-3;
	
	//Turns off all LED and only turns ON the ones the ones 
	//According to food pieces consumed
	for ( i = 0; i < 8; i-- ) {
		turnOffLED(i);
		if ( bitmap & ( 1<<i ) ) {
			turnOnLED(i);
		}
	}
	os_tsk_delete_self();
}

//Check head's location for Game Over conditions
__task void checkHead() {
	while(1) {		
		body* head = sillySnake->head;
		body* currptr= head->nextBlock;
		//Pauses Task while game is Paused
		while( Paused ) {}
		
		os_sem_wait(&chSem, 0xFFFF);
			
		//If the head is moving up or down and touches border, then Game Over
		if ( head->currdir == up || head->currdir == down ) {
			if ( head->y == 10 || head->y == 230 ) {
				GameOver = 1;
				os_tsk_delete_self();
			} 
		}
		//If head is moving left or right and touches border, then Game Over
		if ( head->currdir == left || head->currdir == right ) {
			if ( head->x == 10 || head->x == 310 ) {
				GameOver = 1; 
				os_tsk_delete_self();
			} 
		}
		//If head touches it's own body
		while(currptr != 0) {
			if ( ( currptr->x+3 <= head->x+3 && currptr->x >= head->x-3 ) && ( currptr->y <= head->y+3 && currptr->y >= head->y-3 ) ) {
				GameOver = 1;
				os_tsk_delete_self();
			}
			currptr = currptr->nextBlock;
		}
	}
}

//Moves by creating block to new location and clearing old location
__inline void Move(body* movething, unsigned short dir) {
	unsigned int tempx = movething->x;
	unsigned int tempy = movething->y;
	
	if    ( dir == down ) { movething->y += bodsize; }
	else if ( dir == up ) {	movething->y -= bodsize; }
	else if (dir == right){	movething->x += bodsize; }
	else                  {	movething->x -= bodsize; }

	GLCD_Bitmap(movething->x, movething->y, bodsize,bodsize, (unsigned char*)movething->bmp);
	GLCD_Bitmap(tempx, tempy, bodsize,bodsize, (unsigned char*)clearbmp);
}

//Moves the snake according to JoyStick output or the previous JoyStick output
__task void MoveSnake() {
	uint32_t jvalue=0;
	body* currptr = sillySnake->head;
	short color = FoodMaker();
	
	os_tsk_create(checkHead, 1);
	
	//Moves snake while game is still running
	while ( !GameOver ) {
		currptr = sillySnake->head;
		do {
			//Stops the snake move while game is paused
			while (Paused) {}
			
			jvalue = joystickGet();
			
			//If there is no JoyStick input, then move Snake previous JoyStick value
			if ( jvalue != jidle ) {
				if ( jvalue == jup ) {
					if ( sillySnake->head->currdir==up || sillySnake->head->currdir==down )	{continue;}
					sillySnake->head->nextdir = up;
				}	else if ( jvalue == jdown )	{
					if ( sillySnake->head->currdir==up || sillySnake->head->currdir==down )	{continue;}
					sillySnake->head->nextdir = down;
				} else if ( jvalue == jleft ) {
					if ( sillySnake->head->currdir==left || sillySnake->head->currdir==right ) {continue;}
					sillySnake->head->nextdir = left;
				} else if ( jvalue == jright )	{
					if ( sillySnake->head->currdir==left || sillySnake->head->currdir==right ) {continue;}  
					sillySnake->head->nextdir = right;
				}
			}
			
			//Function will move head, insert a new body piece and then clear the previous frame's tail
			
			//Creates a new body piece after after moving head
			if ( ( sillySnake->head->x+5 >= floc[0] &&  sillySnake->head->x-5 <= floc[0]) && (sillySnake->head->y+5 >= floc[1] &&  sillySnake->head->y-5 <= floc[1] ) )	{
				GLCD_Bitmap(floc[0], floc[1], 10,10, (unsigned char*)clearbmp);
				MakeBody(color);
				color = FoodMaker();
				os_tsk_create(UpdateLEDS, 1);
			}
			
			//Inserts the new body piece according to previous body piece's movement
			if ( currptr->currdir == up ) {
				Move(currptr, up);
			}	else if ( currptr->currdir == down ) {
				Move(currptr, down);
			} else if ( currptr->currdir == left ) {
				Move(currptr, left);
			} else if ( currptr->currdir == right ) {
				Move(currptr, right);
			}
			
			//Moves the next body piece's direction to the current one
			if ( currptr->nextBlock != 0 ) {
				currptr->nextBlock->nextdir = currptr->currdir;
			}
			
			os_sem_send(&chSem);
			currptr->currdir = currptr->nextdir;
			currptr = currptr->nextBlock;
			
			//Controls moving speed as more body slows down the snake
			if ( sillySnake->len < 40  ) {
				if ( sillySnake-> len < 3 ) {os_dly_wait(4);}
				else if ( sillySnake-> len < 8 ) {os_dly_wait(3);}
				else if ( sillySnake->len < 13 ) {os_dly_wait(2);}
				else {os_dly_wait(1);}
			}
			
		} while ( currptr != 0 ) ;
	}
	os_tsk_delete_self();
}

//Task which initializes Game
__task void init_task ( void ) {
	int i;
	sillySnake = (snake*) malloc(sizeof(snake));
	
	//Creates Border
	drawBords();
	
	//Creates Head and attaches two body pieces to it.
	MakeHead(Red);
	MakeBody(Yellow);
	MakeBody(Green);
	
	//Initializes Semaphors and Task to move snake
	os_sem_init(&chSem, 0);
	os_sem_init(&LEDSem, 1);
	
	os_tsk_create(MoveSnake, 2);
	
	//Keep task running until Player loses the game
	while(!GameOver){}
	
	//Creates a Game Over display
	GLCD_Clear(Black);
	for ( i = 0; i<50; i++ ) {
		GLCD_Bitmap(55, 90+i, 200, 1, (unsigned char*)redBackground);
	}
	GLCD_SetBackColor(Red);
	GLCD_DisplayString(15, 5, 1, "GAME OVER");
	GLCD_SetBackColor(Black);
	os_tsk_delete_self();
}																																

//***************************************** TASK END **************************************************//

//******************************************* MAIN ****************************************************//

int main( void ) {
	
	//Initializes the system
	SystemInit();
	
	//Initializes Display Screen
	GLCD_Init();
 	GLCD_Clear(Black);
 	GLCD_SetBackColor (Black);
	GLCD_SetTextColor(Yellow);
	
	//Initializes LED, Push BUtton and JoyStick
 	LEDInit();
 	INT0Init();
 	joystickInit();
	
	//Begins the Game after initialization is completed
	os_sys_init(init_task);
	
}

//***************************************** MAIN END **************************************************//
