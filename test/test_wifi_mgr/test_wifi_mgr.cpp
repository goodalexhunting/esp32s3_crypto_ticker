#include <ArduinoJson.h>
#include <unity.h>

#include <cstring>

#include "../../src/wifi_mgr.cpp"
#include "Arduino.h"
#include "embedded_ui.h"
#include "preferences_mock.h"
#include "wifi_mgr.h"
#include "wifi_mocks.h"

// Helper to POST to the WifiManager's private server via the mock.
static WebServer* server() {
    return WebServer::current();
}

static void postConnect(const std::vector<std::pair<std::string, std::string>>& args) {
    server()->setArgs(args);
    server()->call("/connect", HTTP_POST);
}

void setUp() {
    Preferences::clear();
    mockResetMillis();
    mockWiFi         = MockWiFiRecords();
    mockWiFi.localIp = IPAddress(192, 168, 1, 50);
    mockWiFi.apIp    = IPAddress(192, 168, 4, 1);
}

void tearDown() {}

// ---------------------------------------------------------------------------
// begin()
// ---------------------------------------------------------------------------

void test_begin_no_credentials_starts_ap() {
    cryptoapp::WifiManager wm;
    bool                   connected = wm.begin();
    TEST_ASSERT_FALSE(connected);
    TEST_ASSERT_EQUAL(cryptoapp::WifiManager::State::AP_MODE, wm.getState());
    TEST_ASSERT_EQUAL(WIFI_AP, mockWiFi.lastMode);
    TEST_ASSERT(wm.getAPName().startsWith("CryptoTicker-"));
    // Captive portal wiring: the private server must be running.
    TEST_ASSERT_TRUE(server()->_begun);
}

void test_begin_credentials_connect_success() {
    Preferences::put("wifi_creds", "ssid", "MyNet");
    Preferences::put("wifi_creds", "pass", "secret");
    mockWiFi.status          = WL_CONNECTED;
    mockWiFi.lastBeginResult = true;

    cryptoapp::WifiManager wm;
    bool                   connected = wm.begin();
    TEST_ASSERT_TRUE(connected);
    TEST_ASSERT_EQUAL(cryptoapp::WifiManager::State::CONNECTED, wm.getState());
    TEST_ASSERT_EQUAL(0, mockWiFi.disconnectCount);
}

void test_begin_credentials_connect_fail_starts_ap() {
    Preferences::put("wifi_creds", "ssid", "MyNet");
    Preferences::put("wifi_creds", "pass", "secret");
    mockWiFi.status = WL_DISCONNECTED;

    cryptoapp::WifiManager wm;
    bool                   connected = wm.begin();
    TEST_ASSERT_FALSE(connected);
    TEST_ASSERT_EQUAL(cryptoapp::WifiManager::State::AP_MODE, wm.getState());
}

// ---------------------------------------------------------------------------
// handle(): AP timeout
// ---------------------------------------------------------------------------

void test_handle_ap_timeout_reconnects_with_stored_creds() {
    Preferences::put("wifi_creds", "ssid", "MyNet");
    Preferences::put("wifi_creds", "pass", "secret");

    cryptoapp::WifiManager wm;
    wm.begin();  // no connection -> AP mode
    TEST_ASSERT_EQUAL(cryptoapp::WifiManager::State::AP_MODE, wm.getState());

    mockSetMillis(6UL * 60UL * 1000UL);
    mockWiFi.status          = WL_CONNECTED;
    mockWiFi.lastBeginResult = true;

    wm.handle();
    TEST_ASSERT_EQUAL(cryptoapp::WifiManager::State::CONNECTED, wm.getState());
    TEST_ASSERT_EQUAL_STRING("MyNet", mockWiFi.lastSsid.c_str());
}

void test_handle_ap_timeout_reconnect_fail_restarts_ap() {
    Preferences::put("wifi_creds", "ssid", "MyNet");
    Preferences::put("wifi_creds", "pass", "secret");

    cryptoapp::WifiManager wm;
    wm.begin();

    mockSetMillis(6UL * 60UL * 1000UL);
    mockWiFi.status = WL_DISCONNECTED;

    wm.handle();
    TEST_ASSERT_EQUAL(cryptoapp::WifiManager::State::AP_MODE, wm.getState());
}

