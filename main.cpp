#include <Arduino.h>
#include <LCD_I2C.h>

LCD_I2C lcd(0x27, 16, 2); // Default address of most PCF8574 modules 0x27

//định nghĩa các chân và khai báo biến cần dùng
#define THERMISTORNOMINAL 10000   // resistance at 25 degrees C   
#define TEMPERATURENOMINAL 25   // temp. for nominal resistance (almost always 25 C)
#define BCOEFFICIENT 3435 // The beta coefficient of the thermistor (usually 3000-4000)
#define SERIESRESISTOR 9690   // the value of the 'other' resistor, điên trở thực tế,đo VOM để lấy  
#define NUMSAMPLES 10 // how many samples to take and average, more takes longer but is more 'smooth'
#define volt_sensor_1 32 // read voltage
#define current_sensor_1 14 // read current
#define temp_1 4  // read NTC1
#define relay_charge_adaptor_1 26 //kích sạc gồm bật nguồn ac và T1 trong adaptor 1
#define discharge_1 27 //đóng ngắt mosfet trong adaptor phần xả pin 1
#define bat_tat 34 //nhấn lần 1 start,nhấn lần nữa stop hiện lên lcd 
#define enter 35 //chuyển đổi giữa các màn hình hiển thị lcd : áp1,dòng1,Temp1 , trạng thái đang sạc hay đang xả
#define ALPHA 0.05        // Hệ số lọc thông thấp
#define ALPHA_T 0.03        // Hệ số lọc thông thấp nhiet do
float V_offset_acs712 = 0; // điện áp đo ở Vout khi chưa có dòng,dung VOM do 
float offsetV = 1.09; // diện áp sụt khi qua tranducer va dien tro phan ap
float ap1; // gia tri của ham_doc_ap_1()
float dong1; // gia tri của ham_doc_dong_1()
float nhietdo1; // gia tri của ham_doc_nhiet_do1()
bool buttonStateBT1 = 0;         // Trạng thái của nút nhấn hiện tại
bool lastButtonStateBT1 = 1;     // Trạng thái trước đó của nút nhấn (bắt đầu ở HIGH do điện trở kéo lên)
bool buttonStateBT2 = 0;         // Trạng thái của nút nhấn hiện tại
bool lastButtonStateBT2 = 1;     // Trạng thái trước đó của nút nhấn (bắt đầu ở HIGH do điện trở kéo lên)
int16_t currentScreen = 0;       // Biến để lưu trạng thái màn hình hiện tại
float last_ap1 = -9999; // Giá trị cũ của điện áp
float last_dong1 = -9999; // Giá trị cũ của dòng điện
float last_nhietdo1 = -9999; // Giá trị cũ của nhiệt độ
int16_t cycle1 = 0;
// Hệ số tỉ lệ phân áp 
const float voltageDividerRatio = (4633.0 + 2190.0) / 2190.0; // (R1+R2)/R1
// Các biến theo dõi trạng thái
bool charging1 = false;
bool discharging1 = false;
bool errorState = false;
bool systemRunning = false; // Quản lý trạng thái bật tắt hệ thống

