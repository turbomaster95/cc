struct Point {
    int x;
    int y;
};

int compute_area(int w, int h) {
    int area;
    area = w * h;
    return area;
}

int main(void) {
    int total;
    int i;
    struct Point pt;

    total = 0;
    i = 0;
    while (i < 5) {
        total = total + i;
        i = i + 1;
    }

    pt.x = 15;
    pt.y = 25;

    return compute_area(10, 20) + total + pt.x;
}
