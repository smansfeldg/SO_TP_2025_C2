#include <stdio.h>
#include "include/shared.h"

int main() {
    printf("Structure sizes:\n");
    printf("id_request_t: %zu bytes\n", sizeof(id_request_t));
    printf("record_t: %zu bytes\n", sizeof(record_t));
    printf("shared_memory_t: %zu bytes\n", sizeof(shared_memory_t));
    printf("MAX_DATA_SIZE: %d\n", MAX_DATA_SIZE);
    return 0;
}