void test_handle_connection_lost_triggers_reconnect() {
    Preferences::put("wifi_creds", "ssid", "MyNet");
    Preferences::put("wifi_creds", "pass", "secret");
    mockWiFi.status          = WL_CONNECTED;
    mockWiFi.lastBeginResult = true;

    cryptoapp::WifiManager wm;
    wm.begin();  // CONNECTED

    mockWiFi.status = WL_DISCONNECTED;
    wm.handle();
    TEST_ASSERT_EQUAL(cryptoapp::WifiManager::State::DISCONNECTED, wm.getState());

    mockSetMillis(11UL * 1000UL);
    mockWiFi.status          = WL_CONNECTED;
    mockWiFi.lastBeginResult = true;
    wm.handle();
    TEST_ASSERT_EQUAL(cryptoapp::WifiManager::State::CONNECTED, wm.getState());
    TEST_ASSERT_EQUAL_STRING("MyNet", mockWiFi.lastSsid.c_str());
}

// ---------------------------------------------------------------------------
// /connect handler (via the mock WebServer)
// ---------------------------------------------------------------------------

void test_handle_connect_valid_credentials() {
    cryptoapp::WifiManager wm;
    wm.begin();  // AP mode, server running

    // A reachable network.
    mockWiFi.status          = WL_CONNECTED;
    mockWiFi.lastBeginResult = true;

    postConnect({{"ssid", "CoffeeShop"}, {"pass", "wifi123"}});

    TEST_ASSERT_EQUAL(200, server()->response.code);
    TEST_ASSERT_EQUAL_STRING("OK", server()->response.content.c_str());
    TEST_ASSERT_EQUAL_STRING("CoffeeShop", mockWiFi.lastSsid.c_str());
    TEST_ASSERT_EQUAL_STRING("wifi123", mockWiFi.lastPass.c_str());
    // Credentials persist to NVS.
    TEST_ASSERT_EQUAL_STRING("CoffeeShop", Preferences::get("wifi_creds", "ssid").c_str());
    TEST_ASSERT_EQUAL_STRING("wifi123", Preferences::get("wifi_creds", "pass").c_str());
    // State moves to connected and AP is stopped.
    TEST_ASSERT_EQUAL(cryptoapp::WifiManager::State::CONNECTED, wm.getState());
    TEST_ASSERT_FALSE(server()->_begun);
}

void test_handle_connect_missing_ssid_400() {
    cryptoapp::WifiManager wm;
    wm.begin();

    postConnect({{"pass", "wifi123"}});
    TEST_ASSERT_EQUAL(400, server()->response.code);
    TEST_ASSERT_EQUAL_STRING("Missing SSID", server()->response.content.c_str());
}

void test_handle_connect_empty_ssid_400() {
    cryptoapp::WifiManager wm;
    wm.begin();

    // ssid is whitespace -> trims to empty.
    postConnect({{"ssid", "   "}, {"pass", "wifi123"}});
    TEST_ASSERT_EQUAL(400, server()->response.code);
    TEST_ASSERT_EQUAL_STRING("Empty SSID", server()->response.content.c_str());
}

void test_handle_connect_wrong_credentials_401() {
    cryptoapp::WifiManager wm;
    wm.begin();

    // Wi-Fi join fails.
    mockWiFi.status = WL_DISCONNECTED;

    postConnect({{"ssid", "WrongNet"}, {"pass", "badpass"}});
    TEST_ASSERT_EQUAL(401, server()->response.code);
    TEST_ASSERT_EQUAL_STRING("Wrong credentials or unreachable network",
                             server()->response.content.c_str());
    // Nothing persisted on failure.
    TEST_ASSERT_EQUAL_STRING("", Preferences::get("wifi_creds", "ssid").c_str());
    // Still in AP mode so the user can retry.
    TEST_ASSERT_EQUAL(cryptoapp::WifiManager::State::AP_MODE, wm.getState());
}

