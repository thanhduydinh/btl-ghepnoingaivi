#include "BLEDevice.h"  //  giao tiếp qua Bluetooth Low Energy.
#include <Wire.h>       // giao tiếp I2C
#include <Adafruit_SSD1306.h> // OLED SSD1306 và các tính năng đồ họa
#include <Adafruit_GFX.h>

#define temperatureCelsius // Định nghĩa đơn vị là độ C
#define bleServerName "BME280_ESP32"// Định nghĩa tên của server BLE. Đây là tên của
// thiết bị ESP32 chạy chương trình server BLE.

// Định nghĩa UUID của dịch vụ BLE mà thiết bị sẽ kết nối tới.
static BLEUUID bmeServiceUUID("91bad492-b950-4226-aa2b-4ede9fa42f59");
// Kiểm tra nếu macro temperatureCelsius đã được định nghĩa.
#ifdef temperatureCelsius
  // Nếu temperatureCelsius được định nghĩa, UUID của đặc tính nhiệt độ C được gán.
  static BLEUUID temperatureCharacteristicUUID("cba1d466-344c-4be3-ab3f-189f80dd7518");
#else
  // Nếu temperatureCelsius không được định nghĩa, sẽ chọn đặc tính nhiệt độ F.
  static BLEUUID temperatureCharacteristicUUID("f78ebbff-c8b7-4107-93de-889a6a06d408");
#endif

  //  Định nghĩa UUID của đặc tính độ ẩm BLE.
static BLEUUID humidityCharacteristicUUID("ca73b3ba-39f6-4ab3-91ae-186dc9577d99");
static boolean doConnect = false; // Đã bắt đầu kết nối tới thiết bị ngoại vi hay chưa
static boolean connected = false; // Trạng thái kết nối hiện tại

// Khai báo một con trỏ pServerAddress kiểu BLEAddress. 
// Biến này sẽ lưu trữ địa chỉ của thiết bị ngoại vi mà ESP32 sẽ kết nối tới.
// Địa chỉ này sẽ được xác định trong quá trình quét thiết bị
static BLEAddress *pServerAddress;
// Khai báo một con trỏ temperatureCharacteristic kiểu BLERemoteCharacteristic.
// Biến này sẽ lưu trữ đặc tính nhiệt độ mà ESP32 muốn đọc từ thiết bị ngoại vi.
static BLERemoteCharacteristic* temperatureCharacteristic;
// Khai báo một con trỏ humidityCharacteristic kiểu BLERemoteCharacteristic.
// Biến này sẽ lưu trữ đặc tính độ ẩm mà ESP32 muốn đọc từ thiết bị ngoại vi.
static BLERemoteCharacteristic* humidityCharacteristic;

// Khai báo một mảng notificationOn kiểu const uint8_t với hai phần tử {0x1, 0x0}.
// Mảng này được sử dụng để kích hoạt thông báo (notify) cho một đặc tính BLE. 
// Thông báo cho phép thiết bị ngoại vi gửi dữ liệu ngay lập tức khi có 
// sự thay đổi giá trị của đặc tính mà không cần thiết bị trung tâm phải yêu cầu đọc giá trị đó.
const uint8_t notificationOn[] = {0x1, 0x0};
// Mảng này được sử dụng để vô hiệu hóa thông báo cho một đặc tính BLE.
const uint8_t notificationOff[] = {0x0, 0x0};

// Định nghĩa chiều dài và chiều rộng của màn hình OLED
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 

// Khai báo cho màn hình SSD1306 được kết nối với I2C (chiều dài,chiều rộng,sử dụng giao thức I2C,ko có chân reset)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

char* temperatureChar; // lưu trữ giá trị nhiệt độ 
char* humidityChar; // lưu trữ giá trị độ ẩm

//Khai báo một biến newTemperature kiểu boolean với giá trị ban đầu là false.
// Biến này dùng để kiểm tra xem liệu có giá trị nhiệt độ mới hay không.
boolean newTemperature = false;
boolean newHumidity = false;

