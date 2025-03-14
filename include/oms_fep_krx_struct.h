#ifndef OMSFEPKRX_STRUCT_H
#define OMSFEPKRX_STRUCT_H

// 헤더 구성
typedef struct {
	int tr_id;
	int length;
} hdr;

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

typedef struct {
    hdr hdr;
    char transaction_code[7]; // 거래코드 
    char padding1;          // 1 byte (패딩)
    char user_id[21];         // 유저 ID 
    char padding2[3];       // 3 bytes (패딩, 4의 배수 정렬)
    char time[15];            // 응답시간 (YYYYMMDDHHMMSS)
    char padding3;          // 1 byte (패딩)
    char reject_code[7];      // 거부사유코드 (문자열)
    char padding4;          // 1 byte (패딩)
} kft_order;

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

typedef struct{
    hdr hdr;
    char stock_code[7];       // 종목코드
    char padding1;          // 1 byte (패딩)
    char stock_name[51];      // 종목명
    char padding2;          // 1 byte (패딩) 
    char transaction_code[7]; // 거래코드
    char padding3;          // 1 byte (패딩)
    char user_id[21];         // 유저 ID
    char padding4[3];          // 3 byte (패딩) 
    char order_type;          // 매수(B) / 매도(S) (1바이트)
    char padding5[3];          // 1 byte (패딩)
    int quantity;             // 수량 (정수형)
    char order_time[15];      // 주문시간 (YYYYMMDDHHMMSS)
    char padding6;          // 1 byte (패딩)
    int price;                // 호가 (정수형)
    char original_order[7];   // 원주문번호 (문자열)
    char padding7;          // 1 byte (패딩)
} ofq_order;

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



#endif //OMSFEPKRX_STRUCT_H