void test_handle_connect_truncates_long_credentials_on_save() {
    cryptoapp::WifiManager wm;
    wm.begin();

    mockWiFi.status          = WL_CONNECTED;
    mockWiFi.lastBeginResult = true;

    std::string longSsid("N");
    longSsid.append(60, 'x');  // 61 chars > 32
    std::string longPass("P");
    longPass.append(80, 'y');  // 81 chars > 64

    postConnect({{"ssid", longSsid}, {"pass", longPass}});
    TEST_ASSERT_EQUAL(200, server()->response.code);

    std::string savedSsid = Preferences::get("wifi_creds", "ssid");
    std::string savedPass = Preferences::get("wifi_creds", "pass");
    TEST_ASSERT_EQUAL(32u, savedSsid.size());
    TEST_ASSERT_EQUAL(64u, savedPass.size());
}

// ---------------------------------------------------------------------------
// handleRoot / handleScan / handleNotFound (captive portal)
// ---------------------------------------------------------------------------

void test_handle_root_serves_embedded_page() {
    cryptoapp::WifiManager wm;
    wm.begin();

    server()->call("/", HTTP_GET);
    TEST_ASSERT_EQUAL(200, server()->response.code);
    TEST_ASSERT_EQUAL_STRING("text/html", server()->response.contentType.c_str());
    TEST_ASSERT_EQUAL_STRING("no-store", server()->response.headers["Cache-Control"].c_str());
    // The embedded page must contain the config form's key elements.
    TEST_ASSERT_NOT_NULL(strstr(server()->response.content.c_str(), "</html>"));
}

void test_handle_scan_returns_valid_json() {
    cryptoapp::WifiManager wm;
    wm.begin();

    mockWiFi.scanSsids      = {"NetA", "NetB"};
    mockWiFi.scanRssis      = {-45, -70};
    mockWiFi.scanEncryption = {WIFI_AUTH_WPA2_PSK, WIFI_AUTH_OPEN};
    mockWiFi.scanResult     = 2;

    server()->call("/scan", HTTP_GET);
    TEST_ASSERT_EQUAL(200, server()->response.code);
    TEST_ASSERT_EQUAL_STRING("application/json", server()->response.contentType.c_str());
    TEST_ASSERT_EQUAL_STRING("no-store", server()->response.headers["Cache-Control"].c_str());

    JsonDocument         doc;
    DeserializationError err = deserializeJson(doc, server()->response.content);
    TEST_ASSERT_FALSE(bool(err));
    JsonArray arr = doc.as<JsonArray>();
    TEST_ASSERT_EQUAL(2, arr.size());
    // Hoist the template-comma expressions out of the Unity macros so the
    // preprocessor does not split on the commas inside as<T>().
    const char* ssid0    = arr[0]["ssid"].as<const char*>();
    int         rssi0    = arr[0]["rssi"].as<int>();
    bool        encrypt0 = arr[0]["encrypt"].as<bool>();
    const char* ssid1    = arr[1]["ssid"].as<const char*>();
    int         rssi1    = arr[1]["rssi"].as<int>();
    bool        encrypt1 = arr[1]["encrypt"].as<bool>();
    TEST_ASSERT_EQUAL_STRING("NetA", ssid0);
    TEST_ASSERT_EQUAL(-45, rssi0);
    TEST_ASSERT_TRUE(encrypt0);
    TEST_ASSERT_EQUAL_STRING("NetB", ssid1);
    TEST_ASSERT_EQUAL(-70, rssi1);
    TEST_ASSERT_FALSE(encrypt1);
}

void test_handle_not_found_redirects_to_ap_ip() {
    cryptoapp::WifiManager wm;
    wm.begin();
    mockWiFi.apIp = IPAddress(192, 168, 4, 1);

    server()->call("/some/other/host", HTTP_GET);
    TEST_ASSERT_EQUAL(302, server()->response.code);
    TEST_ASSERT_TRUE(server()->response.isRedirect);
    // Captive portal redirect target: http://<ap-ip>/
    TEST_ASSERT_EQUAL_STRING("http://192.168.4.1/", server()->response.location.c_str());
}