// Non-blocking delay function, các biến thời gian
unsigned long lastUpdateMillisLCD = 0;
unsigned long lastUpdateMillisADC1=0;
unsigned long lastUpdateMillisADC2=0;
unsigned long lastUpdateMillisADC3=0;
unsigned long lastUpdateMillisCOM = 0;
//unsigned long lastUpdateMillisCharge =0;
const long updateIntervalLCD = 1000;
const long updateIntervalCOM = 1000; // biến thời gian để chạy 1 giây 1 lần gửi data cổng COM
const long updateIntervalADC1 = 400;  // Interval for ADC readings (400 ms)
const long updateIntervalADC2 = 650;  // Interval for ADC readings (600 ms)
const long updateIntervalADC3 = 900;  // Interval for ADC readings (900 ms)
//const long updateIntervalCharge = 60000;  // Interval for update charge (60s)
///*
void ham_doc_ap_1(){
///ghi code vao đây
int16_t samples3[NUMSAMPLES];
      uint8_t i;
      float average_value3;
      float filteredVoltage = 0;
        // take NUMSAMPLES samples in a row, with a slight delay
      for (i=0; i < NUMSAMPLES; i++)
         {
            samples3[i]= analogRead(volt_sensor_1); 
            delay(10);
         }
      
        // average all the samples out
          average_value3 = 0;
      for (i=0; i< NUMSAMPLES; i++) 
        {
           average_value3 += samples3[i];
        }
          average_value3 /= NUMSAMPLES;
          // Bộ lọc trung bình động
    
            // Ta sẽ đọc giá trị của cảm biến được arduino số hóa trong khoảng từ 0-4095 
            //Giá trị được số hóa thành 1 số nguyên có giá trị trong khoảng từ 0 đến 4095 tuong ứng 0-3.3V 
        float v11 = (average_value3/4095.0)*3.3;
        filteredVoltage = ALPHA * v11 + (1 - ALPHA) * v11;  // Áp dụng bộ lọc thông thấp
                     /*
                      Serial.print("Volt ADC: ");
                      Serial.print(filteredVoltage);
                      Serial.println();
                      delay(1000);
                      */
        ap1 = ((filteredVoltage*offsetV*voltageDividerRatio)*10.03); 
       

}
// đọc dòng từ current sensor 
void calibrateCurrentSensor() {
    int16_t samples[NUMSAMPLES];
    float total = 0;
    uint8_t l;
    // Lấy mẫu và tính trung bình để xác định giá trị trung tâm (offset)
    for (uint8_t l = 0; l < NUMSAMPLES; l++) {
        samples[l] = analogRead(current_sensor_1);
        delayMicroseconds(100); // Để tín hiệu ổn định
    }
    // Tính trung bình của các giá trị đọc
    for ( l = 0; l < NUMSAMPLES; l++) {
        total += samples[l];
    }
    V_offset_acs712 = (total / NUMSAMPLES) * 3.3 / 4095.0; // Quy đổi sang điện áp
    V_offset_acs712 = 2*V_offset_acs712; // khôi phuc trước phân áp

    
}
void ham_doc_dong_1(){
      ///ghi code vao đây
     int16_t samples2[NUMSAMPLES];
      uint8_t j;
      float average_value2;
      float filteredValue = 0;
    
      // take UMSAMPLES samples in a row, with a slight delay
      for (j=0; j< NUMSAMPLES; j++)
         {
            samples2[j] = analogRead(current_sensor_1);
            delay(10);
         }
      
         // average all the samples out
          average_value2 = 0;
      for (j=0; j< NUMSAMPLES; j++) 
        {
           average_value2 += samples2[j];
        }
          average_value2 /= NUMSAMPLES;
         
        float voltage =(average_value2/4095.0)*3.3;   // Giá trị được số hóa thành 1 số nguyên có giá trị trong khoảng từ 0 đến 4095 tuong ứng 0-3.3V
        filteredValue = ALPHA * voltage + (1 - ALPHA) * voltage;  // Áp dụng bộ lọc thông thấp
        filteredValue  = filteredValue * 2; // Khôi phục điện áp thực trước phân áp
                      /*
                      Serial.print("Volt acs712: ");
                      Serial.print(filteredValue);
                      Serial.println();
                      delay(1000);    
                      */  
        // Bây giờ ta chỉ cần tính ra giá trị dòng điện, 0.1 là độ nhạy của sensor 20A (100mV/A) 
        dong1 = (filteredValue-V_offset_acs712)/0.1;
        
      
}
//*/
void ham_doc_NTC1(){
  // Khởi tạo biến average
    float average = 0;
    
    uint8_t k;
    // Đọc mẫu và lọc nhiễu
    for (k = 0; k < NUMSAMPLES; k++) {
        average += analogRead(temp_1);
        delay(10); // Tăng delay lên để ADC ổn định
    }
    average /= NUMSAMPLES;
    // Chuyển đổi sang điện áp và điện trở
    float voltage = (average * 3.3) / 4095.0;
    float resistance = (SERIESRESISTOR * voltage) / (3.3 - voltage);
   
    float new_resistance = ALPHA_T * resistance + (1 - ALPHA_T) * resistance;
                
    // Tính nhiệt độ theo công thức Steinhart-Hart
    float steinhart = new_resistance / THERMISTORNOMINAL;
    steinhart = log(steinhart);
    steinhart /= BCOEFFICIENT;
    steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15);
    steinhart = 1.0 / steinhart;
    steinhart -= 273.15;
    nhietdo1 = steinhart;
}
// các hàm hiện thị giá trị lên LCD
void hien_thi_dien_ap1(){
    if (ap1 != last_ap1) {  
        lcd.setCursor(0, 1);
        lcd.print("   V1: ");
        lcd.print(ap1);
        lcd.print(" V     ");  // In thêm khoảng trắng để xóa ký tự cũ
        last_ap1 = ap1;  // Cập nhật giá trị mới
    }
}
void hien_thi_nhiet_do1(){
       if (nhietdo1 != last_nhietdo1) {  
        lcd.setCursor(0, 1);
        lcd.print("   T1: ");
        lcd.print(nhietdo1);
        lcd.print(" C    ");  // In thêm khoảng trắng để xóa ký tự cũ
        last_nhietdo1 = nhietdo1;  // Cập nhật giá trị mới
    }  
}
void hien_thi_dong_dien1(){
    if (dong1 != last_dong1) {  
        lcd.setCursor(0, 1);
        lcd.print("   A1: ");
        lcd.print(dong1);
        lcd.print(" A    ");  // In thêm khoảng trắng để xóa ký tự cũ
        last_dong1 = dong1;  // Cập nhật giá trị mới
    }
}
void hien_thi_cycle(){
         lcd.setCursor(0, 1);
         lcd.print("  Cycle : ");
         lcd.print(cycle1);
         lcd.print("      ");
}
//các hàm chu trình trạng thái hoạt đông sạc xả
void start_charge1(){
    
    lcd.setCursor(0, 0);
    lcd.print("   Charging...   ");   
    charging1 = true;
    discharging1 = false;   
    digitalWrite(discharge_1,LOW);
    digitalWrite(relay_charge_adaptor_1,HIGH); 
}
void start_discharge1(){
  
    lcd.setCursor(0, 0);
    lcd.print("   Discharging... ");
    charging1 = false;
    discharging1 = true;
    digitalWrite(discharge_1,HIGH);
    digitalWrite(relay_charge_adaptor_1,LOW); 
}
void system_stop(){
    lcd.setCursor(0, 0);
    lcd.print("  System Stop... ");  
    charging1 = false;
    discharging1 = false;
    systemRunning = false;
    digitalWrite(discharge_1,LOW);
    digitalWrite(relay_charge_adaptor_1,LOW);
}
// các hàm hiện thị LCD
void showScreen(int Screen) {
    if (Screen == 0) {
        lcd.setCursor(0, 1);
        lcd.print("   V1: ");
        lcd.print(ap1);
        lcd.print(" V     ");  // In thêm khoảng trắng để xóa ký tự cũ
     } 
  else if (Screen == 1) {
       lcd.setCursor(0, 1);
       lcd.print("   A1: ");
        lcd.print(dong1);
        lcd.print(" A    ");  // In thêm khoảng trắng để xóa ký tự cũ
       } 
  else if (Screen == 2) {
    
        lcd.setCursor(0, 1);
        lcd.print("   T1: ");
        lcd.print(nhietdo1);
        lcd.print(" C    ");  // In thêm khoảng trắng để xóa ký tự cũ
      } 
    else if (Screen == 3) {
        lcd.setCursor(0, 1);
        lcd.print("  Cycle : ");
        lcd.print(cycle1);
        lcd.print("      ");
  }
  
}
void updatescreen(int curentScreen){

// Cập nhật các giá trị theo màn hình hiện tại
    switch (currentScreen) {
       
       case 0:
            hien_thi_dien_ap1();  // Màn hình điện áp
            break;
        case 1:
            hien_thi_dong_dien1();  // Màn hình dòng điện
            break;
        case 2:
            hien_thi_nhiet_do1();  // Màn hình nhiệt độ
            break;
        
        case 3:
           
            hien_thi_cycle();  // Màn hình cycle P1
            break;

    }
}
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200); // open port 115200
  
  //code cho LCD I2C
  lcd.begin();    //Khởi động màn hình LCD                
  lcd.backlight();//Bật đèn nền LCD 16×2.
  lcd.clear(); // clear man hình
  
    // cấu hình các chân
    pinMode(current_sensor_1,INPUT);
    pinMode(volt_sensor_1,INPUT);
    pinMode(temp_1,INPUT);
    pinMode(bat_tat,INPUT);
    pinMode(enter,INPUT);
    pinMode(relay_charge_adaptor_1,OUTPUT);
    pinMode(discharge_1,OUTPUT);
   // lúc đầu tắt hết output sạc xả
    digitalWrite(relay_charge_adaptor_1,LOW);
    digitalWrite(discharge_1,LOW);
    // Print a message to the LCD.
  lcd.print("Calibrating...");
  // Gọi hàm hiệu chỉnh cảm biến dòng
    calibrateCurrentSensor();
    delay(2000);
    lcd.clear();
  lcd.print(" Press BT1 start");
     // Đợi LCD và ADC ổn định
    delay(200);
}

