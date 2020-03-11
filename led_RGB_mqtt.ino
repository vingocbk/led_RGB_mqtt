#include "led_RGB_mqtt.h"

void getStatus(){
    String datasend = "{\"deviceid\" : \"";
    datasend += String(deviceId);
    datasend += "\", \"devicetype\" : \"ledRgb\", \"typecontrol\" : \"getstatus\",  \"number_disconnect\" : \"";
    datasend += String(countDisconnectToServer - 1);
    datasend += "\", \"all_time\" : \"";
    datasend += String(sum_time_disconnect_to_sever);
    datasend += "\", \"red\" : \"";
    datasend += String(red_before);
    datasend += "\", \"green\" : \"";
    datasend += String(green_before);
    datasend += "\", \"blue\" : \"";
    datasend += String(blue_before);
    datasend += "\", \"alpha\" : \"";
    datasend += String(intensityLight);
    datasend +=  "\", \"status\" : \"";
    if(flag_led == true){
        datasend += "on\"}";
        client.publish(topicsendStatus, datasend.c_str());
    }else{
        datasend += "off\"}";
        client.publish(topicsendStatus, datasend.c_str());
    }
    ECHOLN("-------getStatus-------");
}

void SendStatusReconnect(){
    const char* willTopic = "CabinetAvy/HPT/LWT";
    String ReconnectMessage = "{\"devicetype\" : \"";
    ReconnectMessage += m_Typedevice;
    ReconnectMessage += "\", \"deviceid\" : \"";
    ReconnectMessage += String(deviceId);
    ReconnectMessage += "\", \"status\" : \"ok\"}";
    client.publish(willTopic, ReconnectMessage.c_str());
    ECHOLN("-------Reconnect-------");
}



void clearEeprom(){
    ECHOLN("clearing eeprom");
    for (int i = 0; i < EEPROM_WIFI_MAX_CLEAR; ++i){
        EEPROM.write(i, 0);
    }
    EEPROM.commit();
    ECHOLN("Done writing!");
}

void ConfigMode(){
    StaticJsonBuffer<RESPONSE_LENGTH> jsonBuffer;
    ECHOLN(server.arg("plain"));
    JsonObject& rootData = jsonBuffer.parseObject(server.arg("plain"));
    ECHOLN("--------------");
    tickerSetApMode.stop();
    digitalWrite(LED_TEST, HIGH);
    if (rootData.success()) {
        server.sendHeader("Access-Control-Allow-Headers", "*");
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(200, "application/json; charset=utf-8", "{\"status\":\"success\"}");
        //server.stop();
        String nssid = rootData["ssid"];
        String npass = rootData["password"];
        String nid = rootData["deviceid"];
        String nserver = rootData["server"];

        esid = nssid;
        epass = npass;
        deviceId = nid.toInt();
        serverMqtt = nserver;

        ECHOLN("clearing eeprom");
        for (int i = 0; i <= EEPROM_WIFI_SERVER_END; i++){ 
            EEPROM.write(i, 0); 
        }
        ECHOLN("writing eeprom ssid:");
        ECHO("Wrote: ");
        for (int i = 0; i < nssid.length(); ++i){
            EEPROM.write(i+EEPROM_WIFI_SSID_START, nssid[i]);             
            ECHO(nssid[i]);
        }
        ECHOLN("");
        ECHOLN("writing eeprom pass:"); 
        ECHO("Wrote: ");
        for (int i = 0; i < npass.length(); ++i){
            EEPROM.write(i+EEPROM_WIFI_PASS_START, npass[i]);
            ECHO(npass[i]);
        }
        ECHOLN("");
        ECHOLN("writing eeprom device id:"); 
        ECHO("Wrote: ");
        EEPROM.write(EEPROM_WIFI_DEVICE_ID, deviceId);
        ECHOLN(deviceId);

        ECHOLN("writing eeprom server:"); 
        ECHO("Wrote: ");
        for (int i = 0; i < nserver.length(); ++i){
            EEPROM.write(i+EEPROM_WIFI_SERVER_START, nserver[i]);
            ECHO(nserver[i]);
        }
        ECHOLN("");

        EEPROM.commit();
        ECHOLN("Done writing!");

        if (testWifi(nssid, npass)) {
            ConnecttoMqttServer();
            Flag_Normal_Mode = true;
            return;
        }

        ECHOLN("Wrong wifi!!!");
        SetupConfigMode();
        StartConfigServer();
        return;
    }
    ECHOLN("Wrong data!!!");
}



