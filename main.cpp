#include <Arduino.h>
#include <LiquidCrystal.h>
#include <Wire.h> 
//#include <LiquidCrystal_I2C.h>
//LiquidCrystal_I2C lcd(0x3F,16,2); // Địa chỉ I2C, số cột, số dòng
// Sử dụng chân mặc định cho lcd i2C
//Wire.begin();  // Sử dụng SDA=2, SCL=3
// Hoặc sử dụng chân thay thế
//Wire.begin(4, 5);  // SDA=4, SCL=5
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2; // sơ đồ kết nối chân lcd 16x2a với arduino
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
/*The circuit:
 * LCD RS pin to digital pin 12
 * LCD Enable pin to digital pin 11
 * LCD D4 pin to digital pin 5
 * LCD D5 pin to digital pin 4
 * LCD D6 pin to digital pin 3
 * LCD D7 pin to digital pin 2
 * LCD R/W pin to ground
 * 10K resistor:
 * ends to +5V and ground
 * V0 qua biên trở 10k để chỉnh độ sáng chữ hiện
*/

//CÁC BIẾN CẦN DÙNG
#define THERMISTORNOMINAL 10000   // resistance at 25 degrees C   
#define TEMPERATURENOMINAL 25   // temp. for nominal resistance (almost always 25 C)
#define BCOEFFICIENT 3435 // The beta coefficient of the thermistor (usually 3000-4000)
#define SERIESRESISTOR 9690   // the value of the 'other' resistor, điên trở thực tế,đo VOM để lấy  
#define NUMSAMPLES 5 // how many samples to take and average, more takes longer but is more 'smooth'
#define current_sensor_1 A0
#define current_sensor_2 A1
#define volt_sensor_2 A2
#define volt_sensor_1 A3
#define temp_1 A4  // NTC1
#define temp_2 A5 //NTC2
#define relay_charge_adaptor_1 13 //kích sạc gồm bật nguồn ac và T1 trong adaptor 1
#define relay_charge_adaptor_2 6 //kích sạc gồm bật nguồn ac và T1 trong adaptor 2
#define discharge_1 8 //đóng ngắt mosfet trong adaptor phần xả pin 1
#define relay_5 7 //out điều khiển relay,mục đích chưa rõ
#define bat_tat 10 //nhấn lần 1 start,nhấn lần nữa stop hiện lên lcd ,16 trên arduino micro, 10 trên arduino uno
#define enter 9 //chuyển đổi giữa các màn hình hiển thị lcd : áp1,dòng1,Temp1 , áp2,dòng2,Temp2 , trạng thái đang sạc hay đang xả


float ap1; // gia tri của ham_doc_ap_1()
float ap2; // gia tri của ham_doc_ap_2()
float dong1; // gia tri của ham_doc_dong_1()
float dong2;// gia tri của ham_doc_dong_2()
float nhietdo1; // gia tri của ham_doc_NTC1()
float nhietdo2;// gia tri của ham_doc_NTC2()
int buttonStateBT1 = 0;         // Trạng thái của nút nhấn hiện tại
int lastButtonStateBT1 = 1;     // Trạng thái trước đó của nút nhấn (bắt đầu ở HIGH do điện trở kéo lên)
int buttonStateBT2 = 0;         // Trạng thái của nút nhấn hiện tại
int lastButtonStateBT2 = 1;     // Trạng thái trước đó của nút nhấn (bắt đầu ở HIGH do điện trở kéo lên)
int currentScreen = 0;       // Biến để lưu trạng thái màn hình hiện tại
float last_ap1 = -9999, last_ap2 = -9999; // Giá trị cũ của điện áp
float last_dong1 = -9999, last_dong2 = -9999; // Giá trị cũ của dòng điện
float last_nhietdo1 = -9999, last_nhietdo2 = -9999; // Giá trị cũ của nhiệt độ
int cycle1 = 0;
int cycle2 = 0;
// Các biến theo dõi trạng thái
bool charging = false;
bool discharging = false;
bool errorState = false;
bool charging2 = false;
bool discharging2 = false;
bool errorState2 = false;
bool systemRunning = false; // Quản lý trạng thái bật tắt hệ thống

