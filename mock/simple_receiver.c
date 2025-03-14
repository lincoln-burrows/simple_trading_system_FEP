    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <envs.h>

    // Define `hdr` structure
    typedef struct {
        int tr_id;
        int length;
    } hdr;

    // Define `fkq_order` structure
typedef struct {
    hdr hdr;           // 4 bytes
    char stock_code[7];     // 7 bytes
    char padding1;          // 1 byte (패딩)
    char stock_name[51];    // 51 bytes
    char padding2;          // 1 byte (패딩)
    char transaction_code[7]; // 7 bytes
    char padding3;          // 1 byte (패딩)
    char user_id[21];       // 21 bytes
    char padding4[3];       // 3 bytes (패딩, 4의 배수 정렬)
    char order_type;        // 1 byte
    char padding5[3];       // 3 bytes (패딩)
    int quantity;           // 4 bytes
    char order_time[15];    // 15 bytes
    char padding6;          // 1 byte (패딩)
    int price;              // 4 bytes
    char original_order[7]; // 7 bytes
    char padding7;          // 1 byte (패딩)
} fkq_order;  // **총 136 bytes (패딩 포함)**

    // Define `kft_execution` structure
typedef struct {
    hdr hdr;
    char transaction_code[7]; // 거래코드
    char padding1;          // 1 byte (패딩)
    int status_code;          // 상태 코드 (0: 체결, 1: 취소, 99: 오류)
    char time[15];            // 응답시간 (YYYYMMDDHHMMSS)
    char padding2;          // 1 byte (패딩)
    int executed_price;       // 체결 가격
    char original_order[7];   // 원주문번호
    char padding3;          // 1 byte (패딩)
    char reject_code[7];      // 거부사유코드 (문자열)
    char padding4;          // 1 byte (패딩)
} kft_execution;