// ---------------------------------------------------------------------------
// forceAPMode / clearCredentials / sleep
// ---------------------------------------------------------------------------

void test_force_ap_mode_from_connected() {
    Preferences::put("wifi_creds", "ssid", "MyNet");
    Preferences::put("wifi_creds", "pass", "secret");
    mockWiFi.status          = WL_CONNECTED;
    mockWiFi.lastBeginResult = true;

    cryptoapp::WifiManager wm;
    wm.begin();
    TEST_ASSERT_EQUAL(cryptoapp::WifiManager::State::CONNECTED, wm.getState());

    wm.forceAPMode();
    TEST_ASSERT_EQUAL(cryptoapp::WifiManager::State::AP_MODE, wm.getState());
    TEST_ASSERT_EQUAL(WIFI_AP, mockWiFi.lastMode);
}

void test_clear_credentials_erases_nvs_and_disconnects() {
    Preferences::put("wifi_creds", "ssid", "MyNet");
    Preferences::put("wifi_creds", "pass", "secret");
    mockWiFi.status          = WL_CONNECTED;
    mockWiFi.lastBeginResult = true;

    cryptoapp::WifiManager wm;
    wm.begin();

    wm.clearCredentials();
    TEST_ASSERT_EQUAL_STRING("", Preferences::get("wifi_creds", "ssid").c_str());
    TEST_ASSERT_EQUAL_STRING("", Preferences::get("wifi_creds", "pass").c_str());

    cryptoapp::WifiManager wm2;
    bool                   connected = wm2.begin();
    TEST_ASSERT_FALSE(connected);
    TEST_ASSERT_EQUAL(cryptoapp::WifiManager::State::AP_MODE, wm2.getState());
}

void test_sleep_shuts_down_radio() {
    Preferences::put("wifi_creds", "ssid", "MyNet");
    Preferences::put("wifi_creds", "pass", "secret");
    mockWiFi.status          = WL_CONNECTED;
    mockWiFi.lastBeginResult = true;

    cryptoapp::WifiManager wm;
    wm.begin();
    TEST_ASSERT_EQUAL(cryptoapp::WifiManager::State::CONNECTED, wm.getState());

    wm.sleep();
    TEST_ASSERT_EQUAL(cryptoapp::WifiManager::State::DISCONNECTED, wm.getState());
    TEST_ASSERT_EQUAL(WIFI_OFF, mockWiFi.lastMode);
}

// ---------------------------------------------------------------------------
// AP SSID naming
// ---------------------------------------------------------------------------

void test_ap_name_built_from_efuse_mac() {
    mockSetEfuseMac(0x00AA00BBULL);
    cryptoapp::WifiManager wm;
    wm.begin();
    TEST_ASSERT(wm.getAPName().startsWith("CryptoTicker-"));
    TEST_ASSERT_TRUE(wm.getAPName().length() > 13u);
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_begin_no_credentials_starts_ap);
    RUN_TEST(test_begin_credentials_connect_success);
    RUN_TEST(test_begin_credentials_connect_fail_starts_ap);
    RUN_TEST(test_handle_ap_timeout_reconnects_with_stored_creds);
    RUN_TEST(test_handle_ap_timeout_reconnect_fail_restarts_ap);
    RUN_TEST(test_handle_connection_lost_triggers_reconnect);

    RUN_TEST(test_handle_connect_valid_credentials);
    RUN_TEST(test_handle_connect_missing_ssid_400);
    RUN_TEST(test_handle_connect_empty_ssid_400);
    RUN_TEST(test_handle_connect_wrong_credentials_401);
    RUN_TEST(test_handle_connect_truncates_long_credentials_on_save);

    RUN_TEST(test_handle_root_serves_embedded_page);
    RUN_TEST(test_handle_scan_returns_valid_json);
    RUN_TEST(test_handle_not_found_redirects_to_ap_ip);

    RUN_TEST(test_force_ap_mode_from_connected);
    RUN_TEST(test_clear_credentials_erases_nvs_and_disconnects);
    RUN_TEST(test_sleep_shuts_down_radio);
    RUN_TEST(test_ap_name_built_from_efuse_mac);

    return UNITY_END();
}