// Non-blocking delay function
unsigned long lastUpdateMillis = 0;
unsigned long lastUpdateMillisADC=0;
unsigned long lastUpdateMillisCOM = 0;
unsigned long lastUpdateMillisCharge =0;
const long updateIntervalLCD = 1500;
const long updateIntervalCOM = 1000; //// biến thời gian để chạy 1 giây 1 lần
const long updateIntervalADC = 500;  // Interval for ADC readings (500 ms)
const long updateIntervalCharge = 60000;  // Interval for ADC readings (60s)
// put function definitions here:
// đọc áp từ tranducer 1
void ham_doc_ap_1(){
///ghi code vao đây
int samples3[NUMSAMPLES];
      uint8_t i2;
      float average_value3;
     // take NUMSAMPLES samples in a row, with a slight delay
      for (i2=0; i2 < NUMSAMPLES; i2++)
         {
            samples3[i2]= analogRead(volt_sensor_1); 
            delay(2);
         }
      
    // average all the samples out
          average_value3 = 0;
      for (i2=0; i2< NUMSAMPLES; i2++) 
        {
           average_value3 += samples3[i2];
        }
          average_value3 /= NUMSAMPLES;
// Ta sẽ đọc giá trị của cảm biến được arduino số hóa trong khoảng từ 0-1023 
//Giá trị được số hóa thành 1 số nguyên có giá trị trong khoảng từ 0 đến 1023 tuong ứng 0-5V 
float v11 = ((float)average_value3/1023)*5;
if(v11<=0.05){ap1=0;} // nhỏ hơn 1V thì =0 hết
else{
  ap1 = v11*2*10 ; // *2*10
}
      /*
    Serial.print("Volt 1 (V):");
    Serial.print(ap1);
    Serial.print(",");
    */
}

// đọc áp từ tranducer 2
void ham_doc_ap_2(){
///ghi code vao đây
int samples2[NUMSAMPLES];
      uint8_t l;
      float average_value2;
      // take NUMSAMPLES samples in a row, with a slight delay
      for (l=0; l< NUMSAMPLES; l++)
         {
            samples2[l] = analogRead(volt_sensor_2);
            delay(2);
         }
     
    // average all the samples out
          average_value2 = 0;
      for (l=0; l< NUMSAMPLES; l++) 
        {
           average_value2 += samples2[l];
        }
          average_value2 /= NUMSAMPLES;
float v21 = ((float)average_value2/1023)*5;
if(v21<= 0.05){ap2=0;} // nhỏ hơn 1V thì =0 hết
else{
    ap2 = v21*2*10 ; // *2*10
}
    /*
    Serial.print("Volt 2 (V):");
    Serial.print(ap2);
    Serial.print(",");
    */
}
// đọc dòng từ current sensor 
void ham_doc_dong_1(){
///ghi code vao đây
int samples[NUMSAMPLES];
      uint8_t j;
      float average_value;
    
      // take UMSAMPLES samples in a row, with a slight delay
      for (j=0; j< NUMSAMPLES; j++)
         {
            samples[j] = analogRead(current_sensor_1);
            delay(2);
         }
      
    // average all the samples out
          average_value = 0;
      for (j=0; j< NUMSAMPLES; j++) 
        {
           average_value += samples[j];
        }
          average_value /= NUMSAMPLES;
     // Ta sẽ đọc giá trị của cảm biến được arduino số hóa trong khoảng từ 0-1023
 float voltage =((float)average_value/1023)*5;   // Giá trị được số hóa thành 1 số nguyên có giá trị trong khoảng từ 0 đến 1023 tuong ứng 0-5V
 if(voltage<=2.748){dong1=0;} // Giá trị được số hóa thành 1 số nguyên có giá trị trong khoảng từ 0 đến 1023 tuong ứng 0-5V
 else{
   dong1 = voltage ; // Bây giờ ta chỉ cần tính ra giá trị dòng điện, 0.1 là độ nhạy của sensor 20A (100mV/A) ,2.5V tương ứng là 0A,VOM dothuc te: 2.746
   dong1 = (dong1-2.748)/0.1;
 }

}
// đọc dòng từ current sensor 
void ham_doc_dong_2(){
  ///ghi code vao đây
  int samples1[NUMSAMPLES];
      uint8_t k;
      float average_value1;
     
      // take NUMSAMPLES samples in a row, with a slight delay
      for (k = 0; k < NUMSAMPLES; k++)
         {
            samples1[k] = analogRead(current_sensor_2);   
                        delay(2);
         }
     
    // average all the samples out
          average_value1 = 0;
      for (k= 0; k < NUMSAMPLES; k++) 
        {
           average_value1 += samples1[k];
        }
          average_value1 /= NUMSAMPLES;
     
  float voltage1 =((float)average_value1/1023)*5;   // Giá trị được số hóa thành 1 số nguyên có giá trị trong khoảng từ 0 đến 1023 tuong ứng 0-5V
   
   if(voltage1<=2.748){dong2=0;} // đo thực tế 0A tương ứng 2.746V
   else{
    dong2 = voltage1 ; // Bây giờ ta chỉ cần tính ra giá trị dòng điện, 0.1 là độ nhạy của sensor 20A (100mV/A) ,2.5V tương ứng là 0A,VOM đo thuc te 2.748V
    dong2 = (dong2-2.748)/0.1;
    }
      
}

