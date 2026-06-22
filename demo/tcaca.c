#include <caca.h>

int main(int argc, char **argv)
{
	caca_canvas_t *cv;

	cv = caca_create_canvas(100, 100);

	caca_free_canvas(cv);

	return 0;
}