// CONECTTOSERVER
// Định nghĩa một hàm connectToServer nhận vào một tham số pAddress kiểu BLEAddress. 
// Hàm này trả về true nếu kết nối và thiết lập thành công, ngược lại trả về false.
bool connectToServer(BLEAddress pAddress) {
  //  Tạo một đối tượng pClient thuộc lớp BLEClient bằng cách gọi phương thức createClient của BLEDevice.
   BLEClient* pClient = BLEDevice::createClient();
  //  Kết nối tới server BLE với địa chỉ pAddress.
  pClient->connect(pAddress);
  Serial.println(" - Connected to server");
 
  // Lấy một tham chiếu tới server từ server BLE bằng cách sử dụng UUID server bmeServiceUUID.
  BLERemoteService* pRemoteService = pClient->getService(bmeServiceUUID);
  // Kiểm tra nếu không tìm thấy server thì in ra thông báo lỗi và trả về false.
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(bmeServiceUUID.toString().c_str());
    return (false);
  }
  // Lấy một tham chiếu tới đặc tính nhiệt độ, độ ẩm từ server.
  temperatureCharacteristic = pRemoteService->getCharacteristic(temperatureCharacteristicUUID);
  humidityCharacteristic = pRemoteService->getCharacteristic(humidityCharacteristicUUID);
  // Nếu ko nhận được giá trị của nhiệt độ ,độ ẩm thì trả về false
  if (temperatureCharacteristic == nullptr || humidityCharacteristic == nullptr) {
    Serial.print("Failed to find our characteristic UUID");
    return false;
  }
  // Còn ko thì in ra thông báo đã tìm thấy giá trị của nhiệt độ độ ẩm trên server
  Serial.println(" - Found our characteristics");
  // Đăng ký hàm callback temperatureNotifyCallback để xử lý thông báo từ đặc tính nhiệt độ, độ ẩm.
  temperatureCharacteristic->registerForNotify(temperatureNotifyCallback);
  humidityCharacteristic->registerForNotify(humidityNotifyCallback);
  // Trả về true để xác nhận kết nối và thiết lập thành công.
  return true;
}

// XỬ LÝ KHI BLE NHẬN ĐƯỢC THÔNG BÁO TỪ THIẾT BỊ KHÁC
// Khai báo một lớp định nghĩa các hành vi khi thiết bị BLE nhận được thông báo từ các thiết bị khác.
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  // Định nghĩa hàm onResult được gọi khi thiết bị BLE nhận được một thông báo từ thiết bị khác. 
  // Hàm này nhận một tham số advertisedDevice kiểu BLEAdvertisedDevice, đại diện cho thiết bị đã gửi thông báo.
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.getName() == bleServerName) { //Kiểm tra tên có trùng ko
      advertisedDevice.getScan()->stop(); // Gọi phương thức stop() của đối tượng quét (getScan()) để dừng quá trình quét, vì đã tìm thấy thiết bị cần tìm.
      // Tạo một đối tượng BLEAddress mới với địa chỉ của thiết bị. Đây là địa chỉ của thiết bị mà ta cần kết nối.
      pServerAddress = new BLEAddress(advertisedDevice.getAddress());
      doConnect = true; // Đặt biến doConnect thành true, chỉ thị rằng đã sẵn sàng để kết nối tới thiết bị.
      Serial.println("Device found. Connecting!"); //In ra thông báo để xác nhận rằng đã tìm thấy thiết bị và đang chuẩn bị kết nối.
    }
  }
};

// LƯU TRỮ GIÁ TRỊ NHIỆT ĐỘ VÀ ĐỘ ẨM
// Nhiệt độ : pData - Con trỏ lưu dữ liệu giá trị nhiệt độ ,length - độ dài dữ liệu thông báo, isNotify - đây có phải thông báo
static void temperatureNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  temperatureChar = (char*)pData;// lưu trữ giá trị nhiệt độ ,pData từ uint8_t*(số nguyên) sang char* (chuỗi).
  newTemperature = true; // Đặt biến newTemperature thành true để chỉ ra rằng đã có giá trị nhiệt độ mới.
}

// Độ ẩm - tương tự nhiệt độ
static void humidityNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  humidityChar = (char*)pData;
  newHumidity = true;
  Serial.print(newHumidity);
}