void ham_doc_NTC1(){
long total1 =0;
  for( int n1 =0;n1<5;n1++){
     total1 += analogRead(temp_1);
     delay(5);
  }
  float analogvalue = total1/5;
  float resistance = (1023.0 / analogvalue - 1) * SERIESRESISTOR; // Tính điện trở NTC
  // Chuyển đổi điện trở thành nhiệt độ (độ Celsius)
  float temperature = 1 / (log(resistance / THERMISTORNOMINAL) / BCOEFFICIENT + 1 / (TEMPERATURENOMINAL + 273.15)) - 273.15;
  nhietdo1= temperature;
}

void ham_doc_NTC2(){
  //ghi code vào đây
// doc nhiet do NTC 10kohm so 2
  long total2 =0;
  for( int n2 =0;n2<5;n2++){
     total2 += analogRead(temp_2);
     delay(5);
  }
   float analogvalue1 = total2/5;
   float average1 = ((1023/analogvalue1) - 1.0)*SERIESRESISTOR; 
  float steinhart1;
  steinhart1 = average1 / THERMISTORNOMINAL;     // (R/Ro)
  steinhart1 = log(steinhart1);                  // ln(R/Ro)
  steinhart1 /= BCOEFFICIENT;                   // 1/B * ln(R/Ro)
  steinhart1 += 1.0 / (TEMPERATURENOMINAL + 273.15); // + (1/To)
  steinhart1 = 1.0 / steinhart1;                 // Invert
  steinhart1 -= 273.15;                        // convert absolute temp to C
  nhietdo2=steinhart1;
}

// Hàm hiển thị điện áp
void hien_thi_dien_ap1() {
    if (ap1 != last_ap1) {  
        lcd.setCursor(0, 1);
        lcd.print("   V1: ");
        lcd.print(ap1);
        lcd.print("V     ");  // In thêm khoảng trắng để xóa ký tự cũ
        last_ap1 = ap1;  // Cập nhật giá trị mới
    }
}
void hien_thi_dien_ap2() {
    if (ap2 != last_ap2) {  
      
        lcd.setCursor(0, 1);
        lcd.print("   V2: ");
        lcd.print(ap2);
        lcd.print("V     ");  // In thêm khoảng trắng để xóa ký tự cũ
        last_ap2 = ap2;  // Cập nhật giá trị mới
    }
}