bool testWifi(String esid, String epass) {
    ECHO("Connecting to: ");
    ECHOLN(esid);
    WiFi.softAPdisconnect();
    WiFi.disconnect();
    server.close();
    // delay(1000);
    ECHO("delay: ");
    ECHOLN((deviceId-1)*3000 + 1000);
    for(int i = 0; i < 100; i++){
        if(digitalRead(PIN_CONFIG) == LOW){
            break;
        }
        delay(((deviceId-1)*3000 + 1000)/100);
    }

    WiFi.mode(WIFI_STA);        //bat che do station
    WiFi.begin(esid.c_str(), epass.c_str());
    int c = 0;
    ECHOLN("Waiting for Wifi to connect");
    while (c < 20) {
        if (WiFi.status() == WL_CONNECTED) {
            ECHOLN("\rWifi connected!");
            ECHO("Local IP: ");
            ECHOLN(WiFi.localIP());
            // digitalWrite(LED_TEST, LOW);
            ConnecttoMqttServer();
            return true;
        }
        delay(500);
        ECHO(".");
        c++;
        if(digitalRead(PIN_CONFIG) == LOW){
            break;
        }
    }
    ECHOLN("");
    ECHOLN("Connect timed out");
    return false;
}

void setLedApMode() {
    digitalWrite(LED_TEST, !digitalRead(LED_TEST));
}



String GetFullSSID() {
    uint8_t mac[WL_MAC_ADDR_LENGTH];
    String macID;
    WiFi.mode(WIFI_AP);
    WiFi.softAPmacAddress(mac);
    macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) + String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
    macID.toUpperCase();
    ECHO("[Helper][getIdentify] Identify: ");
    ECHO(SSID_PRE_AP_MODE);
    ECHOLN(macID);
    return SSID_PRE_AP_MODE + macID;
}

void checkButtonConfigClick(){
    if (digitalRead(PIN_CONFIG) == LOW && (ConfigTimeout + CONFIG_HOLD_TIME) <= millis()) { // Khi an nut
        ConfigTimeout = millis();
        //tickerSetMotor.attach(0.2, setLedApMode);  //every 0.2s
        Flag_Normal_Mode = false;
        tickerSetApMode.start();
        SetupConfigMode();
        StartConfigServer();
    } else if(digitalRead(PIN_CONFIG) == HIGH) {
        ConfigTimeout = millis();
    }
}


void SetupConfigMode(){
    ECHOLN("[WifiService][setupAP] Open AP....");
    WiFi.softAPdisconnect();
    WiFi.disconnect();
    server.close();
    delay(1000);
    WiFi.mode(WIFI_AP_STA);
    IPAddress APIP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(APIP, gateway, subnet);
    String SSID_AP_MODE = GetFullSSID();
    WiFi.softAP(SSID_AP_MODE.c_str(), PASSWORD_AP_MODE);
    ECHOLN(SSID_AP_MODE);

    ECHOLN("[WifiService][setupAP] Softap is running!");
    IPAddress myIP = WiFi.softAPIP();
    ECHO("[WifiService][setupAP] IP address: ");
    ECHOLN(myIP);
}
void StartConfigServer(){    
    ECHOLN("[HttpServerH][startConfigServer] Begin create new server...");
    server.on("/config", HTTP_POST, ConfigMode);
    server.begin();
    ECHOLN("[HttpServerH][startConfigServer] HTTP server started");
}



