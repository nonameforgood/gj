#ifndef LWIP_OPEN_SRC
#define LWIP_OPEN_SRC
#endif
#include <functional>
#include <WiFiUdp.h>
#include "GJArduinoOTA.h"
//#include "ESPmDNS.h"
#include "MD5Builder.h"
#include "Update.h"

#include "gj/base.h"

// #define OTA_DEBUG Serial

ArduinoOTAClass::ArduinoOTAClass()
: _port(0)
, _initialized(false)
, _rebootOnSuccess(true)
, _mdnsEnabled(true)
, _state(OTA_IDLE)
, _size(0)
, _cmd(0)
, _ota_port(0)
, _ota_timeout(1000)
, _start_callback(NULL)
, _end_callback(NULL)
, _error_callback(NULL)
, _progress_callback(NULL)
{
}

ArduinoOTAClass::~ArduinoOTAClass(){
    _udp_ota.stop();
}

ArduinoOTAClass& ArduinoOTAClass::onStart(THandlerFunction fn) {
    _start_callback = fn;
    return *this;
}

ArduinoOTAClass& ArduinoOTAClass::onEnd(THandlerFunction fn) {
    _end_callback = fn;
    return *this;
}

ArduinoOTAClass& ArduinoOTAClass::onProgress(THandlerFunction_Progress fn) {
    _progress_callback = fn;
    return *this;
}

ArduinoOTAClass& ArduinoOTAClass::onError(THandlerFunction_Error fn) {
    _error_callback = fn;
    return *this;
}

ArduinoOTAClass& ArduinoOTAClass::setPort(uint16_t port) {
    if (!_initialized && !_port && port) {
        _port = port;
    }
    return *this;
}

ArduinoOTAClass& ArduinoOTAClass::setHostname(const char * hostname) {
    if (!_initialized && !_hostname.length() && hostname) {
        _hostname = hostname;
    }
    return *this;
}

String ArduinoOTAClass::getHostname() {
    return _hostname;
}

ArduinoOTAClass& ArduinoOTAClass::setPassword(const char * password) {
    if (!_initialized && !_password.length() && password) {
        MD5Builder passmd5;
        passmd5.begin();
        passmd5.add(password);
        passmd5.calculate();
        _password = passmd5.toString();
    }
    return *this;
}

ArduinoOTAClass& ArduinoOTAClass::setPasswordHash(const char * password) {
    if (!_initialized && !_password.length() && password) {
        _password = password;
    }
    return *this;
}

ArduinoOTAClass& ArduinoOTAClass::setPartitionLabel(const char * partition_label) {
    if (!_initialized && !_partition_label.length() && partition_label) {
        _partition_label = partition_label;
    }
    return *this;
}

String ArduinoOTAClass::getPartitionLabel() {
    return _partition_label;
}

ArduinoOTAClass& ArduinoOTAClass::setRebootOnSuccess(bool reboot){
    _rebootOnSuccess = reboot;
    return *this;
}

ArduinoOTAClass& ArduinoOTAClass::setMdnsEnabled(bool enabled){
    _mdnsEnabled = enabled;
    return *this;
}

