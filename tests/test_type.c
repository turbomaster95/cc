typedef int custom_int;

typedef struct {
    int x;
    int y;
} Point;

enum Color {
    RED = 10,
    GREEN = 20,
    BLUE = 12
};

int main(void) {
    custom_int a = 5;
    custom_int b = 15;

    Point p;
    p.x = a;
    p.y = b;

    Point *ptr = &p;

    enum Color current_color = BLUE;

    return ptr->x + ptr->y + current_color; // 5 + 15 + 12 = 32
}