void SetupNetwork() {
    ECHOLN("Reading EEPROM ssid");
    esid = "";
    for (int i = EEPROM_WIFI_SSID_START; i < EEPROM_WIFI_SSID_END; ++i){
        esid += char(EEPROM.read(i));
    }
    ECHO("SSID: ");
    ECHOLN(esid);
    ECHOLN("Reading EEPROM pass");
    epass = "";
    for (int i = EEPROM_WIFI_PASS_START; i < EEPROM_WIFI_PASS_END; ++i){
        epass += char(EEPROM.read(i));
    }
    ECHO("PASS: ");
    ECHOLN(epass);

    ECHOLN("Reading EEPROM Device ID");
    deviceId = EEPROM.read(EEPROM_WIFI_DEVICE_ID);
    ECHO("ID: ");
    ECHOLN(deviceId);

    ECHOLN("Reading EEPROM server");
    serverMqtt = "";
    for (int i = EEPROM_WIFI_SERVER_START; i < EEPROM_WIFI_SERVER_END; ++i){
        serverMqtt += char(EEPROM.read(i));
    }
    ECHO("SERVER: ");
    ECHOLN(serverMqtt);

    //lay du lieu thong tin den led
    red_before = uint8_t(EEPROM.read(EEPROM_WIFI_LED_RED));
    green_before = uint8_t(EEPROM.read(EEPROM_WIFI_LED_GREEN));
    blue_before = uint8_t(EEPROM.read(EEPROM_WIFI_LED_BLUE));
    intensityLight = uint8_t(EEPROM.read(EEPROM_WIFI_LED_INTENSITY));
    if(intensityLight < 0 || intensityLight > 100){
        intensityLight = 100;
    }
    analogWrite(PIN_LED_RED, red_before);
    analogWrite(PIN_LED_GREEN, green_before);
    analogWrite(PIN_LED_BLUE, blue_before);
    // if(EEPROM.read(EEPROM_WIFI_LED_STATUS_ON_OFF) == 0){
    //     ECHOLN("LED IS OFF!");
    //     flag_led = false;
    //     analogWrite(PIN_LED_RED, 0);
    //     analogWrite(PIN_LED_GREEN, 0);
    //     analogWrite(PIN_LED_BLUE, 0);
    // }else{
    //     ECHOLN("LED IS ON!");
    //     flag_led = true;
    //     analogWrite(PIN_LED_RED, red_before);
    //     analogWrite(PIN_LED_GREEN, green_before);
    //     analogWrite(PIN_LED_BLUE, blue_before);
    // }

    ECHO("LED: ");
    ECHOLN(red_before);
    ECHO("GREEN: ");
    ECHOLN(green_before);
    ECHO("BLUE: ");
    ECHOLN(blue_before);
    ECHO("INTENSITY: ");
    ECHOLN(intensityLight);


    testWifi(esid, epass);
}

void ConnecttoMqttServer(){
    // client.setServer(mqtt_server, MQTT_PORT);
    client.setServer(serverMqtt.c_str(), MQTT_PORT);
    client.setCallback(callbackMqttBroker);
    reconnect();
}


