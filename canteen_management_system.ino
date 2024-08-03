#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>
#include "BluetoothSerial.h"

#define EEPROM_SIZE 200

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

BluetoothSerial SerialBT;


#define RST_PIN 15
#define SS_PIN 5

byte readCard[4];

MFRC522 mfrc522(SS_PIN, RST_PIN);


#define ROW_NUM     4 // four rows
#define COLUMN_NUM  4 // four columns
char keys[ROW_NUM][COLUMN_NUM] = {
  {'F', '1', '2', '3'},
  {'G', '4', '5', '6'},
  {'H', '7', '8', '9'},
  {'I', 'C', '0', 'E'}
};

String RF_Cards[] = {"000000","43955331","102A51A", "C163FA24"};
String current_contact;
String tagID = "";
// String current_card;
int current_user_index = 9;


byte pin_rows[ROW_NUM]      = {32,33,25,26}; // GPIO19, GPIO18, GPIO5, GPIO17 connect to the row pins
byte pin_column[COLUMN_NUM] = {27,14,12,13};   // GPIO16, GPIO4, GPIO0, GPIO2 connect to the column pins
Keypad keypad = Keypad( makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM );

LiquidCrystal_I2C lcd(0x27,16,2);  // set the LCD address to 0x3F for a 16 chars and 2 line display

char keypressed;
int current_serial = 0;
int current_qty = 0;
int buzzer = 2;

int max_dishes = 2;
int cart_dishes[] = {0,0,0,0,0};
int cart_qty[] = {0,0,0,0,0};

String dishes[] = {"None","Idly Set", "Plain Dosa", "Fried Rice", "Biriyani", "Vadaa", "Poori","Pongal"};
int prices[] = {0,20, 40, 80, 110, 8, 30, 25};



void setup() {
  Serial.begin(115200);
  SerialBT.begin("SmartCanteen");
  Serial.println("The device started, now you can pair it with bluetooth!");
  EEPROM.begin(EEPROM_SIZE);
  SPI.begin(); // SPI bus
  mfrc522.PCD_Init(); // MFRC522
  pinMode(buzzer, OUTPUT);
  lcd.init();
  lcd.clear();         
  lcd.backlight();      // Make sure backlight is on
  lcd.setCursor(2,0);
  lcd.print("Smart Payment");
  lcd.setCursor(4,1);
  lcd.print("Gateway");
  digitalWrite(buzzer, HIGH);
  delay(2000);
  digitalWrite(buzzer, LOW);
  delay(500);
  lcd.clear();  
  Serial.println("Smart Payment Gateway");       
}


void loop() {
  current_user_index = 9;
  lcd.setCursor(0,0);
  lcd.print("Press F1 to");
  lcd.setCursor(0,1);
  lcd.print("Start Order");
  keypressed = keypad.getKey();
  if (keypressed == 'F'){start_order();}
  if (keypressed == 'G'){register_card();}
  if (keypressed == 'H'){topup_card();}
}

void topup_card(){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Place your Card:");
  while (current_user_index == 9){
    read_card();
  }
  buzzer_tone();
  update_balance(current_user_index, 2000);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Balance set:");
  lcd.setCursor(0,1);
  lcd.print("Rs. 2000");
  buzzer_tone();
  delay (3000);
}

void start_order(){
  clear_cart();
  Update_Cart:
  add_item();
  lcd.clear();
  lcd.setCursor(0, 0);
  if (get_array_length() < max_dishes){lcd.print("F1 to Add Items");}
  else{lcd.print("Max. Reached");}
  lcd.setCursor(0, 1);
  lcd.print("F2 for Checkout");
  bool NO_KEY = true;
  keypressed = keypad.getKey();
  while(NO_KEY == true)
    {
      if(keypressed == 'F' && get_array_length() < max_dishes  ){goto   Update_Cart;NO_KEY = false; }
      if(keypressed == 'G'){Serial.println("Going to Check Out"); checkout(); NO_KEY = false;}
      keypressed = keypad.getKey();
      delay(10);
    }
}


void checkout(){
  current_user_index = 9;
  int total_amt = get_total_cart_value();
  Serial.print("Total Cart Value:");
  Serial.println(total_amt);
  buzzer_tone();
  delay(10);
  lcd.clear();
  delay(10);
  lcd.setCursor(0,0);
  lcd.print("Place your Card");
  lcd.setCursor(0,1);
  lcd.print("Total :");
  lcd.setCursor(8,1);
  lcd.print("Rs.");
  lcd.setCursor(12,1);
  lcd.print(total_amt); 
    while (current_user_index == 9){
    read_card();
  }
  int current_bal = get_balance(current_user_index);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Mob:  ");
  lcd.setCursor(5,0);
  lcd.print(current_contact);
  lcd.setCursor(0,1);
  lcd.print("Cur Bal:");
  lcd.setCursor(9,1);
  lcd.print("Rs.");
  lcd.setCursor(12,1);
  lcd.print(current_bal);
  int ending_balance = (current_bal - total_amt) ;
  bool NO_KEY = true;
  keypressed = keypad.getKey();
  while(NO_KEY == true)
    {
        if(keypressed == 'E')
          {
            update_balance(current_user_index, ending_balance);
            buzzer_tone();
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print("Successful");
            lcd.setCursor(0,1);
            lcd.print("Bal Rs.");
            lcd.setCursor(8,1);
            lcd.print(ending_balance);
            NO_KEY = false;
            digitalWrite(buzzer, HIGH);
            delay(2000);
            digitalWrite(buzzer, LOW);
            delay(2000);
          }
          keypressed = keypad.getKey();
    }
    
    print_cart();

  current_user_index = 9;
  lcd.clear();
}

