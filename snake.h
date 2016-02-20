
typedef struct {
	unsigned int x;
	unsigned int y;
	unsigned short* bmp;
	body* nextBlock;
} body;

typedef struct {
	body* head;
	body* tail;
} snake;