#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <envs.h>

// Define the structures
typedef struct {
    int tr_id;
    int length;
} hdr;

typedef struct {
    hdr hdr;
    char stock_code[7];       // 종목코드
    char stock_name[51];      // 종목명 
    char transaction_code[7]; // 거래코드 
    char user_id[21];         // 유저 ID 
    char order_type;          // 매수(B) / 매도(S) (1바이트)
    int quantity;             // 수량 (정수형)
    char order_time[15];      // 주문시간 (YYYYMMDDHHMMSS)
    int price;                // 호가 (정수형)
    char original_order[7];   // 원주문번호 (문자열)
} fkq_order;

typedef struct {
    hdr hdr;
    char transaction_code[7]; // 거래코드 
    char user_id[21];         // 유저 ID 
    char time[15];            // 응답시간 (YYYYMMDDHHMMSS)
    char reject_code[7];      // 거부사유코드 (문자열)
} fot_order_is_submitted;

// Function to print the received structure
void print_fot_order_is_submitted(const fot_order_is_submitted *submit_result) {
    printf("Header:\n");
    printf("  Transaction ID: %d\n", submit_result->hdr.tr_id);
    printf("  Length: %d\n", submit_result->hdr.length);
    printf("Transaction Code: %s\n", submit_result->transaction_code);
    printf("User ID: %s\n", submit_result->user_id);
    printf("Response Time: %s\n", submit_result->time);
    printf("Reject Code: %s\n", submit_result->reject_code);
}

int main() {
    int sock;
    struct sockaddr_in server_addr;

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(FEP_OMS_R_PORT);
    if (inet_pton(AF_INET, FEP_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid IP address");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Prepare fkq_order data
    hdr header;
    header.length = sizeof(fkq_order);
    header.tr_id = 9;

    fkq_order order;
    order.hdr = header;
    strncpy(order.stock_code, "stock1", sizeof(order.stock_code));
    order.stock_code[sizeof(order.stock_code) - 1] = '\0'; // Null-terminate

    strncpy(order.stock_name, "netflix", sizeof(order.stock_name));
    order.stock_name[sizeof(order.stock_name) - 1] = '\0'; // Null-terminate

    strncpy(order.transaction_code, "tx0007", sizeof(order.transaction_code));
    order.transaction_code[sizeof(order.transaction_code) - 1] = '\0'; // Null-terminate

    strncpy(order.user_id, "testusr1", sizeof(order.user_id));
    order.user_id[sizeof(order.user_id) - 1] = '\0'; // Null-terminate

    order.order_type = 'S';
    order.quantity = 17;

    strncpy(order.order_time, "20250121094930", sizeof(order.order_time));
    order.order_time[sizeof(order.order_time) - 1] = '\0'; // Null-terminate

    order.price = 43000;

    strncpy(order.original_order, "tx99", sizeof(order.original_order));
    order.original_order[sizeof(order.original_order) - 1] = '\0'; // Null-terminate

    // Send the order to the server
    if (send(sock, &order, sizeof(order), 0) < 0) {
        perror("Send failed");
        close(sock);
        exit(EXIT_FAILURE);
    }
    printf("Order sent successfully.\n");

    // Receive the response from the server
    fot_order_is_submitted submit_result;
    ssize_t bytes_received = recv(sock, &submit_result, sizeof(submit_result), 0);

    if (bytes_received < 0) {
        perror("Error receiving data");
    } else if (bytes_received == 0) {
        printf("Connection closed by server.\n");
    } else if (bytes_received < sizeof(fot_order_is_submitted)) {
        fprintf(stderr, "Incomplete data received. Expected %lu bytes, got %ld bytes.\n",
                sizeof(fot_order_is_submitted), bytes_received);
    } else {
        printf("OMS get struct order is submitted :\n");
        print_fot_order_is_submitted(&submit_result);
    }

    // Close socket
    close(sock);
    printf("Socket closed.\n");

    return 0;
}