void print_cart(){
      SerialBT.println("New Order Received !");
      SerialBT.print("Mobile Number: ");
      SerialBT.println(current_contact);



      Serial.println("New Order Received !");
      Serial.print("Mobile Number: ");
      Serial.println(current_contact);
      for(int j = 0; j < max_dishes; j++)
      {
        int current_dish_price  =  prices[cart_dishes[j]];
        int current_dish_total_amt = current_dish_price * cart_qty[j];

        SerialBT.print("Dish : ");
        SerialBT.print(dishes[cart_dishes[j]]);
        SerialBT.print(" : Qty. :");
      SerialBT.println(cart_qty[j]);

        Serial.print("Dish : ");
        Serial.print(dishes[cart_dishes[j]]);
        Serial.print(" : Qty. :");
        Serial.println(cart_qty[j]);
      }
      SerialBT.println("*******************************");
      Serial.println("*******************************");

}

void clear_cart(){
      for(int j = 0; j < max_dishes; j++)
      {

        cart_dishes[j] = 0;
        cart_qty[j] = 0;
      }

}

int get_balance(int userIndex){
  int EEPROM_Addr = 100+(2*userIndex);
  int value = 0;
  byte highByteValue = EEPROM.read(EEPROM_Addr);
  EEPROM_Addr = EEPROM_Addr + 1;
  byte lowByteValue = EEPROM.read(EEPROM_Addr);
  value = word(highByteValue, lowByteValue);
  return value;
}

void update_balance(int userIndex, int amount ){
  int EEPROM_Addr = 100+(2*userIndex);
  EEPROM.write(EEPROM_Addr, highByte(amount));
  EEPROM.commit();
  delay (10);
  EEPROM_Addr = EEPROM_Addr + 1;
  EEPROM.write(EEPROM_Addr, lowByte(amount));
  EEPROM.commit();
  delay(10);
}




void read_card(){
  current_contact = "0000";
    while (getID()) 
    {
      for (int k = 0; k <= ((sizeof(RF_Cards))/4); k++) 
        {
          if (RF_Cards[k] == tagID){current_user_index = k; break; }
          delay(10);
        }
        current_contact = readStringFromEEPROM(current_user_index*20, 11);
        // Serial.print(" User Index is:");
        // Serial.print(current_user_index);
        // Serial.print("; UID is: ");
        // Serial.println(RF_Cards[current_user_index]);
        // Serial.print("Mobile: ");
        // Serial.println(current_contact);
    }
    return;
}

void register_card(){
  buzzer_tone();
  current_user_index = 9;
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print ("Registering :");
  lcd.setCursor(0,1);
  lcd.print("Place Your Card");
  while (current_user_index == 9){
    read_card();
  }
  delay(20);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Enter Mobile No.");
  char number_array[] = "FFFFFFFFFF";
  for (int m=0; m<=9;){
    bool NO_KEY = true;
    keypressed = keypad.getKey();
    if(keypressed == '1' || keypressed == '2' || keypressed == '3'|| keypressed == '4'|| keypressed == '5'|| keypressed == '6'|| keypressed == '7'|| keypressed == '8'|| keypressed == '9' || keypressed == '0')
    {lcd.setCursor(m,1);lcd.print(keypressed);buzzer_tone(); m = m+1; delay (10); number_array[m]= keypressed;}
  }
  if (current_user_index != 9){ 
    String mobile_number = number_array;
    writeStringToEEPROM(current_user_index*20,mobile_number);
    current_user_index = 9;
  }
  lcd.clear();
  return;
}


int get_total_cart_value(){
      int total_cart_value = 0;
      for(int j = 0; j < max_dishes; j++)
      {
        int current_dish_price  =  prices[cart_dishes[j]];
        int current_dish_total_amt = current_dish_price * cart_qty[j];
        total_cart_value = total_cart_value + current_dish_total_amt;
      }
      return (total_cart_value);
}