void callbackMqttBroker(char* topic, byte* payload, unsigned int length){
    
    String Topic = String(topic);
    ECHO("TOPIC: ");
    ECHOLN(Topic);

    String data;
    for (int i = 0; i < length; i++) {
        data += char(payload[i]);
    }
    ECHO("DATA: ");
    ECHOLN(data);
    StaticJsonBuffer<RESPONSE_LENGTH> jsonBuffer;
    JsonObject& rootData = jsonBuffer.parseObject(data);

    if(Topic.indexOf(m_Controlhand) > 0){
        if (rootData.success()){
            if(rootData["typedevice"] == m_Typedevice){
                int arraySize = rootData["deviceid"].size();   //get size of JSON Array
                int sensorValue[arraySize];
                bool isTrueControl = false;
                for (int i = 0; i < arraySize; i++) { //Iterate through results
                    sensorValue[i] = rootData["deviceid"][i];  //Implicit cast
                    // ECHOLN(sensorValue[i]);
                    if(sensorValue[i] == deviceId){
                        isTrueControl = true;
                        break;
                    }
                }
                if(isTrueControl == true){
                    String dataType = rootData["typecontrol"];
                    //---------control color------------------
                    if(dataType == "controlled"){
                        timeToSaveData = millis();
                        int controlled[3];
                        for (int i = 0; i < 3; i++) { //Iterate through results
                            controlled[i] = rootData["data"][i];  //Implicit cast
                        }
                        red_after = controlled[0];
                        green_after = controlled[1];
                        blue_after = controlled[2];
                        analogWrite(PIN_LED_RED, uint8_t((float)red_after*intensityLight/100));
                        analogWrite(PIN_LED_GREEN, uint8_t((float)green_after*intensityLight/100));
                        analogWrite(PIN_LED_BLUE, uint8_t((float)blue_after*intensityLight/100));
                        red_before = red_after;
                        green_before = green_after;
                        blue_before = blue_after;

                    }
                    else if(dataType == "changealpha"){
                        timeToSaveData = millis();
                        String intensityLightStr = rootData["data"];
                        intensityLight = intensityLightStr.toInt();

                        float out_led_red, out_led_green, out_led_blue;
                        out_led_red = ((float)red_after*intensityLight)/100;
                        analogWrite(PIN_LED_RED, uint8_t(out_led_red));

                        out_led_green = ((float)green_after*intensityLight)/100;
                        analogWrite(PIN_LED_GREEN, uint8_t(out_led_green));

                        out_led_blue = ((float)blue_after*(float)intensityLight)/100;
                        analogWrite(PIN_LED_BLUE, uint8_t(out_led_blue));
                    }
                }
            }
        }
    }

    else if(Topic.indexOf(m_Controlvoice) > 0){
        if (rootData.success()){
            if(rootData["typedevice"] == m_Typedevice){
                int arraySize = rootData["deviceid"].size();   //get size of JSON Array
                int sensorValue[arraySize];
                bool isTrueControl = false;
                for (int i = 0; i < arraySize; i++) { //Iterate through results
                    sensorValue[i] = rootData["deviceid"][i];  //Implicit cast
                    // ECHOLN(sensorValue[i]);
                    if(sensorValue[i] == deviceId){
                        isTrueControl = true;
                        break;
                    }
                }
                if(isTrueControl == true){
                    float out_led_red, out_led_green, out_led_blue;
                    String dataType = rootData["typecontrol"];
                    //---------getstatus------------------
                    if(dataType == "getstatus"){
                        getStatus();
                    }
                    //---------control------------------
                    else if(dataType == "on"){
                        flag_led = true;
                        for(int i = 1; i<=255; i++){
                            out_led_red = (float)0 + ((float)((float)red_after - (float)0)/255)*i;
                            out_led_red = abs(out_led_red);
                            analogWrite(PIN_LED_RED, uint8_t(out_led_red*intensityLight/100));
                            out_led_green = (float)0 + ((float)((float)green_after - (float)0)/255)*i;
                            out_led_green = abs(out_led_green);
                            analogWrite(PIN_LED_GREEN, uint8_t(out_led_green*intensityLight/100));
                            out_led_blue = (float)0 + ((float)((float)blue_after - (float)0)/255)*i;
                            out_led_blue = abs(out_led_blue);
                            analogWrite(PIN_LED_BLUE, uint8_t(out_led_blue*intensityLight/100));
                            delay(10);
                        }
                        // ECHOLN("Wrote Led on");
                        // EEPROM.write(EEPROM_WIFI_LED_STATUS_ON_OFF, 1);
                        // EEPROM.commit();
                    }
                    else if(dataType == "off"){
                        flag_led = false;
                        for(int i = 1; i<=255; i++){
                            out_led_red = (float)red_before + (((float)0 - (float)red_before)/255)*i;
                            out_led_red = abs(out_led_red);
                            analogWrite(PIN_LED_RED, uint8_t(out_led_red*intensityLight/100));
                            out_led_green = (float)green_before + (((float)0 - (float)green_before)/255)*i;
                            out_led_green = abs(out_led_green);
                            analogWrite(PIN_LED_GREEN, uint8_t(out_led_green*intensityLight/100));
                            out_led_blue = (float)blue_before + (((float)0 - (float)blue_before)/255)*i;
                            out_led_blue = abs(out_led_blue);
                            analogWrite(PIN_LED_BLUE, uint8_t(out_led_blue*intensityLight/100));
                            delay(10);
                        }
                        // ECHOLN("Wrote Led off!");
                        // EEPROM.write(EEPROM_WIFI_LED_STATUS_ON_OFF, 0);
                        // EEPROM.commit();
                    }
                    else if(dataType == "controlled"){
                        int controlled[3];
                        for (int i = 0; i < 3; i++) { //Iterate through results
                            controlled[i] = rootData["data"][i];  //Implicit cast
                        }
                        red_after = controlled[0];
                        green_after = controlled[1];
                        blue_after = controlled[2];
                        for(int i = 1; i<=255; i++){
                            out_led_red = (float)red_before + (((float)red_after - (float)red_before)/255)*i;
                            out_led_red = abs(out_led_red);
                            analogWrite(PIN_LED_RED, uint8_t(out_led_red*intensityLight/100));
                            out_led_green = (float)green_before + (((float)green_after - (float)green_before)/255)*i;
                            out_led_green = abs(out_led_green);
                            analogWrite(PIN_LED_GREEN, uint8_t(out_led_green*intensityLight/100));
                            out_led_blue = (float)blue_before + (((float)blue_after - (float)blue_before)/255)*i;
                            out_led_blue = abs(out_led_blue);
                            analogWrite(PIN_LED_BLUE, uint8_t(out_led_blue*intensityLight/100));
                            delay(10);
                        }
                        red_before = red_after;
                        green_before = green_after;
                        blue_before = blue_after;
                        
                        ECHO("RED: ");
                        ECHOLN(uint8_t(out_led_red));
                        ECHO("GREEN: ");
                        ECHOLN(uint8_t(out_led_green));
                        ECHO("BLUE: ");
                        ECHOLN(uint8_t(out_led_blue));

                        EEPROM.write(EEPROM_WIFI_LED_RED, red_after);
                        EEPROM.write(EEPROM_WIFI_LED_GREEN, green_after);
                        EEPROM.write(EEPROM_WIFI_LED_BLUE, blue_after);
                        EEPROM.commit();
                        ECHOLN("Done writing!");
                    }
                }
            }
        }
    }
    
}