void loop() {
            // đọc trạng thái nút nhấn
            buttonStateBT1 = digitalRead(bat_tat);
         
            if(buttonStateBT1==LOW && lastButtonStateBT1== HIGH){
                delay(50);
                    // code ở đây 
                    if(!systemRunning){ 
                     systemRunning = true;   
                    start_charge1();
                    }
                    else{
                    system_stop();
                    }
                    if(systemRunning){
                        if(charging1){
                          if(ap1>41.9 && dong1 <=0.1 && nhietdo1 <=33){
                          start_discharge1();        
                          } 
                        }
                        if(discharging1){
                            if(ap1<=20){
                                digitalWrite(discharge_1,LOW);
                                if(nhietdo1<45){
                                    start_charge1();
                                    cycle1++;
                                }
                            }
                        }
                    }

            }
            lastButtonStateBT1 = buttonStateBT1; 

           buttonStateBT2 = digitalRead(enter);
             if (buttonStateBT2 == LOW && lastButtonStateBT2 == HIGH) {
                     delay(50);  // debounce
                    currentScreen = (currentScreen + 1) % 4; // Vòng lặp giữa các màn hình 0-3
                    showScreen(currentScreen);  
                    
            }
            lastButtonStateBT2 = buttonStateBT2;

            // update man hinh lcd mỗi updateIntervalLCD giây
            if(millis()-lastUpdateMillisLCD >= updateIntervalLCD){
                lastUpdateMillisLCD = millis();
                updatescreen(currentScreen);
            }

            // update đọc ADC môi updateIntervalADC giây
            if(millis()-lastUpdateMillisADC1 >= updateIntervalADC1){
                lastUpdateMillisADC1 = millis();
                ham_doc_ap_1();   
            }
            if(millis()-lastUpdateMillisADC2 >= updateIntervalADC2){
                lastUpdateMillisADC2 = millis();
                
                ham_doc_dong_1(); 
                 // Giới hạn dòng điện
                   if(dong1 < 0) dong1 = 0;
                  if(dong1 > 20) dong1 = 20;
            }
            if(millis()-lastUpdateMillisADC3 >= updateIntervalADC3){
                lastUpdateMillisADC3 = millis();      
                ham_doc_NTC1();
                
            }
            //giao tiếp đọc data từ cổng COM
            
            if(Serial.available()>0){
                
                  if (millis() - lastUpdateMillisCOM >= updateIntervalCOM) {
                    lastUpdateMillisCOM = millis();
                      // cycle
                      Serial.print("Cycle : ");
                      Serial.print(cycle1);
                      Serial.print(",");
                        //ham_doc_ap_1();
                      Serial.print("Volt 1 (V):");
                      Serial.print(ap1);
                      Serial.print(",");
                       //ham_doc_dong_1();
                      Serial.print("Current 1 (A):");
                      Serial.print(dong1);
                      Serial.print(",");
  
                     //ham_doc_NTC1();
                      Serial.print("Nhiet do 1 (C):");
                      Serial.print(nhietdo1);
                      Serial.print(",");
                      Serial.println("");
                     
                } 
            }

}