// Hàm hiển thị dòng điện
void hien_thi_dong_dien1() {
    if (dong1 != last_dong1) {  
        lcd.setCursor(0, 1);
        lcd.print("   A1: ");
        lcd.print(dong1);
        lcd.print("A    ");  // In thêm khoảng trắng để xóa ký tự cũ
        last_dong1 = dong1;  // Cập nhật giá trị mới
    }
}
void hien_thi_dong_dien2() {
    if (dong2 != last_dong2) { 
        lcd.setCursor(0, 1);
        lcd.print("   A2: ");
        lcd.print(dong2);
        lcd.print("A    ");  // In thêm khoảng trắng để xóa ký tự cũ
        last_dong2 = dong2;  // Cập nhật giá trị mới
    }
}
// Hàm hiển thị nhiệt độ
void hien_thi_nhiet_do1() {
 
    if (nhietdo1 != last_nhietdo1) {  
        lcd.setCursor(0, 1);
        lcd.print("   T1:");
        lcd.print(nhietdo1);
        lcd.print(" C    ");  // In thêm khoảng trắng để xóa ký tự cũ
        last_nhietdo1 = nhietdo1;  // Cập nhật giá trị mới
    }

}
void hien_thi_nhiet_do2() {
    if (nhietdo2 != last_nhietdo2) { 
        lcd.setCursor(0, 1);
        lcd.print("   T2:");
        lcd.print(nhietdo2);
        lcd.print(" C    ");
        last_nhietdo2 = nhietdo2;  // Cập nhật giá trị mới
    }
}
void hien_thi_cycle1(){
    lcd.setCursor(0, 1);
    lcd.print("  Cycle P1: ");
    lcd.print(cycle1);
    lcd.print("      ");

}
void hien_thi_cycle2(){
    lcd.setCursor(0, 1);
    lcd.print("  Cycle P2: ");
    lcd.print(cycle2);
    lcd.print("      ");

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
            hien_thi_dien_ap2();  // Màn hình điện áp
            break;
        case 4:
             hien_thi_dong_dien2();  // Màn hình dòng điện
            break;   
        case 5:
           
            hien_thi_nhiet_do2();  // Màn hình nhiệt độ
            break; 
        case 6:
           
            hien_thi_cycle1();  // Màn hình cycle P1
            break;
        case 7:
           
            hien_thi_cycle2();  // Màn hình cycle P2
            break;
        default:
              
            break;
    }
}
void showScreen(int screen) {
    if (screen == 0) {
     
    lcd.setCursor(0, 1);
    lcd.print("   V1: ");
        lcd.print(ap1);
        lcd.print("V     ");  // In thêm khoảng trắng để xóa ký tự cũ
  } else if (screen == 1) {
       lcd.setCursor(0, 1);
       lcd.print("   A1: ");
        lcd.print(dong1);
        lcd.print("A    ");  // In thêm khoảng trắng để xóa ký tự cũ
  } else if (screen == 2) {
    
    lcd.setCursor(0, 1);
      lcd.print("   T1: ");
        lcd.print(nhietdo1);
        lcd.print(" C    ");  // In thêm khoảng trắng để xóa ký tự cũ
   
      } else if (screen == 3) {
   
    lcd.setCursor(0, 1);
    lcd.print("   V2: ");
        lcd.print(ap2);
        lcd.print("V     ");  // In thêm khoảng trắng để xóa ký tự cũ
  } else if (screen == 4) {
 
    lcd.setCursor(0, 1);
   
    lcd.print("   A2: ");
        lcd.print(dong2);
        lcd.print("A    ");  // In thêm khoảng trắng để xóa ký tự cũ
  } else if (screen == 5) {
     
    lcd.setCursor(0, 1);
    lcd.print("   T2: ");
        lcd.print(nhietdo2);
        lcd.print(" C    ");

  } else if (screen == 6) {
     
    lcd.setCursor(0, 1);
    lcd.print("  Cycle P1: ");
    lcd.print(cycle1);
    lcd.print("      ");

  }else if (screen == 7) {
     
    lcd.setCursor(0, 1);
    lcd.print("  Cycle P2: ");
    lcd.print(cycle2);
    lcd.print("      ");
  
    }

}
//ham bat dau sac
void startCharging1() {
  if (ap1 < 41.6 && nhietdo1 < 45) {
   // lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("   Charging...   ");
    digitalWrite(relay_charge_adaptor_1,HIGH);
    
    lcd.print("   Charging...   ");
    charging = true;
    discharging = false;
    
} else {
    lcd.setCursor(0, 0);
    lcd.print("Charging Failed");
    errorState =true;
  }
}
// Hàm bắt đầu xả
void startDischarging1() {
  //lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" DisCharging...");
  digitalWrite(relay_charge_adaptor_1,LOW);
  digitalWrite(discharge_1,HIGH);
  charging = false;
  discharging = true;
}
//ham bat dau sac 2
void startCharging2() {
  if (ap2 < 41.6 && nhietdo2 < 45) {
   // lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("   Charging...   ");
    digitalWrite(relay_charge_adaptor_2,HIGH);
   
    charging2 = true;
    discharging2 = false;
   
} else {
    lcd.setCursor(0, 0);
    lcd.print("Charging Failed");
    errorState2 =true;
  }
  }
