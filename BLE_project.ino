#include <BLEDevice.h> // Cung cấp các chức năng để tạo và quản lý các thiết bị BLE.
#include <BLEServer.h> // cung cấp các chức năng để tạo và quản lý máy chủ BLE.
#include <BLEUtils.h> // cung cấp các công cụ tiện ích cho BLE.
#include <BLE2902.h> // cung cấp các chức năng để tạo và quản lý các descriptor BLE với UUID 0x2902.
// #include <Adafruit_Sensor>
#include <DHT.h>

#define temperatureCelsius // Định nghĩa để sử dụng đơn vị đo là độ C.
#define bleServerName "Sensor_ESP32" // Định nghĩa tên của server BLE.
#define DHTPIN 26  // Chân kết nối với cảm biến DHT.
#define DHTTYPE DHT11   // Loại cảm biên đang dùng
DHT dht(DHTPIN, DHTTYPE); // Cấu hình cảm biến DHT.


float temp; // biến lưu giá trị nhiệt độ (C)
float tempF; // biến lưu giá trị nhiệt độ (F)
float humid; // biến lưu giá trị độ ẩm

unsigned long lastTime = 0; // Biến để lưu thời điểm lần cuối cùng thực hiện đo
unsigned long timerDelay = 30000; // Biến lưu tg delay

bool deviceConnected = false; // Trạng thái kết nối của thiết bị

#define SERVICE_UUID "91bad492-b950-4226-aa2b-4ede9fa42f59" // UUID của dịch vụ BLE mà thiết bị sẽ cung cấp

// tạo một đặc tính BLE với UUID dành cho nhiệt độ C hoặc F và thuộc tính PROPERTY_NOTIFY.
#ifdef temperatureCelsius
  BLECharacteristic dhtTemperatureCelsiusCharacteristics("cba1d466-344c-4be3-ab3f-189f80dd7518", BLECharacteristic::PROPERTY_NOTIFY);
  BLEDescriptor dhtTemperatureCelsiusDescriptor(BLEUUID((uint16_t)0x2902));
#else
  BLECharacteristic dhtTemperatureFahrenheitCharacteristics("f78ebbff-c8b7-4107-93de-889a6a06d408", BLECharacteristic::PROPERTY_NOTIFY);
  BLEDescriptor dhtTemperatureFahrenheitDescriptor(BLEUUID((uint16_t)0x2902));
#endif

// Tạo một đặc tính BLE cho độ ẩm với UUID và thuộc tính, gửi thông báo khi giá trị của nó thay đổi.
BLECharacteristic dhtHumidityCharacteristics("ca73b3ba-39f6-4ab3-91ae-186dc9577d99", BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor dhtHumidityDescriptor(BLEUUID((uint16_t)0x2903));

// Thiết lập callback khi kết nối và ngắt kết nối
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
  };
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};

// ĐỂ CẤU HÌNH VÀ KHỞI TẠO CÁC THÀNH PHẦN CẦN THIẾT CHO BLE.
// Khởi tạo cảm biến DHT
void initDHT(){
dht.begin(); 
}

void setup() {
  // Khởi tạo Serial và cảm biến DHT
  Serial.begin(115200); // Khởi động giao tiếp Serial với baud rate là 115200
  initDHT(); // // Gọi hàm initDHT để khởi tạo cảm biến DHT

  // Khởi tạo BLE Device và Server
  BLEDevice::init(bleServerName); // Khởi tạo thiết bị BLE với tên lưu ở biến bleServerName

  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer(); // Tạo một đối tượng BLE Server và gán vào con trỏ pServer
  // Thiết lập callback cho các sự kiện kết nối và ngắt kết nối bằng MyServerCallbacks đã định nghĩa trước đó.
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *dhtService = pServer->createService(SERVICE_UUID); // Tạo một dịch vụ BLE với UUID là SERVICE_UUID

  // Tạo và cấu hình BLE Characteristics và Descriptor
  // Temperature
  #ifdef temperatureCelsius
    dhtService->addCharacteristic(&dhtTemperatureCelsiusCharacteristics);
    dhtTemperatureCelsiusDescriptor.setValue("DHT temperature Celsius");
    dhtTemperatureCelsiusCharacteristics.addDescriptor(&dhtTemperatureCelsiusDescriptor);
  #else
    dhtService->addCharacteristic(&dhtTemperatureFahrenheitCharacteristics);
    dhtTemperatureFahrenheitDescriptor.setValue("DHT temperature Fahrenheit");
    dhtTemperatureFahrenheitCharacteristics.addDescriptor(&dhtTemperatureFahrenheitDescriptor);
  #endif  

  // Humidity
  dhtService->addCharacteristic(&dhtHumidityCharacteristics);
  dhtHumidityDescriptor.setValue("DHT humidity");
  dhtHumidityCharacteristics.addDescriptor(new BLE2902());
  
  // Start the service
  dhtService->start(); // Khởi động dịch vụ BLE đã được tạo

  // Bắt đầu dịch vụ BLE đã được tạo (dhtService).
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising(); // Lấy đối tượng quảng cáo BLE (pAdvertising).
  pAdvertising->addServiceUUID(SERVICE_UUID); // Thêm UUID 
  pServer->getAdvertising()->start(); // Kết nối với server
  Serial.println("Waiting a client connection to notify...");

}

// ĐỂ ĐỌC DỮ LIỆU TỪ CẢM BIẾN VÀ THÔNG BÁO CÁC GIÁ TRỊ NÀY TỚI TB BLE ĐÃ KẾT NỐI.
void loop() {
if (deviceConnected) { // Nếu có thiết bị kết nối
    if ((millis() - lastTime) > timerDelay) { // Kiểm tra xem đã đủ thời gian để đọc lại dữ liệu từ cảm biến
      temp = dht.readTemperature();
      // Fahrenheit
      tempF = 1.8*temp +32;
      // Read humidity
      humid = dht.readHumidity();
  
      //Notify temperature reading from BME sensor
      #ifdef temperatureCelsius
        static char temperatureCTemp[6]; // Mảng để lưu trữ chuỗi nhiệt độ C
        dtostrf(temp, 6, 2, temperatureCTemp); // Chuyển đổi giá trị nhiệt độ thành chuỗi
        //Set temperature Characteristic value and notify connected client
        dhtTemperatureCelsiusCharacteristics.setValue(temperatureCTemp); // Thiết lập giá trị của đặc tính nhiệt độ C
        dhtTemperatureCelsiusCharacteristics.notify(); // Thông báo rằng gtri nhiệt độ đã kết nối 
        Serial.print("Temperature Celsius: ");
        Serial.print(temp);
        Serial.print(" ºC");
      #else
        static char temperatureFTemp[6];
        dtostrf(tempF, 6, 2, temperatureFTemp);
        //Set temperature Characteristic value and notify connected client
        dhtTemperatureFahrenheitCharacteristics.setValue(temperatureFTemp);
        dhtTemperatureFahrenheitCharacteristics.notify();
        Serial.print("Temperature Fahrenheit: ");
        Serial.print(tempF);
        Serial.print(" ºF");
      #endif
      
      //Notify humidity reading from BME
      static char humidityTemp[6];
      dtostrf(humid, 6, 2, humidityTemp);
      //Set humidity Characteristic value and notify connected client
      dhtHumidityCharacteristics.setValue(humidityTemp);
      dhtHumidityCharacteristics.notify();   
      Serial.print(" - Humidity: ");
      Serial.print(humid);
      Serial.println(" %");
      
      lastTime = millis();
    }
  }

}