void reconnect() {
    // Loop until we're reconnected
    ECHO("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP32Avy-";
    // clientId += GetFullSSID;
    clientId += String(random(0xffffff), HEX);
    // const char *user = "tkrvqyzs";
    // const char *pass = "lS_VJSmDuiXC";
    const char* willTopic = "CabinetAvy/HPT/LWT";
    uint8_t willQos = 0;
    boolean willRetain = false;
    String willMessage = "{\"devicetype\" : \"";
    willMessage += m_Typedevice;
    willMessage += "\", \"deviceid\" : \"";
    willMessage += String(deviceId);
    willMessage += "\", \"status\" : \"error\"}";
    // Attempt to connect
    // if (client.connect(clientId.c_str(), user, pass)){
    if (client.connect(clientId.c_str(), willTopic, willQos, willRetain, willMessage.c_str())) {
        ECHO("connected with id: ");
        ECHOLN(clientId);
        // Once connected, publish an announcement...
        // client.publish("outTopic", "hello world");
        // ... and resubscribe
        // String topicGetstatus = m_Pretopic + m_Getstatus;
        String topicControlvoice = m_Pretopic + m_Controlvoice;
        String topicControlhand = m_Pretopic + m_Controlhand;

        
        client.subscribe(topicControlvoice.c_str());
        client.subscribe(topicControlhand.c_str());
        ECHO("Done Subscribe Channel: ");
        ECHO(topicControlvoice);
        ECHO("  +  ");
        ECHOLN(topicControlhand);
        countDisconnectToServer++;
        if(flag_disconnect_to_sever == true){
            sum_time_disconnect_to_sever += millis() - count_time_disconnect_to_sever;
            flag_disconnect_to_sever = false;
        }
        digitalWrite(LED_TEST, LOW);
        getStatus();
        SendStatusReconnect();
    } else {
        ECHO("failed, rc=");
        ECHO(client.state());
        ECHOLN(" try again in 2 seconds");
        // Wait 2 seconds before retrying
        delay(2000);
    }
}




// void StartNormalSever(){
//     server.on("/", HTTP_GET, handleRoot);
//     server.on("/getstatus", HTTP_GET, getStatus);
//     server.on("/control_led", HTTP_POST, ControlLed);
//     server.on("/control_led_color", HTTP_POST, ControlLedColor);
//     server.on("/control_intensity", HTTP_POST, ControlIntensity);
//     // server.on("/", HTTP_OPTIONS, handleOk);
//     // server.on("/go_up", HTTP_OPTIONS, handleOk);
//     // server.on("/go_down", HTTP_OPTIONS, handleOk);
//     // server.on("/stop", HTTP_OPTIONS, handleOk);
//     // server.on("/action", HTTP_OPTIONS, handleOk);
//     // server.on("/control_led", HTTP_OPTIONS, handleOk);
//     // server.on("/control_led_color", HTTP_OPTIONS, handleOk);
//     // server.on("/control_intensity", HTTP_OPTIONS, handleOk);
//     server.begin();
//     ECHOLN("HTTP server started");
// }


void tickerupdate(){
    tickerSetApMode.update();
}

void setup() {
    pinMode(LED_TEST, OUTPUT);
    pinMode(LED_TEST_MOTOR, OUTPUT);
    pinMode(PIN_CONFIG, INPUT);
    pinMode(PIN_PUL_MOTOR, OUTPUT);
    pinMode(PIN_DIR_MOTOR, OUTPUT);
    pinMode(PIN_ENCODER_MOTOR, OUTPUT);
    // pinMode(PIN_ENCODER_MOTOR, INPUT_PULLUP);
    delay(10);
    digitalWrite(LED_TEST, HIGH);
    analogWriteRange(255);      //max gia tri PWM la 255
    Serial.begin(115200);
    EEPROM.begin(512);
    SetupNetwork();     //khi hoat dong binh thuong

}


void loop() {
    if (Flag_Normal_Mode == true && WiFi.status() != WL_CONNECTED){
        // digitalWrite(LED_TEST, HIGH);
        if(flag_disconnect_to_sever == false){
            count_time_disconnect_to_sever = millis();
            flag_disconnect_to_sever = true;
        }
        testWifi(esid, epass);
    } 

    if(millis() == timeToSaveData + 5000){
        ECHOLN("Save data");
        ECHO("RED: ");
        ECHOLN(red_after);
        ECHO("GREEN: ");
        ECHOLN(green_after);
        ECHO("BLUE: ");
        ECHOLN(blue_after);
        ECHO("INTENSITY: ");
        ECHOLN(intensityLight);

        EEPROM.write(EEPROM_WIFI_LED_RED, red_after);
        EEPROM.write(EEPROM_WIFI_LED_GREEN, green_after);
        EEPROM.write(EEPROM_WIFI_LED_BLUE, blue_after);
        EEPROM.write(EEPROM_WIFI_LED_INTENSITY, intensityLight);
        
        EEPROM.commit();
        ECHOLN("Done writing!");

    }

    if(WiFi.status() == WL_CONNECTED){
        if (!client.connected()) {
            if(flag_disconnect_to_sever == false){
                count_time_disconnect_to_sever = millis();
                flag_disconnect_to_sever = true;
            }
            digitalWrite(LED_TEST, HIGH);
            reconnect();
        }
        client.loop();
    }

    checkButtonConfigClick();
    tickerupdate();
    server.handleClient();

}