// Hàm bắt đầu xả 2
void startDischarging2() {
  //lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" DisCharging...");
  digitalWrite(relay_charge_adaptor_2,LOW);
  digitalWrite(relay_5,HIGH);
  charging2 = false;
  discharging2 = true;
}
// Hàm dừng sạc và hệ thống
void stopSystem() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" System Stopped");
  digitalWrite(discharge_1,LOW);
  digitalWrite(relay_5,LOW);
  digitalWrite(relay_charge_adaptor_1,LOW);
  digitalWrite(relay_charge_adaptor_2,LOW);
  charging = false;
  discharging = false;
  systemRunning = false;
}
void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600); // open port 9600
  /*
  //code cho LCD I2C
  lcd.init();    //Khởi động màn hình LCD                
  lcd.backlight();//Bật đèn nền LCD 16×2.
  
  */
  lcd.begin(16, 2);// set up the LCD's number of columns and rows:
  lcd.clear(); // clear man hình
  // Print a message to the LCD.
  lcd.print(" Press BT1 start");
  pinMode(current_sensor_1,INPUT);
  pinMode(current_sensor_2,INPUT);
  pinMode(volt_sensor_1,INPUT);
  pinMode(volt_sensor_2,INPUT);
  pinMode(temp_1,INPUT);
  pinMode(temp_2,INPUT);
  pinMode(relay_charge_adaptor_1,OUTPUT);
  pinMode(relay_charge_adaptor_2,OUTPUT);
  pinMode(bat_tat,INPUT);
  pinMode(enter,INPUT);
  pinMode(discharge_1,OUTPUT);
  pinMode(relay_5,OUTPUT);
  // lúc đầu tắt hết output sạc xả
   digitalWrite(relay_charge_adaptor_1,LOW);
   digitalWrite(relay_charge_adaptor_2,LOW);
   digitalWrite(discharge_1,LOW);
   digitalWrite(relay_5,LOW);
 
}

