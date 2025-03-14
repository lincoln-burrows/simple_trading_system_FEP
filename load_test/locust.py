import socket
import struct
import time
import pytz
from datetime import datetime
from locust import constant
from locust import User, TaskSet, task, events

# Function to get the current time in KST (Korea Standard Time)
def get_kst_time():
    kst = pytz.timezone('Asia/Seoul')
    return datetime.now(kst).strftime("%Y%m%d%H%M%S")  # Format: YYYYMMDDHHMMSS

class TCPClient:
    def __init__(self, host, port, environment):
        self.host = host
        self.port = port
        self.environment = environment
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            self.sock.connect((self.host, self.port))
            print("Connection established.")
        except socket.error as e:
            print(f"Failed to connect to {self.host}:{self.port}. Error: {e}")
            self.sock = None

    def send_order(self, order_data):
        if self.sock:
            start_time = time.time()
            try:
                self.sock.sendall(order_data)
                response = self.sock.recv(1024)  # Adjust size as necessary
                total_time = int((time.time() - start_time) * 1000)  # time in milliseconds
                self.environment.events.request.fire(
                    request_type="tcp",
                    name="send_order",
                    response_time=total_time,
                    response_length=len(response),
                    exception=None,
                    context={}
                )
                return response
            except socket.error as e:
                total_time = int((time.time() - start_time) * 1000)
                self.environment.events.request.fire(
                    request_type="tcp",
                    name="send_order",
                    response_time=total_time,
                    response_length=0,
                    exception=e,
                    context={}
                )
                return None
        else:
            print("Connection not established.")
            return None

    def close(self):
        if self.sock:
            self.sock.close()
            print("Socket closed.")

def pack_order(tr_id, user_id):
    hdr = struct.pack('=ii', 9, 136)  # transaction id and length of the entire message
    current_time_kst = get_kst_time().encode()  # Get current KST time in YYYYMMDDHHMMSS format

    order = struct.pack(
        '=7sB51sB7sB21s3sB3si15sBi7sB',
        '005930'.encode(),  # Stock code
        0,                  # Padding1
        '삼성전자_test'.ljust(50).encode(),  # Stock name
        0,                  # Padding2
        f'{800001 + tr_id:06}'.encode(),  # Transaction code
        0,                  # Padding3
        user_id.ljust(20).encode(),  # User ID
        b'\x00\x00\x00',    # Padding4
        ord('B'),           # Order type
        b'\x00\x00\x00',    # Padding5
        100,                # Quantity
        current_time_kst,   # **Dynamic Order Time in KST**
        0,                  # Padding6
        50000,               # Price
        'NONE'.encode(),    # Original order
        0                   # Padding7
    )
    return hdr + order

def unpack_response(data):
    fmt = '=ii7sB21s3s15sB7sB'
    try:
        unpacked_data = struct.unpack(fmt, data)
        return {
            "transaction_id": unpacked_data[0],
            "length": unpacked_data[1],
            "transaction_code": unpacked_data[2].decode().strip('\x00'),
            "user_id": unpacked_data[4].decode().strip('\x00'),
            "time": unpacked_data[6].decode().strip('\x00'),
            "reject_code": unpacked_data[8].decode().strip('\x00'),
        }
    except struct.error as e:
        print(f"Error unpacking data: {e}")
        return None

class TCPUser(User):
    wait_time = constant(0)

    def on_start(self):
        self.client = TCPClient('IP_ADDRESS', 8080, self.environment)
        self.tr_id = 0  # Initialize transaction ID counter
        self.request_count = 0  # Counter to track the number of sent requests

    def on_stop(self):
        self.client.close()

class LoadTestTasks(TaskSet):
    @task
    def send_binary_data(self):
        if self.user.request_count >= 1000:  # Stop after 1000 requests
            self.interrupt(reschedule=False)  # Stop this TaskSet and remove the user

        packed_order = pack_order(self.user.tr_id, 'UserID123')
        response = self.client.send_order(packed_order)
        if response:
            unpacked_response = unpack_response(response)
            print('Response:', unpacked_response)

        self.user.tr_id += 1
        self.user.request_count += 1  # Increment the counter

class MyLocust(TCPUser):
    tasks = [LoadTestTasks]