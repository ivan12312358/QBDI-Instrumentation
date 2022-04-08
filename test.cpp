#include <cstdlib>

int* source(size_t size) { return new int[size]; }
void sink(int* pnt) {}

void test()
{
	const size_t size = 10;
    int* pnt = source(size);
    sink(pnt);

    int* p1 = pnt + 2;
    sink(p1);

    for (int i = 0; i < size; i++)
        sink(pnt + i);

    sink(pnt - 1);  // untraceable data
    sink(pnt + 10); // untraceable data

    delete[] pnt;
}