void add_item(){
  current_qty = 0;
  current_serial = 0;
  buzzer_tone(); lcd.clear(); lcd.setCursor(0,0); 
  lcd.print("Enter Dish No.");
  boolean NO_KEY = true;
  keypressed = keypad.getKey();
  while(NO_KEY == true)
    {
      if(keypressed == '1' || keypressed == '2' || keypressed == '3'|| keypressed == '4'|| keypressed == '5'|| keypressed == '6'|| keypressed == '7'|| keypressed == '8'|| keypressed == '9')
      { buzzer_tone(); if (keypressed == '1'){current_serial = 1;} if (keypressed == '2'){current_serial = 2;} if (keypressed == '3'){current_serial = 3;}if (keypressed == '4'){current_serial = 4;}if (keypressed == '5'){current_serial = 5;}if (keypressed == '6'){current_serial = 6;}if (keypressed == '7'){current_serial = 7;}if (keypressed == '8'){current_serial = 8;}
        NO_KEY = false;
      }
      keypressed = keypad.getKey();
      delay(10);
    }  
  lcd.clear(); lcd.setCursor(0,0); lcd.print(dishes[current_serial]); lcd.setCursor(11,0); lcd.print("Rs"); lcd.print(prices[current_serial]); lcd.setCursor(0,1);lcd.print("Enter Qty. :");
  NO_KEY = true;
  keypressed = keypad.getKey();
  while(NO_KEY == true)
    {
      if(keypressed == '1' || keypressed == '2' || keypressed == '3'|| keypressed == '4'|| keypressed == '5'|| keypressed == '6'|| keypressed == '7'|| keypressed == '8'|| keypressed == '9')
          { buzzer_tone(); lcd.setCursor(14,1); lcd.print(keypressed); if (keypressed == '1'){current_qty = 1;} if (keypressed == '2'){current_qty = 2;} if (keypressed == '3'){current_qty = 3;}if (keypressed == '4'){current_qty = 4;}
            if (keypressed == '5'){current_qty = 5;}if (keypressed == '6'){current_qty = 6;}if (keypressed == '7'){current_qty = 7;}if (keypressed == '8'){current_qty = 8;}if (keypressed == '9'){current_qty = 9;}
          }
        if(keypressed == 'E' && current_qty != 0)
          {
            buzzer_tone();
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print("Added:");
            lcd.print(dishes[current_serial]);
            update_cart(current_serial,current_qty);
            NO_KEY = false;
          }
          keypressed = keypad.getKey();
    }
}

void update_cart(int dish_id, int dish_qty){
    int array_length = get_array_length();
    int i = array_length;
    cart_dishes[i] = dish_id;
    cart_qty[i] = dish_qty;
    for(int j = 0; j < max_dishes; j++)
      {
        int current_dish_price  =  prices[cart_dishes[j]];
        int current_dish_total_amt = current_dish_price * cart_qty[j];

        // Serial.print("Dish : ");
        // Serial.print(dishes[cart_dishes[j]]);
        // Serial.print(" : Qty. :");
        // Serial.print(cart_qty[j]);
        // Serial.print("Amount : ");
        // Serial.println(current_dish_total_amt);
      }
      // Serial.println("*************************");
    return;
}


int get_array_length(){
  int current_array_length = 0;
  for (int i = 0; i <= ((sizeof(cart_qty))/4); i++) 
    {
      if (cart_dishes[i] != 0){current_array_length  = current_array_length+1 ; }
      else {break;}
    }
    return(current_array_length);
}



void enter_mobile(){
  Serial.println("Enter a mobile number: ");
  int i;
  for (i=0; i<10; i++) {
  char keypressed = keypad.getKey();
    if (keypressed == 'C'){
      Serial.println("Clearing Number");
    }
    if (keypressed != NO_KEY) {
      Serial.print(keypressed);
      digitalWrite(buzzer, HIGH);
      delay(250);
      digitalWrite(buzzer,LOW);
      delay(10);
    }
  }
}

boolean getID() 
{
  // Getting ready for Reading PICCs
  if ( ! mfrc522.PICC_IsNewCardPresent()) { //If a new PICC placed to RFID reader continue
  return false;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) { //Since a PICC placed get Serial and continue
  return false;
  }
  tagID = "";
  for ( uint8_t i = 0; i < 4; i++) { // The MIFARE PICCs that we use have 4 byte UID
  //readCard[i] = mfrc522.uid.uidByte[i];
  tagID.concat(String(mfrc522.uid.uidByte[i], HEX)); // Adds the 4 bytes in a single String variable
  }
  tagID.toUpperCase();
  mfrc522.PICC_HaltA(); // Stop reading
  return true;
}

void buzzer_tone(){digitalWrite(buzzer, HIGH);delay(200);digitalWrite(buzzer, LOW);delay(50);}



void writeStringToEEPROM(int addrOffset, const String &strToWrite)
{
  byte len = strToWrite.length();
  EEPROM.write(addrOffset, len);
  Serial.print("Mobile Number Updated :");
  Serial.println(strToWrite);
  for (int i = 1; i <= 10; i++)
  {
    EEPROM.write(addrOffset  + i, strToWrite[i]);
    EEPROM.commit();
  }
}

String readStringFromEEPROM(int addrOffset, int no_of_digits)
{
  int newStrLen = EEPROM.read(addrOffset);
  char data[no_of_digits];
  for (int i = 0; i < no_of_digits; i++)
  {
    data[i] = EEPROM.read(addrOffset + i);
  }
  data[newStrLen] = '\ 0'; 
  return String(data);
}