void ArduinoOTAClass::begin() {
    if (_initialized){
        log_w("already initialized");
        return;
    }

    if (!_port) {
        _port = 3232;
    }

    if(!_udp_ota.begin(_port)){
        log_e("udp bind failed");
        return;
    }


    if (!_hostname.length()) {
        char tmp[20];

        uint8_t mac[6] = {};
        esp_efuse_mac_get_default(mac);
        sprintf(tmp, "esp32-%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        _hostname = tmp;
    }
    if(_mdnsEnabled){
        //MDNS.begin(_hostname.c_str());
        //MDNS.enableArduino(_port, (_password.length() > 0));
    }
    _initialized = true;
    _state = OTA_IDLE;
    log_i("OTA server at: %s.local:%u", _hostname.c_str(), _port);
}

int ArduinoOTAClass::parseInt(){
    char data[INT_BUFFER_SIZE];
    uint8_t index = 0;
    char value;
    while(_udp_ota.peek() == ' ') _udp_ota.read();
    while(index < INT_BUFFER_SIZE - 1){
        value = _udp_ota.peek();
        if(value < '0' || value > '9'){
            data[index++] = '\0';
            return atoi(data);
        }
        data[index++] = _udp_ota.read();
    }
    return 0;
}

String ArduinoOTAClass::readStringUntil(char end){
    String res = "";
    int value;
    while(true){
        value = _udp_ota.read();
        if(value <= 0 || value == end){
            return res;
        }
        res += (char)value;
    }
    return res;
}

void ArduinoOTAClass::_onRx(){
    if (_state == OTA_IDLE) {
        int cmd = parseInt();
        if (cmd != U_FLASH && cmd != U_SPIFFS)
            return;
        _cmd  = cmd;
        _ota_port = parseInt();
        _size = parseInt();
        _udp_ota.read();
        _md5 = readStringUntil('\n');
        _md5.trim();
        if(_md5.length() != 32){
            log_e("bad md5 length");
            return;
        }

        if (_password.length()){
            MD5Builder nonce_md5;
            nonce_md5.begin();
            nonce_md5.add(String(micros()));
            nonce_md5.calculate();
            _nonce = nonce_md5.toString();

            _udp_ota.beginPacket(_udp_ota.remoteIP(), _udp_ota.remotePort());
            _udp_ota.printf("AUTH %s", _nonce.c_str());
            _udp_ota.endPacket();
            _state = OTA_WAITAUTH;
            return;
        } else {
            _udp_ota.beginPacket(_udp_ota.remoteIP(), _udp_ota.remotePort());
            _udp_ota.print("OK");
            _udp_ota.endPacket();
            _ota_ip = _udp_ota.remoteIP();
            _state = OTA_RUNUPDATE;
        }
    } else if (_state == OTA_WAITAUTH) {
        int cmd = parseInt();
        if (cmd != U_AUTH) {
            log_e("%d was expected. got %d instead", U_AUTH, cmd);
            _state = OTA_IDLE;
            return;
        }
        _udp_ota.read();
        String cnonce = readStringUntil(' ');
        String response = readStringUntil('\n');
        if (cnonce.length() != 32 || response.length() != 32) {
            log_e("auth param fail");
            _state = OTA_IDLE;
            return;
        }

        String challenge = _password + ":" + String(_nonce) + ":" + cnonce;
        MD5Builder _challengemd5;
        _challengemd5.begin();
        _challengemd5.add(challenge);
        _challengemd5.calculate();
        String result = _challengemd5.toString();

        if(result.equals(response)){
            _udp_ota.beginPacket(_udp_ota.remoteIP(), _udp_ota.remotePort());
            _udp_ota.print("OK");
            _udp_ota.endPacket();
            _ota_ip = _udp_ota.remoteIP();
            _state = OTA_RUNUPDATE;
        } else {
            _udp_ota.beginPacket(_udp_ota.remoteIP(), _udp_ota.remotePort());
            _udp_ota.print("Authentication Failed");
            log_w("Authentication Failed");
            _udp_ota.endPacket();
            if (_error_callback) _error_callback(OTA_AUTH_ERROR);
            _state = OTA_IDLE;
        }
    }
}

void ArduinoOTAClass::_runUpdate() {

    const char *partition_label = _partition_label.length() ? _partition_label.c_str() : NULL;
    if (!Update.begin(_size, _cmd, -1, LOW, partition_label)) {

        LOG("ArduinoOTA:Begin ERROR: %s", Update.errorString());

        if (_error_callback) {
            _error_callback(OTA_BEGIN_ERROR);
        }
        _state = OTA_IDLE;
        return;
    }
    Update.setMD5(_md5.c_str());

    if (_start_callback) {
        _start_callback();
    }
    if (_progress_callback) {
        _progress_callback(0, _size);
    }

    WiFiClient client;
    if (!client.connect(_ota_ip, _ota_port)) {
        if (_error_callback) {
            _error_callback(OTA_CONNECT_ERROR);
        }
        _state = OTA_IDLE;
    }

    uint32_t written = 0, total = 0, tried = 0;

    while (!Update.isFinished() && client.connected()) {
        size_t waited = _ota_timeout;
        size_t available = client.available();
        while (!available && waited){
            delay(1);
            waited -=1 ;
            available = client.available();
        }
        if (!waited){
            if(written && tried++ < 3){
                LOG("ArduinoOTA::Try[%u]: %u", tried, written);
                if(!client.printf("%u", written)){
                    LOG("ArduinoOTA:failed to respond");
                    _state = OTA_IDLE;
                    break;
                }
                continue;
            }
            LOG("ArduinoOTA:Receive Failed");
            if (_error_callback) {
                _error_callback(OTA_RECEIVE_ERROR);
            }
            _state = OTA_IDLE;
            Update.abort();
            return;
        }
        if(!available){
            LOG("ArduinoOTA:No Data: %u", waited);
            _state = OTA_IDLE;
            break;
        }
        tried = 0;
        static uint8_t buf[1460];
        if(available > 1460){
            available = 1460;
        }
        size_t r = client.read(buf, available);
        if(r != available){
            LOG("ArduinoOTA:didn't read enough! %u != %u", r, available);
        }

        written = Update.write(buf, r);
        if (written > 0) {
            if(written != r){
                LOG("ArduinoOTA:didn't write enough! %u != %u", written, r);
            }
            if(!client.printf("%u", written)){
                LOG("ArduinoOTA:failed to respond");
            }
            total += written;
            if(_progress_callback) {
                _progress_callback(total, _size);
            }
        } else {
            LOG("ArduinoOTA:Write ERROR: %s", Update.errorString());
        }
    }

    if (Update.end()) {
        client.print("OK");
        client.stop();
        delay(10);
        if (_end_callback) {
            _end_callback();
        }
        if(_rebootOnSuccess){
            //let serial/network finish tasks that might be given in _end_callback
            delay(100);
            ESP.restart();
        }
    } else {
        if (_error_callback) {
            _error_callback(OTA_END_ERROR);
        }
        Update.printError(client);
        client.stop();
        delay(10);
        LOG("ArduinoOTA:Update ERROR: %s", Update.errorString());
        _state = OTA_IDLE;
    }
}


void ArduinoOTAClass::_runAsyncUpdate() {

    uint32_t &written = m_asyncState.written;
    uint32_t &total = m_asyncState.total;
    uint32_t &tried = m_asyncState.tried;

    if (!m_client)     
    {
        const char *partition_label = _partition_label.length() ? _partition_label.c_str() : NULL;
        if (!Update.begin(_size, _cmd, -1, LOW, partition_label)) {

            LOG("ArduinoOTA:Begin ERROR: %s", Update.errorString());

            if (_error_callback) {
                _error_callback(OTA_BEGIN_ERROR);
            }
            _state = OTA_IDLE;
            return;
        }
        Update.setMD5(_md5.c_str());

        if (_start_callback) {
            _start_callback();
        }
        if (_progress_callback) {
            _progress_callback(0, _size);
        }

        m_client = new WiFiClient;

        WiFiClient &client = *m_client;
        if (!client.connect(_ota_ip, _ota_port)) {
            if (_error_callback) {
                _error_callback(OTA_CONNECT_ERROR);
            }
            _state = OTA_IDLE;
        }

        written = 0;
        total = 0;
        tried = 0;
    }

    WiFiClient &client = *m_client;

    if (!Update.isFinished() && client.connected()) {
        size_t waited = _ota_timeout;
        size_t available = client.available();
        while (!available && waited){
            delay(1);
            waited -=1 ;
            available = client.available();
        }
        if (!waited){
            if(written && tried++ < 3){
                LOG("ArduinoOTA::Try[%u]: %u", tried, written);
                if(!client.printf("%u", written)){
                    LOG("ArduinoOTA:failed to respond");
                    _state = OTA_IDLE;
                    return;
                }
                return;
            }
            LOG("ArduinoOTA:Receive Failed");
            if (_error_callback) {
                _error_callback(OTA_RECEIVE_ERROR);
            }
            _state = OTA_IDLE;
            Update.abort();

            delete m_client;
            m_client = nullptr;
            return;
        }
        if(!available){
            LOG("ArduinoOTA:No Data: %u", waited);
            _state = OTA_IDLE;
            Update.abort();

            delete m_client;
            m_client = nullptr;
            return;
        }
        tried = 0;
        static uint8_t buf[1460];
        if(available > 1460){
            available = 1460;
        }
        size_t r = client.read(buf, available);
        if(r != available){
            LOG("ArduinoOTA:didn't read enough! %u != %u", r, available);
        }

        written = Update.write(buf, r);
        if (written > 0) {
            if(written != r){
                LOG("ArduinoOTA:didn't write enough! %u != %u", written, r);
            }
            if(!client.printf("%u", written)){
                LOG("ArduinoOTA:failed to respond");
            }
            total += written;
            if(_progress_callback) {
                _progress_callback(total, _size);
            }
        } else {
            LOG("ArduinoOTA:Write ERROR: %s", Update.errorString());
        }
    }

    if (!Update.isFinished() && client.connected())
        return;

    if (Update.isFinished() && client.connected() && Update.end()) {
        client.print("OK");
        client.stop();
        delete m_client;
        m_client = nullptr;
        delay(10);
        if (_end_callback) {
            _end_callback();
        }
        if(_rebootOnSuccess){
            //let serial/network finish tasks that might be given in _end_callback
            delay(100);
            ESP.restart();
        }
    } else {
        if (_error_callback) {
            _error_callback(OTA_END_ERROR);
        }
        Update.printError(client);
        client.stop();
        delete m_client;
        m_client = nullptr;
        delay(10);
        LOG("ArduinoOTA:Update ERROR: %s", Update.errorString());
    }

    _state = OTA_IDLE;
}

void ArduinoOTAClass::end() {
    _initialized = false;
    _udp_ota.stop();
    if(_mdnsEnabled){
        //MDNS.end();
    }
    _state = OTA_IDLE;
    log_i("OTA server stopped.");
}

void ArduinoOTAClass::handle() {
    if (!_initialized) {
        return;
    }
    if (_state == OTA_RUNUPDATE) {
        _runUpdate();
        _state = OTA_IDLE;
    }
    if(_udp_ota.parsePacket()){
        _onRx();
    }
    _udp_ota.flush(); // always flush, even zero length packets must be flushed.
}


void ArduinoOTAClass::handleAsync() {
    if (!_initialized) {
        return;
    }
    if (_state == OTA_RUNUPDATE) {
        _runAsyncUpdate();
        //_state = OTA_IDLE;
    }
    if(_state == OTA_IDLE && _udp_ota.parsePacket()){
        _onRx();
    }
    _udp_ota.flush(); // always flush, even zero length packets must be flushed.
}

int ArduinoOTAClass::getCommand() {
    return _cmd;
}

void ArduinoOTAClass::setTimeout(int timeoutInMillis) {
    _ota_timeout = timeoutInMillis;
}

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_ARDUINOOTA)
ArduinoOTAClass GJArduinoOTA;
#endif