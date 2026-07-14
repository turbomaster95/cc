struct Payload {
    int id;
    int data[5];
};

struct Node {
    int value;
    struct Payload *payload;
    struct Node *next;
};

int calculate_sum(int a, int b, int c, int d);
struct Node* get_head(void);

int main(void) {
    int matrix[3][3][3];
    int i = 0;
    int j = 1;
    
    matrix[i++][j--][i] = 99;

    struct Node root;
    struct Node *ptr = &root;

    ptr->next->payload->data[2] = 42;
    
    root.payload->id = 7;

    int x = 10;
    
    int result = calculate_sum(x++, matrix[0][0][0], ptr->value, 500);

    get_head()->next->payload->data[calculate_sum(1, 2, 3, 4)] = 100;

    struct Payload dynamic_box;
    
    dynamic_box = (struct Payload){ .id = 101, .data = {1, 2, 3, 4, 5} };

    return result;
}
