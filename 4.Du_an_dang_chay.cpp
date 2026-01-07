/*
HỆ THỐNG CẢNH BÁO LŨ
*/
// 1. Cấu hình Blynk
#define BLYNK_TEMPLATE_ID "TMPL6yudQwaMH"
#define BLYNK_TEMPLATE_NAME "Canh bao lu lut"
#define BLYNK_AUTH_TOKEN "eI-4scDw5s52rO22oAQuMYM7EGhEK0sf"
#define BLYNK_PRINT Serial
// 2. Thư viện
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
// 3. Cấu hình Wifi
char ssid[] = "TP-Link_2A0A"; 
char pass[] = "19628448"; 
// 4. CẤU HÌNH PHẦN CỨNG
const int TRIG = 19;
const int ECHO = 18;
// --- BIẾN TOÀN CỤC (Thay đổi được từ App) ---
// Giá trị mặc định ban đầu (nếu chưa chỉnh trên app)
float chieu_cao_lap_dat = 170.0;  // V1: Chiều cao từ cảm biến xuống mặt nước
float muc_nguy_hiem = 50.0;       // V2: Mức nước dâng thêm thì báo động

// OLED SH1106
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3C
#define OLED_RESET -1
Adafruit_SH1106G man_hinh = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Tạo Timer
BlynkTimer bo_dem_gio;

// ========================== XỬ LÝ DỮ LIỆU TỪ BLYNK ====================

// Hàm này chạy khi ESP32 kết nối thành công tới Blynk
// Mục đích: Lấy lại giá trị cài đặt cũ từ Server xuống (nếu ESP32 bị reset)
BLYNK_CONNECTED() {
  Blynk.syncVirtual(V1, V2);
}

// Nhận giá trị Chiều cao lắp đặt từ chân ảo V1
BLYNK_WRITE(V1) {
  float gia_tri_moi = param.asFloat();
  if (gia_tri_moi > 0) { // Chỉ nhận giá trị dương
    chieu_cao_lap_dat = gia_tri_moi;
  }
}
// Nhận giá trị Mức nguy hiểm từ chân ảo V2
BLYNK_WRITE(V2) {
  float gia_tri_moi = param.asFloat();
  if (gia_tri_moi > 0) {
    muc_nguy_hiem = gia_tri_moi;
  }
}

// ========================== HÀM ĐO 1 LẦN ==============================
float do1Lan() {
  digitalWrite(TRIG, LOW);  delayMicroseconds(3);
  digitalWrite(TRIG, HIGH);  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  // Timeout 30000us (30ms) phù hợp cho khoảng cách ~5m. Nếu quá 30ms không thấy sóng về (xa quá 5m) thì trả về 0 luôn.
  long duration = pulseIn(ECHO, HIGH, 30000); 
  if (duration == 0) return -1;
// Công thức vật lý: Quãng đường = Thời gian x Tốc độ âm thanh (0.0343 cm/us) / 2 (đi và về)
  float d = duration * 0.0343 / 2;
  // Lọc nhiễu thông minh:
  // Loại bỏ các giá trị quá gần (<20cm - điểm mù) 
  // Loại bỏ giá trị quá xa vô lý (> chiều cao lắp đặt + 10cm sai số)
  if (d < 20 || d > (chieu_cao_lap_dat + 10)) return -1; 
  return d;
}

// ========================== LỌC TRUNG VỊ ===============================
float doTrungVi() {
  float arr[9]; 
  int count = 0;

  for (int i = 0; i < 9; i++) {
    float v = do1Lan();
    if (v > 0) arr[count++] = v;
    delay(40); 
  }
  if (count < 3) return -1; // Quá ít mẫu tốt thì báo lỗi
  // Sắp xếp tăng dần
  for (int i = 0; i < count - 1; i++)
    for (int j = i + 1; j < count; j++)
      if (arr[j] < arr[i]) {
        float t = arr[i]; arr[i] = arr[j]; arr[j] = t;
      }
  return arr[count / 2]; 
}

// ========================== XỬ LÝ VÀ HIỂN THỊ =========================
void xuLyVaHienThi() {
  float khoang_cach = doTrungVi();
  float muc_nuoc = 0;

  // Logic tính toán:
  if (khoang_cach > 0) {
    // Dùng biến chieu_cao_lap_dat thay vì hằng số
    muc_nuoc = chieu_cao_lap_dat - khoang_cach;
    if (muc_nuoc < 0) muc_nuoc = 0;
  } else {
     // Nếu lỗi (do -1), giữ nguyên 0 hoặc báo lỗi
    muc_nuoc = 0; 
  }

  // Gửi Blynk
  Blynk.virtualWrite(V0, muc_nuoc);

  // Hiển thị OLED
  man_hinh.clearDisplay();
  man_hinh.setTextColor(SH110X_WHITE); 
  
  man_hinh.setCursor(0, 0);
  man_hinh.setTextSize(1);
  man_hinh.println(" MUC NUOC DANG THEM ");

  man_hinh.setTextSize(2);
  
  // So sánh với biến muc_nguy_hiem (đã cài từ App)
  if (muc_nuoc > muc_nguy_hiem) {
     man_hinh.println("NGUY HIEM!");
     Blynk.logEvent("canh_bao_lu", "Nuoc dang cao nguy hiem!"); 
     man_hinh.print(" ");
     man_hinh.print(muc_nuoc, 0);
     man_hinh.println(" cm");
  } else {
     man_hinh.print("   ");
     man_hinh.print(muc_nuoc, 0);
     man_hinh.println(" cm");
  }

  // Trạng thái Wifi
  man_hinh.setTextSize(1);
  man_hinh.setCursor(0, 55);
  man_hinh.print(Blynk.connected() ? "On " : "Off ");
  
  // Hiển thị thêm thông số cài đặt để dễ kiểm soát (Debug)
  
  // In ra: H:170 W:50 D:120 (H=Height, W=Warning, D=Distance)
  man_hinh.print("H:"); man_hinh.print((int)chieu_cao_lap_dat); // Chiều cao lắp (V1)
  man_hinh.print(" W:"); man_hinh.print((int)muc_nguy_hiem);     // Mức cảnh báo (V2)
  man_hinh.print(" D:"); man_hinh.print((int)khoang_cach);       // Khoảng cách đo được

  man_hinh.display();
  // Debug ra máy tính
  Serial.printf("Cai dat: H=%.0f, Canhbao=%.0f | Do duoc: %.1f | Nuoc: %.1f\n", 
                chieu_cao_lap_dat, muc_nguy_hiem, khoang_cach, muc_nuoc);
}

// ========================== SETUP =====================================
void setup() {
  Serial.begin(115200);
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  if(!man_hinh.begin(SCREEN_ADDRESS, true)) {
    Serial.println("OLED Error");
    for(;;);
  }
  
  man_hinh.clearDisplay();
  man_hinh.setTextColor(SH110X_WHITE); 
  man_hinh.setCursor(0,0);
  man_hinh.setTextSize(1);
  man_hinh.println("Dang ket noi WiFi...");
  man_hinh.display();

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  // Cập nhật 2 giây/lần
  bo_dem_gio.setInterval(2000L, xuLyVaHienThi);
}
// ========================== LOOP ======================================
void loop() {
  Blynk.run();
  bo_dem_gio.run();
}