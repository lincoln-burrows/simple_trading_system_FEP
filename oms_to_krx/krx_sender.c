#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h> // For open()
#include <errno.h>
#include <mqueue.h>
#include <oms_fep_krx_struct.h>
#include <envs.h>

// shared memory
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

//log
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

typedef struct {
    int rc;
} R_count;


#define QUEUE_NAME "/wc_queue"
#define SUBMIT_QUEUE_NAME "/submit_queue"
#define LOG_FILE_PATH "/home/ubuntu/logs/krx_sender.log"


// socket
#define MAX_CLIENTS 20
FILE *log_file = NULL;

// Initialize logging
void init_log() {
    mkdir("/home/ubuntu/logs", 0777);
    log_file = fopen(LOG_FILE_PATH, "a");
    if (!log_file) {
        perror("Failed to open log file");
        exit(EXIT_FAILURE);
    }
}

// Log function with level and module
void log_message(const char *level, const char *module, const char *format, ...) {
    if (!log_file) return;

    struct timeval tv;
    gettimeofday(&tv, NULL);  // Get current time with microseconds

    struct tm *tm_info = localtime(&tv.tv_sec);
    char time_buffer[25];

    // Format: YYYY-MM-DD HH:MM:SS.mmm
    snprintf(time_buffer, sizeof(time_buffer), "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
             tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec,
             tv.tv_usec / 1000);  // Convert microseconds to milliseconds

    fprintf(log_file, "[%s] [%s] [%s] ", time_buffer, level, module);

    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);

    fflush(log_file);
}

// Function to clean up the log file
void close_log() {
    if (log_file) {
        fflush(log_file);  // Ensure all data is written before closing
        fclose(log_file);
    }
}

