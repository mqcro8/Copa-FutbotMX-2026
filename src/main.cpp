#include <esp_now.h>
#include <WiFi.h>

// ── esp now functions ──────────────────────────────────────
uint8_t peerMac[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}; // Replace with Board's actual MAC
bool killed = false;

// Not sure what will be send in the message — both boards must define this identically
typedef struct { 
  int    counter;
  char   label[16];
} Message;

Message outgoing;

// Called after every send attempt
void OnDataSent(const uint8_t *mac, esp_now_send_status_t status) {
  Serial.print("Send status: "); 
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

// Receive data
void OnDataRecv(const esp_now_recv_info_t *info,
                const uint8_t *data, int len) {
  Message incoming;
  memcpy(&incoming, data, sizeof(incoming)); 
  Serial.printf("Got from peer → counter=%d  label=%s\n",
                incoming.counter, incoming.label);
}

// Auto replay 
/*
  reply.counter     = incoming.counter;
  strncpy(reply.label, "ack", sizeof(reply.label));
  esp_now_send(peerMac, (uint8_t *)&reply, sizeof(reply));
*/

void ESP_NOW_PREP(){
    // WiFi in station mode — required for ESP-NOW; no AP needed
  WiFi.mode(WIFI_STA);
  Serial.print("Board A MAC: ");
  Serial.println(WiFi.macAddress());

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  // Register callbacks
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  // Register peer
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, peerMac, 6);
  peer.channel  = 0;   // 0 = use current channel
  peer.encrypt  = false;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
}

void sendMessage(){
    // Filling message
  outgoing.counter++;
  strncpy(outgoing.label, "hello", sizeof(outgoing.label));

    // Send data
  esp_now_send(peerMac, (uint8_t *)&outgoing, sizeof(outgoing));
}

void setup() {
  Serial.begin(115200);

  ESP_NOW_PREP();
}

// ── System functions ──────────────────────────────────────

void killSwitchActive(){

}

void safeShutdown() {
  // Turn off motors, PWM, relays, etc.
}

void runSystem(){
    //Robot's behavior
}

void loop() {
  if (killed) {
    safeShutdown();

    // Wait until switch is released
    while (digitalRead(KILL_PIN) == LOW) {
      delay(10);
    }

    // Reboot after kill is cleared
    ESP.restart();
  }

  // Normal behavior
  runSystem();
}