typedef struct {
    hdr hdr;
    char transaction_code[7]; // 거래코드
    char padding1;          // 1 byte (패딩)
    char user_id[21];         // 유저 ID
    char padding2[3];          // 1 byte (패딩) 
    char time[15];            // 응답시간 (YYYYMMDDHHMMSS)
    char padding3;          // 1 byte (패딩)
    char reject_code[7];      // 거부사유코드 (문자열)
    char padding4;          // 1 byte (패딩)
} fot_order_is_submitted;

    // Function to send `kft_execution` structure to the specified IP and port
    void send_kft_execution(const kft_execution *exec, int sock) {

        // Send the `kft_execution` structure
        ssize_t sent_bytes = send(sock, exec, sizeof(kft_execution), 0);
        if (sent_bytes < 0) {
            perror("Send failed");
        } else if (sent_bytes < sizeof(kft_execution)) {
            fprintf(stderr, "Partial data sent. Expected %lu bytes, sent %ld bytes.\n",
                    sizeof(kft_execution), sent_bytes);
        } else {
            printf("krx says transaction is executed\n");
        }

        // Close the socket
        // close(sock);
    }

    int main() {
        // socket for local port incoming

        int server_fd, client_fd;
        struct sockaddr_in server_addr, client_addr;
        socklen_t addr_len = sizeof(client_addr);

        // Create socket
        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
        }
    // 포트 재사용 옵션 추가
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        printf( "setsockopt failed");
        exit(EXIT_FAILURE);
    }


        // Set server address
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(FEP_OMS_R_PORT); 

        // Bind socket
        if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("Bind failed");
            close(server_fd);
            exit(EXIT_FAILURE);
        }

        // Listen for connections
        if (listen(server_fd, 1) < 0) {
            perror("Listen failed");
            close(server_fd);
            exit(EXIT_FAILURE);
        }

        // socket for sending response
        int sock;
        struct sockaddr_in fep_server_addr;

        // Create socket
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
        }

        // Set server address
        fep_server_addr.sin_family = AF_INET;
        fep_server_addr.sin_port = htons(FEP_KRX_R_PORT);
        if (inet_pton(AF_INET, FEP_IP, &fep_server_addr.sin_addr) <= 0) {
            perror("Invalid IP address or format");
            close(sock);
            exit(EXIT_FAILURE);
        }

        // Connect to the server
        if (connect(sock, (struct sockaddr *)&fep_server_addr, sizeof(fep_server_addr)) < 0) {
            perror("Connection to server failed");
            close(sock);
            exit(EXIT_FAILURE);
        }

        while (1) {
            printf("Waiting for connections...\n");

            // Accept a connection
            if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len)) < 0) {
                perror("Accept failed");
                close(server_fd);
                exit(EXIT_FAILURE);
            }

            printf("Client connected!\n");

            while(1){
                // Receive data
                fkq_order received_order;
                ssize_t bytes_received = recv(client_fd, &received_order, sizeof(received_order), 0);
                if (bytes_received < 0) {
                    perror("Receive failed");
                    close(client_fd);
                    close(server_fd);
                    // exit(EXIT_FAILURE);
                    break;
                } else if (bytes_received == 0) {
                    // Client disconnected
                    printf("Client disconnected.\n");
                    close(client_fd);
                    break; // Exit inner loop on disconnection
                }
          
                printf("krx get order\n");
                // Print received data
                printf("%d,%d,%s,%s,%s,%s,%c,%d,%s,%d,%s\n",
                    received_order.hdr.tr_id,
                    received_order.hdr.length,
                    received_order.stock_code,
                    received_order.stock_name,
                    received_order.transaction_code,
                    received_order.user_id,
                    received_order.order_type,
                    received_order.quantity,
                    received_order.order_time,
                    received_order.price,
                    received_order.original_order);

                // Prepare `fot_order_is_submitted` structure
                fot_order_is_submitted submit_result;            
                memset(&submit_result, 0, sizeof(fot_order_is_submitted)); // Initialize the struct
                submit_result.hdr.tr_id = 10;
                submit_result.hdr.length = sizeof(fot_order_is_submitted);
            
                strncpy(submit_result.transaction_code, received_order.transaction_code, sizeof(submit_result.transaction_code) - 1);
                submit_result.transaction_code[sizeof(submit_result.transaction_code) - 1] = '\0'; // Null-terminate
                
                strncpy(submit_result.user_id, received_order.user_id, sizeof(submit_result.user_id));
                submit_result.user_id[sizeof(submit_result.user_id) - 1] = '\0'; // Null-terminate
            
                strncpy(submit_result.time, "20250123134500", sizeof(submit_result.time) - 1);
                submit_result.time[sizeof(submit_result.time) - 1] = '\0'; // Null-terminate

                strncpy(submit_result.reject_code, "0000", sizeof(submit_result.reject_code) - 1); // No error
                submit_result.reject_code[sizeof(submit_result.reject_code) - 1] = '\0'; // Null-terminate

                // send order_submitted response

                ssize_t bytes_sent = send(client_fd, &submit_result, sizeof(fot_order_is_submitted), 0);
                    if (bytes_sent < 0) {
                        perror("Failed to send response");
                    } else if (bytes_sent < sizeof(submit_result)) {
                        fprintf(stderr, "Partial response sent. Expected %lu bytes, sent %ld bytes.\n",
                                sizeof(submit_result), bytes_sent);
                    } else {
                        printf("Response sent successfully to client. Sent %ld bytes.\n", bytes_sent);
                    }

                // Prepare `kft_execution` structure
                kft_execution exec_response = {
                    .hdr = {11, sizeof(kft_execution)},
                    .status_code = 0, // Success
                };
                strncpy(exec_response.transaction_code, received_order.transaction_code, sizeof(exec_response.transaction_code) - 1);
                strncpy(exec_response.time, "20250123134500", sizeof(exec_response.time) - 1);
                exec_response.executed_price = received_order.price; // Example
                strncpy(exec_response.original_order, received_order.original_order, sizeof(exec_response.original_order) - 1);
                strncpy(exec_response.reject_code, "E777", sizeof(exec_response.reject_code) - 1); // No error
                // Send `kft_execution` to the remote server
                send_kft_execution(&exec_response, sock);
                
            }
        }

        // Close sockets
        close(client_fd);
        close(server_fd);
        return 0;
    }

