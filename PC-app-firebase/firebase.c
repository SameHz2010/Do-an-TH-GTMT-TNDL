#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <conio.h>
#include <time.h>

// ================= CẤU HÌNH =================
const char *PORT_NAME = "\\\\.\\COM5";
const int BAUD_RATE = 115200;
const char *FIREBASE_URL = "https://sensor-dht20-default-rtdb.firebaseio.com/sensor_data.json";

// Biến này không để static nữa để có thể thay đổi trong main
int sampling_period = 1000;
// ============================================

HANDLE hSerial;
static int current_stt = 0;
static DWORD last_save_time = 0;

// --- HÀM: LẤY GIỜ HIỆN TẠI ---
void getCurrentTime(char *buffer)
{
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, 80, "%d/%m/%Y %H:%M:%S", timeinfo);
}

// --- HÀM: GỬI DỮ LIỆU LÊN FIREBASE ---
void uploadToFirebase(int stt, float temp, float hum, char *timeStr)
{
    char cmd[2048];
    sprintf(cmd, "curl -X POST -d \"{\\\"STT\\\":%d, \\\"Temp\\\":%.2f, \\\"Hum\\\":%.2f, \\\"Time\\\":\\\"%s\\\"}\" %s > nul 2>&1",
            stt, temp, hum, timeStr, FIREBASE_URL);

    system(cmd);
    printf("   -> [CLOUD] Da dong bo len Firebase (Chu ky: %dms).\n", sampling_period);
}

// --- HÀM: KHỞI TẠO CỔNG COM ---
int initSerial(const char *port, int baud)
{
    hSerial = CreateFile(port, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (hSerial == INVALID_HANDLE_VALUE)
        return 0;

    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);
    GetCommState(hSerial, &dcb);
    dcb.BaudRate = baud;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    SetCommState(hSerial, &dcb);

    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    SetCommTimeouts(hSerial, &timeouts);
    return 1;
}

// --- HÀM: XỬ LÝ DỮ LIỆU ---
void processData(char *raw_data)
{
    DWORD current_time = GetTickCount();

    // Kiểm tra chu kỳ dựa trên biến sampling_period động
    if (current_time - last_save_time < sampling_period)
        return;

    char *pStart = strstr(raw_data, "Humidity:");
    if (pStart == NULL)
        return;

    float temp = 0.0;
    float hum = 0.0;
    int parsed = sscanf(pStart, "Humidity: %f%%, Temperature: %f C", &hum, &temp);

    if (parsed == 2)
    {
        current_stt++;
        char timeStr[80];
        getCurrentTime(timeStr);

        uploadToFirebase(current_stt, temp, hum, timeStr);

        last_save_time = current_time;
    }
}

int main()
{
    printf("--- HE THONG GIAM SAT IOT (CHINH CHU KY TIME) ---\n");

    if (initSerial(PORT_NAME, BAUD_RATE))
    {
        printf("Ket noi %s THANH CONG!\n", PORT_NAME);
    }
    else
    {
        printf("LOI: Khong mo duoc %s.\n", PORT_NAME);
        return 1;
    }

    printf("Chu ky mac dinh: %d ms\n", sampling_period);
    printf("-------------------------------------------------\n");
    printf(" HUONG DAN:\n");
    printf(" - Nhan 'E' de THOAT chuong trinh.\n");
    printf(" - Nhan 'C' de CAI DAT lai chu ky gui data.\n");
    printf("-------------------------------------------------\n");
    printf("Dang chay...\n");

    char buffer[1024];
    DWORD bytesRead;

    while (1)
    {
        // 1. Đọc dữ liệu từ Serial
        if (ReadFile(hSerial, buffer, sizeof(buffer) - 1, &bytesRead, NULL))
        {
            if (bytesRead > 0)
            {
                buffer[bytesRead] = '\0';
                processData(buffer);
            }
        }

        // Code test giả lập nếu không có mạch thật (bỏ comment để test)
        // char res[] = "Humidity: 60.50%, Temperature: 30.25 C";
        // processData(res);

        // 2. Xử lý phím bấm
        if (_kbhit())
        {
            char ch = _getch(); // Lấy ký tự

            // THOÁT CHƯƠNG TRÌNH
            if (ch == 'e' || ch == 'E')
            {
                printf("\nDang thoat chuong trinh...\n");
                break;
            }

            // --- PHẦN MỚI: CHỈNH SỬA CHU KỲ ---
            if (ch == 'c' || ch == 'C')
            {
                int new_period = 0;
                printf("\n\n========================================\n");
                printf("!!! TAM DUNG HE THONG DE CAI DAT !!!\n");
                printf("Chu ky hien tai: %d ms\n", sampling_period);
                printf("Nhap chu ky moi (ms): ");

                // Lệnh scanf sẽ tạm dừng chương trình chờ bạn nhập số và Enter
                if (scanf("%d", &new_period) == 1)
                {
                    if (new_period >= 100) // Ràng buộc tối thiểu 100ms để tránh treo
                    {
                        sampling_period = new_period;
                        printf("-> THANH CONG: Chu ky moi la %d ms.\n", sampling_period);
                    }
                    else
                    {
                        printf("-> LOI: Chu ky phai lon hon hoac bang 100ms!\n");
                    }
                }
                else
                {
                    // Xóa bộ nhớ đệm nếu nhập sai định dạng
                    while (getchar() != '\n')
                        ;
                    printf("-> LOI: Gia tri nhap khong phai la so!\n");
                }

                printf(">>> TIEP TUC GUI DU LIEU... <<<\n");
                printf("========================================\n\n");
            }
        }

        Sleep(10); // Nghỉ nhẹ để giảm tải CPU
    }

    CloseHandle(hSerial);
    return 0;
}