int main() {

    init_log(); 

    mqd_t mq, submit_mq;
    struct mq_attr attr;
    struct mq_attr submit_attr = {0};
    submit_attr.mq_flags = 0;
    submit_attr.mq_maxmsg = 200;   // Maximum number of messages in the queue
    submit_attr.mq_msgsize = sizeof(fot_order_is_submitted); // Maximum size of each message in bytes
    submit_attr.mq_curmsgs = 0;   // Current number of messages in the queue
    // TCP 송신 함수
    void send_order_to_krx(fkq_order *order, int sock) {

        // 구조체 데이터 전송
        ssize_t sent_byte = send(sock, order, sizeof(fkq_order), 0);
        log_message("INFO", "tcp", "order sent successfully to krx\n");

    }

    void read_order_from_bin_file(FILE *file, int start, int end, R_count *r_count, int sock) {

        fkq_order order;
        memset(&order, 0, sizeof(order)); // Initialize the struct

        while(end > r_count->rc){
            if (fseek(file, sizeof(fkq_order) * r_count->rc, SEEK_SET) != 0) {
            log_message("ERROR", "file", "Failed to seek to line");
            fclose(file);
            exit(EXIT_FAILURE);
            }
            fread(&order, sizeof(fkq_order), 1, file);

            send_order_to_krx(&order, sock);
            r_count->rc++;
            log_message("INFO", "shm", "current value rc = %d\n", r_count->rc);
            
            log_message("INFO", "order", "%d,%d,%s,%s,%s,%s,%c,%d,%s,%d,%s\n",
                            order.hdr.tr_id,
                            order.hdr.length,
                            order.stock_code,
                            order.stock_name,
                            order.transaction_code,
                            order.user_id,
                            order.order_type,
                            order.quantity,
                            order.order_time,
                            order.price,
                            order.original_order);

        }   
    }

    // mmap memory code
    const char *shared_mem_name = "/R_count";
    const size_t shared_mem_size = sizeof(R_count);
    int is_initialized = 0; // Flag to track if shared memory is newly created

   // Create or open the shared memory object
    int shm_fd = shm_open(shared_mem_name, O_CREAT | O_RDWR | O_EXCL, 0666);
    log_message("DEBUG", "shm", "First shm_fd: %d\n", shm_fd);
    if (shm_fd == -1) {
        if (errno == EEXIST) {
            // Shared memory already exists
            shm_fd = shm_open(shared_mem_name, O_RDWR, 0666);
            log_message("DEBUG", "shm", "Second shm_fd: %d\n", shm_fd);
            if (shm_fd == -1) {
                log_message("ERROR", "shm", "shm_open failed");
                exit(EXIT_FAILURE);
            }
        } else {
            log_message("ERROR", "shm", "shm_open failed");
            exit(EXIT_FAILURE);
        }
    } else {
        // This is the first time the shared memory is being created
        is_initialized = 1;
    }

    // Set size of the shared memory object
    if (ftruncate(shm_fd, shared_mem_size) == -1) {
        log_message("ERROR", "shm", "ftruncate failed");
        close(shm_fd);
        shm_unlink(shared_mem_name);
        exit(EXIT_FAILURE);
    }


    // Map the shared memory object
    R_count *r_count = mmap(NULL, shared_mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (r_count == MAP_FAILED) {
        log_message("ERROR", "shm", "mmap failed");
        close(shm_fd);
        shm_unlink(shared_mem_name);
        exit(EXIT_FAILURE);
    }

    // Initialize shared memory if it is newly created
    if (is_initialized) {
        r_count->rc = 0; // Initialize write counter to 0
        log_message("DEBUG", "shm", "Shared memory initialized. rc = %d\n", r_count->rc);
        log_message("DEBUG", "shm", "Shared memory initialized. PID: %d\n", getpid());
    } else {
        log_message("DEBUG", "shm", "Shared memory already exists. rc = %d\n", r_count->rc);
    }

    // set file dir structure
    const char *home_dir = getenv("HOME");
    char filepath[256];
    if (home_dir != NULL) {
        snprintf(filepath, sizeof(filepath), "%s/received_data.txt", home_dir);
    } else {
        // Fallback to current directory if $HOME is not set
        strncpy(filepath, "./received_data.txt", sizeof(filepath));
    }
    FILE *file = fopen(filepath, "rb");
        if (file == NULL) {
            log_message("ERROR", "file", "Error opening file");
            return;
        }
    log_message("INFO", "file", "file is opened\n");

    // Open the message queue
    mq = mq_open(QUEUE_NAME, O_RDONLY);
    if (mq == -1) {
        log_message("ERROR", "mq", "mq_open failed");
        exit(1);
    }
    log_message("DEBUG", "mq", "message queue opened\n");

     // Open the sender queue
    submit_mq = mq_open(SUBMIT_QUEUE_NAME, O_CREAT | O_WRONLY, 0666, NULL, &submit_attr);
    if (submit_mq == -1) {
        log_message("ERROR", "mq", "submit mq_open failed");
        mq_close(mq);
        exit(EXIT_FAILURE);
    }
    log_message("DEBUG", "mq", "submit message queue opened.\n");

    // Get queue attributes
    if (mq_getattr(mq, &attr) == -1) {
        log_message("ERROR", "mq", "mq_getattr error");
        exit(1);
    }

    int received_wc; 

    // socket code
    int sock;
    struct sockaddr_in server_addr;

    // 소켓 생성
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        log_message("ERROR", "socket", "Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 서버 주소 설정
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(KRX_PORT);
    if (inet_pton(AF_INET, KRX_IP, &server_addr.sin_addr) <= 0) {
        log_message("ERROR", "socket","Invalid IP address or format");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // 서버 연결
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        log_message("ERROR", "socket","Connection to the server failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    while(1){
        // Receive the message
        ssize_t bytes_read = mq_receive(mq, (char *)&received_wc, attr.mq_msgsize, NULL);
        if (bytes_read == -1) {
            log_message("ERROR", "mq "," wc mq_receive failed");
            mq_close(mq);
            exit(1);
        }
        // Convert the byte array back to a long
        log_message("DEBUG", "mq", "Received: %d\n", received_wc);
        if(received_wc > r_count->rc){
            read_order_from_bin_file(file, r_count->rc, received_wc, r_count, sock);
        }

    }
    
    // // Close and unlink the message queue
    // mq_close(mq);
    // mq_unlink(QUEUE_NAME);

    return 0;
}