// HIỂN THỊ NHIỆT ĐỘ ĐỘ ẨM
// Hàm này sẽ hiển thị các giá trị đo được (nhiệt độ và độ ẩm) lên màn hình OLED
void printReadings(){
  display.clearDisplay(); //  Xóa nội dung hiện tại trên màn hình OLED, chuẩn bị cho việc vẽ nội dung mới.
  // hiển thị nhiệt độ 
  display.setTextSize(1); // Kích thước chữ 
  display.setCursor(0,0); // Đặt con trỏ vẽ chữ tại vị trí (0, 0) trên màn hình OLED
  display.print("Temperature: ");
  display.setTextSize(2); 
  display.setCursor(0,10);
  display.print(temperatureChar); // in ra giá trị nhiệt độ
  display.setTextSize(1);
  display.cp437(true); // Chuyển đổi bảng mã ký tự sang CP437 để có thể sử dụng các ký tự đặc biệt.
  display.write(167); // In ký tự đặc biệt có mã là 167 (ký tự °, biểu tượng độ) lên màn hình OLED.
  display.setTextSize(2);
  Serial.print("Temperature:");
  Serial.print(temperatureChar);

  // kiểm tra xem biến kia được định nghĩa ko thì in ra độ C hoặc độ F
  #ifdef temperatureCelsius
    //Temperature Celsius
    display.print("C");
    Serial.print("C");
  #else
    //Temperature Fahrenheit
    display.print("F");
    Serial.print("F");
  #endif

  // Hiển thị độ ẩm
  display.setTextSize(1);
  display.setCursor(0, 35);
  display.print("Humidity: ");
  display.setTextSize(2);
  display.setCursor(0, 45);
  display.print(humidityChar);
  display.print("%");
  display.display();
  Serial.print(" Humidity:");
  Serial.print(humidityChar); 
  Serial.println("%");
}

// THIẾT LẬP CẤU HÌNH BAN ĐẦU CHO CHƯƠNG TRÌNH
//  Định nghĩa hàm setup, được gọi một lần khi chương trình bắt đầu chạy trên Arduino. Hàm này thiết lập các cấu hình ban đầu.
void setup() {
  // Khởi tạo màn hình OLED với địa chỉ I2C là 0x3C
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Dừng chương trình vòng lặp vô hạn để không tiếp tục chạy nữa.
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE,0); // Đặt màu chữ là màu trắng và nền là màu đen.
  display.setCursor(0,25); // Đặt con trỏ vẽ chữ tại vị trí (0, 25) trên màn hình OLED.
  display.print("BLE Client");
  display.display();
  Serial.begin(115200); // Khởi động Serial Monitor với tốc độ truyền 115200 baud
  Serial.println("Starting Arduino BLE Client application...");

  // khởi tạo thiết bị BLE.
  BLEDevice::init(""); // Khởi tạo thiết bị BLE với tên mặc định (chuỗi rỗng).
 
  // thiết lập quét BLE với callback để xử lý khi phát hiện thiết bị mới, và bắt đầu quét trong 30 giây.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks()); // Callback này sẽ được gọi khi phát hiện một thiết bị BLE mới.
  pBLEScan->setActiveScan(true);
  pBLEScan->start(30); 
}

// THỰC HIỆN CÁC HÀNH ĐỘNG CHÍNH CỦA CHƯƠNG TRÌNH
void loop() {
  // Nếu cờ doConnect là true, điều đó có nghĩa là đã quét và tìm thấy máy chủ BLE và sẽ kết nối với nó. 
  // Khi kết nối thành công, đặt cờ connected là true.
  if (doConnect == true) {
    // Gọi hàm connectToServer với địa chỉ máy chủ BLE, kiểm tra nếu kết nối thành công (true).
    if (connectToServer(*pServerAddress)) {
      Serial.println("We are now connected to the BLE Server.");
      // Kích hoạt thuộc tính Notify của đặc tính nhiệt độ bằng cách ghi giá trị notificationOn vào Descriptor có UUID 0x2902.
      temperatureCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
      humidityCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
      connected = true; // Kết nối thành công 
    } else {
      Serial.println("We have failed to connect to the server; Restart your device to scan for nearby BLE server again.");
    }
    doConnect = false;
  }
  // Nếu có các giá trị nhiệt độ và độ ẩm mới, sẽ in chúng lên màn hình OLED.
  if (newTemperature && newHumidity){
    newTemperature = false;
    newHumidity = false;
    printReadings();
  }
  delay(1000);
}