void loop() {
  // put your main code here, to run repeatedly:
   
    buttonStateBT1 = digitalRead(bat_tat);  // Đọc trạng thái nút nhấn
    buttonStateBT2 = digitalRead(enter);
      
  // Nếu nút được nhấn và trạng thái khác lần nhấn trước (chống rung tín hiệu)
  if (buttonStateBT1 == LOW && lastButtonStateBT1 == HIGH) {
    delay(50);  // Đợi một chút để tránh việc đọc nhiều lần (chống dội phím)
    // Thay đổi trạng thái hệ thống
    if (!systemRunning) {
      systemRunning = true;  // Bật hệ thống
      startCharging1();       // Bắt đầu sạc
      startCharging2();       // Bắt đầu sạc
    } else {
      stopSystem();          // Tắt toàn bộ hệ thống
    }
  }

    if (systemRunning) {
       // charge 1
    if (charging) {
      if (ap1 >= 40.9 && dong1 <= 0.09) {
        if (nhietdo1 <= 37) {
          startDischarging1();  // Điều kiện để chuyển qua xả
        } else {
          lcd.setCursor(0, 0);
          lcd.print("Temp too high!");
        }
      }
    }
   
    if (discharging) {
      if (ap1 <= 20) {
        if (nhietdo1 < 47) {
          startCharging1();  // Điều kiện để quay lại sạc
          cycle1 ++ ;
        } else {
          lcd.setCursor(0, 0);
          lcd.print("Temp too high!");
          
        }
      }
    }
     // charge 2
    if (charging2) {
      if (ap2 >= 40.9 && dong2 <= 0.09) {
        if (nhietdo2 <= 37) {
          startDischarging2();  // Điều kiện để chuyển qua xả
        } else {
          lcd.setCursor(0, 0);
          lcd.print("Temp2 too high!");
        }
      }
    }

    if (discharging2) {
      if (ap2 <= 20) {
        if (nhietdo2 < 47) {
          startCharging2();  // Điều kiện để quay lại sạc
          cycle2 ++ ;
        } else {
          lcd.setCursor(0, 0);
          lcd.print("Temp2 too high!");
          
        }
      }
    }
  }

  if (buttonStateBT2 == LOW && lastButtonStateBT2 == HIGH) {
    delay(50);  // Tránh hiện tượng dội phím
           
    if (currentScreen > 7) {
      
      currentScreen = 0; // Quay lại màn hình đầu tiên nếu vượt quá số lượng màn hình
      
    }
      
    showScreen(currentScreen);  // Hiển thị thông tin tương ứng với màn hình
    currentScreen++;    // Chuyển sang màn hình tiếp theo
  }
  // Cập nhật nội dung màn hình mỗi updateInterval
    if (millis() - lastUpdateMillis >= updateIntervalLCD) {
        lastUpdateMillis = millis();
    updatescreen(currentScreen);

  }
  // Cập nhật ADC read mỗi updateInterval
    if (millis() - lastUpdateMillisADC >= updateIntervalADC) {
        lastUpdateMillisADC = millis();
          ham_doc_ap_1();
          ham_doc_dong_1();
          ham_doc_NTC1();
          ham_doc_ap_2();
          ham_doc_dong_2();
          ham_doc_NTC2();
  
  }
  lastButtonStateBT2 = buttonStateBT2;  // Cập nhật trạng thái của nút nhấn
  lastButtonStateBT1 = buttonStateBT1;  // Cập nhật trạng thái nút nhấn
  
  if(Serial.available()>0){
  if (millis() - lastUpdateMillisCOM >= updateIntervalCOM) {
    lastUpdateMillisCOM = millis();
      // cycle
    Serial.print("Cycle pack 1: ");
    Serial.print(cycle1);
    Serial.print(",");
    Serial.print("Cycle pack 2: ");
    Serial.print(cycle2);
    Serial.print(",");
   //ham_doc_ap_1();
    Serial.print("Volt 1 (V):");
    Serial.print(ap1);
    Serial.print(",");
  // ham_doc_ap_2();
    Serial.print("Volt 2 (V):");
    Serial.print(ap2);
    Serial.print(",");
   //ham_doc_dong_1();
   Serial.print("Current 1 (A):");
   Serial.print(dong1);
   Serial.print(",");
  // ham_doc_dong_2();
   Serial.print("Current 2 (A):");
   Serial.print(dong2);
   Serial.print(",");
   //ham_doc_NTC1();
   Serial.print("Nhiet do 1 (C):");
   Serial.print(nhietdo1);
   Serial.print(",");
   //ham_doc_NTC2();
   Serial.print("Nhiet do 2 (C):");
   Serial.print(nhietdo2);
   Serial.print(",");
   Serial.println("");
 }
    
    }
    

}
