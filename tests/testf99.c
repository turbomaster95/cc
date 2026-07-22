struct Point {
    int x;
    int y;
};

struct Rectangle {
    struct Point top_left;
    struct Point bottom_right;
    int color_code;
};

inline int compute_area(int w, int h) {
    int area = w * h;
    return area;
}

int process_coords(void) {
    int total = 0;
    for (int i = 0; i < 5; i++) {
        total += i;
    }

    struct Rectangle rect = {
        .top_left = { .x = 0, .y = 0 },
        .bottom_right = { .x = 10, .y = 20 },
        .color_code = 0xFF
    };

    int lookup_table[5] = { [0] = 10, [2] = 30, [4] = 50 };

    struct Point dynamic_pt = (struct Point){ .x = 15, .y = 25 };

    struct Point polygon[2] = {
        [0] = { .x = 1, .y = 2 },
        [1] = { .x = 3, .y = 4 }
    };

    int width = rect.bottom_right.x - rect.top_left.x;
    int height = rect.bottom_right.y - rect.top_left.y;
    return compute_area(width, height) + total + lookup_table[2] + dynamic_pt.x + polygon[1].y;
}

int main(void) {
    int result = process_coords();

    int coords[] = {
        1,
        2,
        3,
    };

    return result + coords[0];
}
