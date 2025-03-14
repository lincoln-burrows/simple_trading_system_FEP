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
#include <mysql/mysql.h>

// shared memory
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

//log
#include <stdarg.h>
#include <time.h>

typedef struct {
    int rc;
} KRX_R_count;

#define QUEUE_NAME "/execution_wc_queue"

#define LOG_FILE_PATH "/home/ubuntu/logs/db_updator.log"


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

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_buffer[20];

    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    fprintf(log_file, "[%s] [%s] [%s] ", time_buffer, level, module);

    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    fflush(log_file);
}

void print_kft_execution(const kft_execution *execution) {
    log_message("INFO", "execution", "krx_execution:\n");
    log_message("INFO", "execution","  Header:\n");
    log_message("INFO", "execution", "    tr_id: %d\n", execution->hdr.tr_id);
    log_message("INFO", "execution","    length: %d\n", execution->hdr.length);
    log_message("INFO", "execution","  transaction_code: %s\n", execution->transaction_code);
    log_message("INFO", "execution","  status_code: %d\n", execution->status_code);
    log_message("INFO", "execution","  time: %s\n", execution->time);
    log_message("INFO", "execution","  executed_price: %d\n", execution->executed_price);
    log_message("INFO", "execution","  original_order: %s\n", execution->original_order);
    log_message("INFO", "execution","  reject_code: %s\n", execution->reject_code);
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

    // mysql connection
    MYSQL *conn;
    MYSQL_RES *res;
    MYSQL_ROW row;
    
    // MySQL 초기화
    conn = mysql_init(NULL);
    if (conn == NULL) {
        log_message("ERROR", "db", "mysql_init() failed\n");
        return EXIT_FAILURE;
    }
    // MySQL connection options
    // mysql_options(conn, MYSQL_OPT_SSL_MODE, "DISABLED"); // Disable SSL if not needed
    // mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, (const void *)5); // Set timeout

    // 데이터베이스 연결
    if (mysql_real_connect(conn, MYSQL_IP, MYSQL_USER, MYSQL_PW, MYSQL_DBNAME, 0, NULL, 0) == NULL) {
        log_message("ERROR", "db", "mysql_real_connect() failed: %s\n", mysql_error(conn));
        mysql_close(conn);
        return EXIT_FAILURE;
    }


    mqd_t mq;
    struct mq_attr attr;


    void read_exec_from_bin_file(FILE *file, int start, int end, KRX_R_count *r_count) {

        kft_execution execution;
        memset(&execution, 0, sizeof(execution)); // Initialize the struct

        while(end > r_count->rc){
            if (fseek(file, sizeof(kft_execution) * r_count->rc, SEEK_SET) != 0) {
            log_message("ERROR", "file", "Failed to seek to line");
            fclose(file);
            exit(EXIT_FAILURE);
            }
            fread(&execution, sizeof(kft_execution), 1, file);
            print_kft_execution(&execution);
            
            char query[512] = {0};
            char status;
            char truncated_reject_code[5];  // 4 bytes + 1 for null terminator
            strncpy(truncated_reject_code, execution.reject_code, 4);
            truncated_reject_code[4] = '\0';  // Ensure null termination
            
            if (execution.status_code == 0) {
                // db update
                // Create query string with transaction_code safely    
                // snprintf(query, sizeof(query), "UPDATE tx_history SET status = 'D' WHERE transaction_code = '%s'", execution.transaction_code);
                snprintf(query, sizeof(query),
                "UPDATE tx_history SET status = 'D', reject_code = '%s' WHERE transaction_code = '%s'",
                truncated_reject_code, execution.transaction_code);
                status = 'D';
            } else if (execution.status_code == 1){
                // snprintf(query, sizeof(query), "UPDATE tx_history SET status = 'C' WHERE transaction_code = '%s'", execution.transaction_code);
                snprintf(query, sizeof(query),
                "UPDATE tx_history SET status = 'C', reject_code = '%s' WHERE transaction_code = '%s'",
                truncated_reject_code, execution.transaction_code);
                status = 'C';
            } else if (execution.status_code == 99){
                // snprintf(query, sizeof(query), "UPDATE tx_history SET status = 'R' WHERE transaction_code = '%s'", execution.transaction_code);
                snprintf(query, sizeof(query),
                "UPDATE tx_history SET status = 'R', reject_code = '%s' WHERE transaction_code = '%s'",
                truncated_reject_code, execution.transaction_code);
                status = 'R';
            }

            // Execute the update query
            if (mysql_query(conn, query)) {
                fprintf(stderr, "UPDATE query failed: %s\n", mysql_error(conn));
                mysql_close(conn);
                // return EXIT_FAILURE;
            }
            log_message("INFO", "db","update status to %c executed successfully!\n", status);

            r_count->rc++;
            log_message("INFO", "shm", "current exec rc = %d\n", r_count->rc);
        }   
    }

    // mmap memory code
    const char *shared_mem_name = "/KRX_R_count";
    const size_t shared_mem_size = sizeof(KRX_R_count);
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
    KRX_R_count *r_count = mmap(NULL, shared_mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
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
        snprintf(filepath, sizeof(filepath), "%s/krx_received_data.txt", home_dir);
    } else {
        // Fallback to current directory if $HOME is not set
        strncpy(filepath, "./krx_received_data.txt", sizeof(filepath));
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

    int received_wc; 

    while(1){
        // Receive the message
        ssize_t bytes_read = mq_receive(mq, (char *)&received_wc, attr.mq_msgsize, NULL);
        if (bytes_read == -1) {
            log_message("ERROR", "mq "," wc mq_receive failed");
            mq_close(mq);
            exit(1);
        }
        // Convert the byte array back to a long
        log_message("DEBUG", "mq", "Received execution wc: %d\n", received_wc);
        if(received_wc > r_count->rc){
            read_exec_from_bin_file(file, r_count->rc, received_wc, r_count);
        }
    }
    
    // // Close and unlink the message queue
    // mq_close(mq);
    // mq_unlink(QUEUE_NAME);

    return 0;
}

