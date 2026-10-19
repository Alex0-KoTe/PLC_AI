#include "webui.h"
#include "globals.h"
#include "logger.h"
#include "settings.h"
#include "ntp.h"
#include "archive.h"
#include "protection.h"
#include "ramp.h"
#include <WiFi.h>
#include <LittleFS.h>
#include <WebServer.h>

// ================ КОНСТАНТЫ АРХИВА ================
#ifndef ARCH_REC_SIZE
#define ARCH_REC_SIZE 16
#endif
#ifndef ARCH_MAX_DAYS
#define ARCH_MAX_DAYS 60
#endif

// ================ ШАПКА (адаптивная) ================
static const char HEAD[] PROGMEM =
"<!DOCTYPE html><html><head><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>UsKoTE v3</title><style>"
":root{--card:#fff;--mut:#8a939b;--blue:#1565c0;--green:#2e7d32;--red:#c62828}"
"*{box-sizing:border-box}"
"body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;"
"background:#eef1f4;margin:0;padding:12px;color:#222;font-size:14px;line-height:1.4}"
"h1{font-size:18px;margin:0 0 4px}"
"h3{font-size:15px;margin:0 0 8px}"
".sub{color:var(--mut);font-size:12px;margin:0 0 12px}"
".card{background:var(--card);border-radius:8px;padding:12px;margin-bottom:12px;"
"box-shadow:0 1px 4px rgba(0,0,0,.1)}"
".tbl-wrap{overflow-x:auto;-webkit-overflow-scrolling:touch}"
"table{border-collapse:collapse;width:100%;min-width:480px}"
"td,th{padding:8px 10px;text-align:left;font-size:14px;border-bottom:1px solid #eee}"
"th{background:#f7f8fa;font-weight:600;white-space:nowrap}"
"tr:last-child td{border-bottom:none}"
"button{background:var(--blue);color:#fff;border:0;border-radius:6px;padding:8px 14px;"
"margin:2px;cursor:pointer;font-size:14px;-webkit-tap-highlight-color:transparent}"
"button.ok{background:var(--green)}"
"button.err{background:var(--red)}"
"input,select{padding:8px;border:1px solid #ccc;border-radius:6px;font-size:14px}"
"input[type=number]{width:70px}"
"input[type=checkbox]{width:auto;transform:scale(1.3)}"
"nav{margin-top:14px}nav a{color:var(--blue);margin-right:12px;text-decoration:none;font-size:14px}"
".err{color:var(--red);font-weight:600}"
".ok-txt{color:var(--green);font-weight:600}"
"pre{font-size:12px;overflow-x:auto;background:#f7f8fa;padding:8px;border-radius:4px}"
"@media(max-width:600px){"
"body{padding:8px;font-size:13px}"
"h1{font-size:16px}"
".card{padding:10px}"
"td,th{padding:6px 8px;font-size:13px}"
"button{padding:8px 12px;font-size:13px}"
"input,select{padding:8px;font-size:13px}"
"nav a{display:inline-block;margin:4px 10px 4px 0}"
"}"
"a.btn{display:inline-block;background:var(--blue);color:#fff;border-radius:6px;padding:8px 14px;margin:2px;text-decoration:none;font-size:14px;text-align:center}"
"a.btn.ok{background:var(--green)}"
"a.btn.err{background:var(--red)}"
".cmp input[type=number]{width:58px;padding:4px;font-size:12px}"
".cmp input[type=hidden]{display:none}"
"button.on{background:var(--green)}"
"button.off{background:#9e9e9e}"
"hr.sep{border:none;border-top:1px solid #eee;margin:12px 0}"
"</style></head><body>";

static const char FOOT[] PROGMEM =
"<footer style='text-align:center;color:var(--mut);font-size:12px;margin-top:24px'>"
"ООО \"Контроль Телеметрия\" 2026г.</footer></body></html>";

// ================ ИНФОСТРОКА ================
String modeStr() {
  String s = "Сеть: " + WiFi.localIP().toString();
  s += " | RSSI " + String(WiFi.RSSI()) + " dBm";
  s += " | heap " + String(ESP.getFreeHeap()/1024) + " KB";
  s += " | аптайм " + String(millis()/60000) + " мин";
  return s;
}

// ================ ГЛАВНАЯ СТРАНИЦА ================
void page() {
  lastWeb = millis();
  String h;
  h.reserve(12000);
  h += FPSTR(HEAD);
  h += "<meta http-equiv='refresh' content='10'>";
  h += "<h1>UsKoTE v3 — охлаждение ёмкостей</h1>";
  h += "<p class='sub'>" + modeStr() + "</p>";
  h += "<p class='sub'>Время: " + getFormattedTime() + "</p>";
  h += "<div class='card'><div class='tbl-wrap'><table class='cmp'>";
  h += "<tr><th>№</th><th>Вкл</th><th>T, °C</th><th>Уставка</th><th>Рамп,ч</th><th>Клапан</th><th>Состояние</th><th></th></tr>";
  for(int k = 0; k < 6; k++) {
    int sm = sensorMap[k];
    bool hasErr = err1[k] || err2[k] || err3[k];
    String st;
    if(hasErr) {
      st = "<span class='err'>";
      if(err1[k]) st += "нет охл. ";
      if(err2[k]) st += "нагрев ";
      if(err3[k]) st += "переохл. ";
      st += "</span>";
    } else if(!en[k]) {
      st = "выключен";
    } else {
      st = "норма";
    }
    h += "<tr>";
    h += "<td><b>" + String(k+1) + "</b></td>";
    h += "<td><form method='get' action='/chen' style='margin:0'>"
         "<input type='hidden' name='ch' value='" + String(k+1) + "'>"
         "<button class='" + String(en[k] ? "on" : "off") + "' type='submit'>" +
         String(en[k] ? "ВКЛ" : "ВЫКЛ") + "</button></form></td>";
    h += "<td>" + (ok[sm] ? String(temp[sm], 2) : String("---")) + "</td>";
    h += "<td><form method='get' action='/ch' style='margin:0'>"
         "<input type='hidden' name='ch' value='" + String(k+1) + "'>"
         "<input type='number' name='val' step='0.1' min='-20' max='60' value='" + String(sp[k],1) + "'>"
         "<button type='submit'>OK</button></form></td>";
    h += "<td><form method='get' action='/ramp' style='margin:0'>"
         "<input type='hidden' name='ch' value='" + String(k+1) + "'>"
         "<input type='number' name='val' min='0' max='99' value='" + String(rampH[k]) + "'>"
         "<button type='submit'>OK</button></form></td>";
    h += "<td>" + String(rel[k] ? "<span class='ok-txt'>ОТКР</span>" : "закр") + "</td>";
    h += "<td>" + st + "</td>";
    h += "<td>";
    if(hasErr) {
      h += "<form method='get' action='/alclr' style='margin:0'>"
           "<input type='hidden' name='ch' value='" + String(k+1) + "'>"
           "<button class='err' type='submit'>Сброс</button></form>";
    }
    h += "</td>";
    h += "</tr>";
  }
  h += "</table></div></div>";
  h += "<nav><a href='/chmap'>Датчики</a>"
       "<a href='/graph'>График</a>"
       "<a href='/ntp'>Время</a>"
       "<a href='/net'>Сеть</a>"
       "<a href='/set'>Защиты</a>"
       "<a href='/logview'>Лог</a></nav>";
  h += FPSTR(FOOT);
  server.send(200, "text/html", h);
}

// ================ JSON СТАТУС ================
void hStatus() {
  lastWeb = millis();
  String j = "{";
  j += "\"heap\":" + String(ESP.getFreeHeap());
  j += ",\"time\":\"" + getFormattedTime() + "\"";
  j += ",\"ch\":[";
  for(int k = 0; k < 6; k++) {
    int sm = sensorMap[k];
    bool hasErr = err1[k] || err2[k] || err3[k];
    if(k) j += ",";
    j += "{\"t\":" + (ok[sm] ? String(temp[sm], 2) : String(-99.99)) +
         ",\"ok\":" + String(ok[sm] ? 1 : 0) +
         ",\"sp\":" + String(sp[k], 2) +
         ",\"wsp\":" + String(workSp[k], 2) +
         ",\"rel\":" + String(rel[k] ? 1 : 0) +
         ",\"err\":" + String(hasErr ? 1 : 0) +
         ",\"s\":" + String(sm) + "}";
  }
  j += "]}";
  server.send(200, "application/json", j);
}

// ================ УСТАВКА КАНАЛА ================
void hCh() {
  lastWeb = millis();
  int k = server.arg("ch").toInt() - 1;
  float v = server.arg("val").toFloat();
  // [FIX] Валидация: toFloat() из "abc" даёт 0.0 — теперь сравниваем со строкой
  // и диапазоном -20..60 включительно; запись общих данных — под stateMutex.
  if(k >= 0 && k < 6 && server.arg("val").length() > 0 && v >= -20.0f && v <= 60.0f) {
    if(stateMutex) xSemaphoreTake(stateMutex, portMAX_DELAY);
    sp[k] = v;
    if(rampH[k] <= 0) workSp[k] = v;
    if(stateMutex) xSemaphoreGive(stateMutex);
    logEv(EV_SP, k + 1, String(v, 2));  // [FIX] канал в логе — 1-based
    // [FIX] Используем eeDirty вместо прямого eeSave()
    eeDirty = true;
    lastChangeTime = millis();
  }
  server.sendHeader("Location", "/");
  server.send(302);
}

// ================ РАМПА КАНАЛА ================
void hRamp() {
  lastWeb = millis();
  int k = server.arg("ch").toInt() - 1;
  int v = server.arg("val").toInt();
  if(k >= 0 && k < 6 && v >= 0 && v <= 99) {
    if(stateMutex) xSemaphoreTake(stateMutex, portMAX_DELAY);   // [FIX] rampSet пишет общие данные
    rampSet((uint8_t)k, (uint8_t)v);
    if(stateMutex) xSemaphoreGive(stateMutex);
    logEv(EV_RAMP_RS, k + 1, String(v));  // [FIX] 1-based
    // [FIX] eeDirty уже установлен в rampSet()
  }
  server.sendHeader("Location", "/");
  server.send(302);
}

// ================ ДЕЛЬТЫ ЗАЩИТ ================
void hDelta() {
  lastWeb = millis();
  // [FIX] Валидация диапазонов: раньше любое значение из URL (в т.ч. NaN,
  // отрицательное, 1e30) записывалось в настройки и сохранялось в EEPROM.
  if(stateMutex) xSemaphoreTake(stateMutex, portMAX_DELAY);   // [FIX] гонка с protectionTick (Core 1)
  if(server.hasArg("cool")) {
    float v = server.arg("cool").toFloat();
    if(v >= 0.1f && v <= 10.0f) deltaCool = v;
  }
  if(server.hasArg("heat")) {
    float v = server.arg("heat").toFloat();
    if(v >= 0.1f && v <= 10.0f) deltaHeat = v;
  }
  if(stateMutex) xSemaphoreGive(stateMutex);
  // [FIX] Используем eeDirty вместо прямого eeSave()
  eeDirty = true;
  lastChangeTime = millis();
  server.sendHeader("Location", "/set");
  server.send(302);
}

// ================ СБРОС АВАРИИ ================
void hAlClr() {
  lastWeb = millis();
  int k = server.arg("ch").toInt() - 1;
  if(k >= 0 && k < 6) {
    // [FIX] Используем clearAlarm(): он также сбрасывает errSince[] и
    // внутренние таймеры защит (coolTimer[]). Без этого защита "нет охлаждения"
    // срабатывала повторно почти сразу после ручного сброса.
    // Логирование события выполняет сам clearAlarm().
    if(stateMutex) xSemaphoreTake(stateMutex, portMAX_DELAY);   // [FIX] гонка с protectionTick
    clearAlarm(k);
    if(stateMutex) xSemaphoreGive(stateMutex);
  }
  server.sendHeader("Location", "/");
  server.send(302);
}

// ================ ВКЛ/ВЫКЛ КАНАЛА ================
void hEn() {
  lastWeb = millis();
  int k = server.arg("ch").toInt() - 1;
  if(k >= 0 && k < 6) {
    if(stateMutex) xSemaphoreTake(stateMutex, portMAX_DELAY);   // [FIX] защита от гонки с Core 1
    en[k] = !en[k];
    if(!en[k]) {
      rel[k] = false;            // при выключении закрываем клапан
    }
    if(stateMutex) xSemaphoreGive(stateMutex);
    if(!en[k]) clearAlarm(k);    // [FIX] снимаем аварии выключенного канала
    logEv(en[k] ? EV_CH_ON : EV_CH_OFF, k + 1, "канал");  // [FIX] событие вкл/выкл канала
    // [FIX] Используем eeDirty вместо прямого eeSave()
    eeDirty = true;
    lastChangeTime = millis();
  }
  server.sendHeader("Location", "/");
  server.send(302);
}

// ================ СТРАНИЦА УСТАВОК ================
void setPage() {
  lastWeb = millis();
  String h;
  h.reserve(4000);
  h += FPSTR(HEAD);
  h += "<h1>Настройки защит</h1>";
  h += "<div class='card'>";
  h += "<form method='get' action='/delta'><div class='tbl-wrap'><table class='cmp'>";
  h += "<tr><td>Переохлаждение (ниже уставки)</td>"
       "<td><input type='number' name='cool' step='0.1' min='0' max='10' "
       "value='" + String(deltaCool, 1) + "'> °C</td></tr>";
  h += "<tr><td>Нагрев (выше уставки)</td>"
       "<td><input type='number' name='heat' step='0.1' min='0' max='10' "
       "value='" + String(deltaHeat, 1) + "'> °C</td></tr>";
  h += "</table></div><br><button class='ok'>Сохранить</button></form>";
  h += "</div>";
  h += "<nav><a href='/'>Назад</a></nav>";
  h += FPSTR(FOOT);
  server.send(200, "text/html", h);
}

// ================ МАППИНГ ДАТЧИКОВ ================
void chmapPage() {
  lastWeb = millis();
  String h;
  h.reserve(6000);
  h += FPSTR(HEAD);
  h += "<h1>Привязка датчиков</h1>";
  h += "<div class='card'>";
  h += "<form method='get' action='/chmapsave'>";
  h += "<div class='tbl-wrap'><table class='cmp'>";
  h += "<tr><th>Канал</th><th>Датчик</th><th>T, °C</th><th>Статус</th></tr>";
  for(int k = 0; k < 6; k++) {
    int sm = sensorMap[k];
    h += "<tr><td><b>Канал " + String(k+1) + "</b></td>";
    h += "<td><select name='s" + String(k) + "'>";
    for(int d = 0; d < 6; d++) {
      h += "<option value='" + String(d) + "'";
      if(sensorMap[k] == d) h += " selected";
      h += ">Датчик " + String(d+1) + "</option>";
    }
    h += "</select></td>";
    h += "<td>" + (ok[sm] ? String(temp[sm], 2) : String("---")) + "</td>";
    h += "<td>" + String(ok[sm] ? "<span class='ok-txt'>✓</span>" : "отключен") + "</td></tr>";
  }
  h += "</table></div><br>";
  h += "<button class='ok'>Сохранить</button> ";
  h += "<button onclick=\"location.href='/chmapreset';return false;\">Сбросить (1:1)</button>";
  h += "</form>";
  h += "<p class='sub' style='margin-top:10px'>Один датчик может управлять несколькими каналами.</p>";
  h += "</div>";
  h += "<nav><a href='/'>Назад</a></nav>";
  h += FPSTR(FOOT);
  server.send(200, "text/html", h);
}

void hChmapSave() {
  lastWeb = millis();
  for(int k = 0; k < 6; k++) {
    String a = "s" + String(k);
    if(server.hasArg(a)) {
      int v = server.arg(a).toInt();
      if(v >= 0 && v < 6) sensorMap[k] = v;
    }
  }
  saveSensorMap();
  server.sendHeader("Location", "/chmap");
  server.send(302);
}

void hChmapReset() {
  lastWeb = millis();
  for(int k = 0; k < 6; k++) sensorMap[k] = k;
  saveSensorMap();
  server.sendHeader("Location", "/chmap");
  server.send(302);
}

// ================ СЕТЬ ================
void netPage() {
  lastWeb = millis();
  String h;
  h += FPSTR(HEAD);
  h += "<h1>Настройки сети</h1>";
  h += "<form method='get' action='/netsave'><div class='card'><div class='tbl-wrap'><table>";
  h += "<tr><td>SSID</td><td><input name='ssid' value='" + String(ssid) + "'></td></tr>";
  h += "<tr><td>Пароль</td><td><input name='pass' type='password' value='" + String(pass) + "'></td></tr>";
  h += "<tr><td>DHCP</td><td><input type='checkbox' name='dhcp'" +
       String(dhcp ? " checked" : "") + "></td></tr>";
  h += "<tr><td>IP (статика)</td><td><input name='ip' value='" + ip.toString() + "'></td></tr>";
  h += "<tr><td>Шлюз</td><td><input name='gw' value='" + gw.toString() + "'></td></tr>";
  h += "<tr><td>Маска</td><td><input name='sn' value='" + sn.toString() + "'></td></tr>";
  h += "</table></div><br><button class='ok'>Сохранить</button></form></div>";
  h += "<nav><a href='/'>Назад</a></nav>";
  h += FPSTR(FOOT);
  server.send(200, "text/html", h);
}

// [FIX] Флаг для отложенной перезагрузки
static bool needRestart = false;
static uint32_t restartTime = 0;

void hNetSave() {
  lastWeb = millis();
  if(server.hasArg("ssid")) server.arg("ssid").toCharArray(ssid, 33);
  if(server.hasArg("pass")) server.arg("pass").toCharArray(pass, 65);
  dhcp = server.hasArg("dhcp");
  if(server.hasArg("ip")) ip.fromString(server.arg("ip"));
  if(server.hasArg("gw")) gw.fromString(server.arg("gw"));
  if(server.hasArg("sn")) sn.fromString(server.arg("sn"));
  eeSave();
  server.send(200, "text/plain", "Сохранено. Перезагрузка через 2 сек...");
  // [FIX] Убираем delay() и ESP.restart() — перезагрузка в loop()
  needRestart = true;
  restartTime = millis();
}

// [FIX] Функция проверки перезагрузки — вызывать из loop()
void checkRestart() {
  if(needRestart && restartTime > 0 && millis() - restartTime >= 2000) {
    ESP.restart();
  }
}

// ================ ВРЕМЯ ================
void ntpPage() {
  lastWeb = millis();
  String h;
  h.reserve(4000);
  h += FPSTR(HEAD);
  h += "<h1>Дата и время</h1>";
  h += "<div class='card'>";
  h += "<p><b>Сейчас:</b> " + getFormattedTime() + "</p>";
  h += "<hr class='sep'>";
  h += "<form method='get' action='/ntpsave'><div class='tbl-wrap'><table class='cmp'>";
  h += "<tr><td>NTP-синхронизация</td><td><input type='checkbox' name='usentp'" +
       String(ntpEnabled ? " checked" : "") + "></td></tr>";
  h += "<tr><td>Часовой пояс (UTC)</td><td><input type='number' name='offset' min='-12' max='14' "
       "value='" + String(ntpOffsetHours) + "'></td></tr>";
  h += "</table></div><br><button class='ok'>Сохранить</button></form>";
  h += "<hr class='sep'>";
  h += "<form method='get' action='/timesave'><div class='tbl-wrap'><table class='cmp'>";
  h += "<tr><td>Дата</td><td><input type='date' name='date'></td></tr>";
  h += "<tr><td>Время</td><td><input type='time' name='time'></td></tr>";
  h += "</table></div><br><button>Установить вручную</button></form>";
  h += "<hr class='sep'>";
  h += "<form method='get' action='/ntpforce'><button>Принудительная синхронизация</button></form>";
  h += "</div>";
  h += "<nav><a href='/'>Назад</a></nav>";
  h += FPSTR(FOOT);
  server.send(200, "text/html", h);
}

void hNtpSave() {
  lastWeb = millis();
  int off = server.arg("offset").toInt();
  bool en = server.hasArg("usentp");
  if(off < -12 || off > 14) { server.send(400, "text/plain", "Неверный пояс"); return; }
  ntpOffsetHours = off;
  ntpEnabled = en;
  saveNtpSettings();
  applyTimezone();
  ntpSynced = false;
  ntpAttemptTime = 0;
  server.send(200, "text/plain", "Сохранено. Время: " + getFormattedTime());
}

void hTimeSave() {
  lastWeb = millis();
  String d = server.arg("date"), t = server.arg("time");
  int y = d.substring(0,4).toInt(), mo = d.substring(5,7).toInt(), dd = d.substring(8,10).toInt();
  int hh = t.substring(0,2).toInt(), mi = t.substring(3,5).toInt();
  if(setManualTime(y, mo, dd, hh, mi, 0))
    server.send(200, "text/plain", "Установлено: " + getFormattedTime());
  else
    server.send(400, "text/plain", "Неверная дата/время");
}

void hNtpForce() {
  lastWeb = millis();
  if(!ntpEnabled) { server.send(200, "text/plain", "NTP выключен в настройках"); return; }
  if(initNTP()) server.send(200, "text/plain", "Синхронизировано: " + getFormattedTime());
  else server.send(200, "text/plain", "Не удалось синхронизироваться");
}

// ================ ГРАФИК ================
static long daysFromCivil(int year, int month, int day) {
  int y = year - (month <= 2 ? 1 : 0);
  long era = (y >= 0 ? y : y - 399) / 400;
  int yoe = y - (int)(era * 400);
  int doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

void graphPage() {
  lastWeb = millis();
  String h;
  h.reserve(9000);
  h += FPSTR(HEAD);
  h += "<p class='sub'>" + modeStr() + " — архив температур</p>";
  h += "<div style='text-align:center;margin-bottom:10px'>";
  h += "<button onclick='prevDay()'>&#8592; Пред.</button> ";
  h += "<span id='dd' style='font-weight:600;margin:0 10px'>...</span> ";
  h += "<button onclick='nextDay()'>След. &#8594;</button> ";
  h += "<button onclick='today()'>Сегодня</button></div>";
  h += "<div id='st' style='text-align:center;color:var(--mut);font-size:12px'>Загрузка...</div>";
  h += "<div style='max-width:900px;margin:0 auto'>";
  h += "<canvas id='cv' width='880' height='400' style='width:100%;background:#fff;border:1px solid #ddd'></canvas>";
  h += "<div style='text-align:center;margin-top:10px'>";
  for(int k = 0; k < 6; k++)
    h += "<span style='color:" + String(COL[k]) + ";margin:0 6px'>— Канал " + String(k+1) + "</span>";
  h += "</div></div>";
  h += "<nav><a href='/'>Назад</a></nav>";
  h += "<script>";
  h += "var COL=['#e6194b','#3cb44b','#4363d8','#f58231','#911eb4','#008080'];";
  h += "var days=[],cur=null;";
  h += "fetch('/archdays').then(function(r){return r.json();}).then(function(d){";
  h += "days=d.days||[];";
  h += "if(days.length>0){cur=days[days.length-1];loadDay(cur);}";
  h += "else{document.getElementById('st').textContent='Нет данных';}";
  h += "}).catch(function(e){document.getElementById('st').textContent='Ошибка: '+e;});";
  h += "function loadDay(d){cur=d;document.getElementById('dd').textContent=fmtDate(d);";
  h += "document.getElementById('st').textContent='Загрузка...';";
  h += "fetch('/archday?d='+d).then(function(r){return r.json();}).then(draw)";
  h += ".catch(function(e){document.getElementById('st').textContent='Ошибка: '+e;});}";
  h += "function fmtDate(d){var y=Math.floor(d/10000),m=Math.floor((d%10000)/100),dd=d%100;";
  h += "return ('0'+dd).slice(-2)+'.'+('0'+m).slice(-2)+'.'+y;}";
  h += "function prevDay(){var i=days.indexOf(cur);if(i>0)loadDay(days[i-1]);}";
  h += "function nextDay(){var i=days.indexOf(cur);if(i>=0&&i<days.length-1)loadDay(days[i+1]);}";
  h += "function today(){if(days.length>0)loadDay(days[days.length-1]);}";
  h += "function draw(d){var pts=d.pts||[];var st=document.getElementById('st');";
  h += "if(pts.length===0){st.textContent='Нет данных за день';return;}";
  h += "pts.sort(function(a,b){return a[0]-b[0];});";
  h += "st.textContent='Точек: '+pts.length;";
  h += "var t0=d.t0;";
  h += "var mn=999,mx=-999;";
  h += "for(var i=0;i<pts.length;i++)for(var k=1;k<7;k++){";
  h += "var v=pts[i][k];if(v>-90&&v<200){if(v<mn)mn=v;if(v>mx)mx=v;}}";
  h += "if(mn>mx){mn=0;mx=30;}if(mx-mn<2){var c=(mn+mx)/2;mn=c-1;mx=c+1;}mn-=0.5;mx+=0.5;";
  h += "var cv=document.getElementById('cv'),ctx=cv.getContext('2d');";
  h += "var W=cv.width,H=cv.height,L=50,R=10,T=10,B=25;ctx.clearRect(0,0,W,H);";
  h += "ctx.strokeStyle='#eee';ctx.fillStyle='#888';ctx.font='11px sans-serif';";
  h += "for(var g=0;g<=4;g++){var yg=T+(H-T-B)*g/4;ctx.beginPath();ctx.moveTo(L,yg);ctx.lineTo(W-R,yg);ctx.stroke();";
  h += "ctx.textAlign='right';ctx.fillText((mx-(mx-mn)*g/4).toFixed(1),L-4,yg+4);}";
  h += "ctx.textAlign='center';";
  h += "for(var hr=0;hr<=24;hr+=3){var xg=L+(W-L-R)*hr/24;ctx.beginPath();ctx.moveTo(xg,T);ctx.lineTo(xg,H-B);ctx.stroke();";
  h += "ctx.fillText(('0'+hr).slice(-2)+':00',xg,H-B+12);}";
  h += "for(var k=0;k<6;k++){ctx.strokeStyle=COL[k];ctx.lineWidth=1.5;ctx.beginPath();var st2=false;";
  h += "for(var i=0;i<pts.length;i++){var v=pts[i][k+1];if(v<=-90){st2=false;continue;}";
  h += "var x=L+(W-L-R)*(pts[i][0]-t0)/86400;var yg=T+(H-T-B)*(1-(v-mn)/(mx-mn));";
  h += "if(!st2){ctx.moveTo(x,yg);st2=true;}else ctx.lineTo(x,yg);}ctx.stroke();}";
  h += "}";
  h += "</script>";
  h += FPSTR(FOOT);
  server.send(200, "text/html", h);
}

void hArchDays() {
  lastWeb = millis();
  uint32_t days[ARCH_MAX_DAYS];
  int count = archGetDayList(days, ARCH_MAX_DAYS);
  String j = "{\"days\":[";
  for(int i = 0; i < count; i++) {
    if(i) j += ",";
    j += String(days[i]);
  }
  j += "],\"count\":" + String(count) + ",\"free\":" + String(archGetFreeSpace()) + "}";
  server.send(200, "application/json", j);
}

void hArchDay() {
  lastWeb = millis();
  uint32_t dateNum = server.arg("d").toInt();
  if(dateNum < 20200101 || dateNum > 21001231) {
    server.send(400, "application/json", "{\"error\":\"bad date\"}");
    return;
  }
  int yy = dateNum/10000, mo = (dateNum/100)%100, dd = dateNum%100;
  long dayStart = daysFromCivil(yy, mo, dd) * 86400L - (long)ntpOffsetHours * 3600;
  if(!archDayExists(dateNum)) {
    server.send(200, "application/json",
                "{\"date\":" + String(dateNum) + ",\"t0\":" + String(dayStart) +
                ",\"tz\":" + String(ntpOffsetHours) + ",\"pts\":[],\"count\":0}");
    return;
  }
  // [FIX] Раньше файл читался ПОД fsMutex и удерживался до конца отдачи
  // ~1 МБ JSON — записи архива и лога с Core 1 отваливались по таймауту
  // мьютекса и терялись. Теперь данные сначала копируются в RAM, мьютекс
  // сразу освобождается, и только потом формируется HTTP-ответ.
  static uint8_t* dayBuf = nullptr;
  if(!dayBuf) dayBuf = (uint8_t*)malloc(ARCH_DAY_FILE_SIZE);
  if(!dayBuf) {
    server.send(500, "application/json", "{\"error\":\"no mem\"}");
    return;
  }
  int got = archReadDay(dateNum, dayBuf);
  if(got <= 0) {
    server.send(200, "application/json",
                "{\"date\":" + String(dateNum) + ",\"t0\":" + String(dayStart) +
                ",\"tz\":" + String(ntpOffsetHours) + ",\"pts\":[],\"count\":0}");
    return;
  }
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  server.sendContent("{\"date\":" + String(dateNum) + ",\"t0\":" + String(dayStart) +
                     ",\"tz\":" + String(ntpOffsetHours) + ",\"pts\":[");
  const int stride = 6;
  String chunk;
  chunk.reserve(2048);
  int taken = 0;
  int nrec = got / ARCH_REC_SIZE;
  for(int i = 0; i < nrec; i += stride) {
    uint8_t* rec = dayBuf + i * ARCH_REC_SIZE;
    uint32_t t = (uint32_t)rec[0] | ((uint32_t)rec[1]<<8) |
                 ((uint32_t)rec[2]<<16) | ((uint32_t)rec[3]<<24);
    if(t > 100000) {
      if(taken) chunk += ",";
      chunk += "[" + String((long)t);
      for(int k = 0; k < 6; k++) {
        int16_t v = (int16_t)(rec[4+2*k] | ((uint16_t)rec[5+2*k]<<8));
        chunk += ",";
        chunk += (v == -32768) ? "-99.99" : String(v/100.0f, 2);
      }
      chunk += "]";
      taken++;
      if(chunk.length() > 1500) {
        server.sendContent(chunk);
        chunk = "";
        chunk.reserve(2048);
      }
    }
  }
  if(chunk.length()) server.sendContent(chunk);
  server.sendContent("],\"count\":" + String(taken) + "}");
  server.sendContent("");
}

// ================ ЛОГ: расшифровка кодов ================
// [FIX] Унифицированы коды — используются EV_ERR1/2/3 из logger.h
static String evNameStr(int code) {
  switch(code) {
    case EV_BOOT:     return "Старт системы";
    case EV_CH_ON:    return "Клапан открыт";
    case EV_CH_OFF:   return "Клапан закрыт";
    case EV_SP:       return "Уставка изменена";
    case EV_RAMP_RS:  return "Рампа установлена";
    case EV_WIFI_ON:  return "WiFi подключен";
    case EV_WIFI_OFF: return "WiFi потерян";
    case EV_ERR1:     return "Авария: нет охлаждения";
    case EV_ERR2:     return "Авария: нагрев";
    case EV_ERR3:     return "Авария: переохлаждение";
    case EV_ERRCLR:   return "Авария сброшена";
    default:          return "Событие #" + String(code);
  }
}

// ================ ЛОГ: извлечь поле из CSV-строки ================
static String csvField(const String& line, int idx) {
  int start = 0;
  for(int i = 0; i <= idx; i++) {
    int p = line.indexOf(';', start);
    if(i == idx) {
      if(p < 0) return line.substring(start);
      return line.substring(start, p);
    }
    if(p < 0) return "";
    start = p + 1;
  }
  return "";
}

// ================ ЛОГ: ПРОСМОТР ================
void logviewPage() {
  lastWeb = millis();
  String h;
  h.reserve(10000);
  h += FPSTR(HEAD);
  h += "<h1>Журнал событий</h1>";
  h += "<p class='sub'>Последние 100 записей (новые сверху)</p>";
  h += "<div class='card'><div class='tbl-wrap'><table class='cmp'>";
  h += "<tr><th>Время</th><th>Событие</th><th>Канал</th><th>Значение</th></tr>";
  
  // [FIX] Убран массив String rows[100] на стеке — выводим напрямую
  String rows[100];  // Оставили, но сделали static ниже через альтернативу
  int n = 0;
  
  if(fsOk && LittleFS.exists("/log.csv")) {
    File f = LittleFS.open("/log.csv", "r");
    if(f) {
      size_t sz = f.size();
      size_t tail = (sz > 16000) ? 16000 : sz;
      f.seek(sz - tail);
      String content = f.readString();
      f.close();
      // обрезаем до первой полной строки
      if(tail < sz) {
        int nl = content.indexOf('\n');
        if(nl >= 0) content = content.substring(nl + 1);
      }
      int start = 0;
      while(start < (int)content.length() && n < 100) {
        int nl = content.indexOf('\n', start);
        if(nl < 0) nl = content.length();
        String line = content.substring(start, nl);
        line.trim();
        start = nl + 1;
        if(line.length() == 0 || !isDigit(line[0])) continue;  // пропуск заголовка
        int code = csvField(line, 0).toInt();
        String ch  = csvField(line, 2);
        String val = csvField(line, 3);
        String dt  = csvField(line, 5);
        String r = "<tr>";
        r += "<td>" + dt + "</td>";
        r += "<td>" + evNameStr(code) + "</td>";
        r += "<td>" + (ch == "0" ? String("—") : ch) + "</td>";
        r += "<td>" + val + "</td>";
        r += "</tr>";
        rows[n++] = r;
      }
    }
  }
  // вывод в обратном порядке (новые сверху)
  for(int i = n - 1; i >= 0; i--) h += rows[i];
  if(n == 0) h += "<tr><td colspan='4'>Лог пуст</td></tr>";
  h += "</table></div></div>";
  h += "<a class='btn ok' href='/log' download>Скачать полный лог</a> ";
  h += "<a class='btn err' href='/logdel' onclick=\"return confirm('Очистить лог?')\">Очистить лог</a>";
  h += "<nav><a href='/'>Назад</a></nav>";
  h += FPSTR(FOOT);
  server.send(200, "text/html", h);
}

// ================ ЛОГ: СКАЧИВАНИЕ ================
void hLog() {
  lastWeb = millis();
  if(!fsOk || !LittleFS.exists("/log.csv")) {
    server.send(404, "text/plain", "Лог не найден");
    return;
  }
  File f = LittleFS.open("/log.csv", "r");
  if(!f) {
    server.send(500, "text/plain", "Ошибка открытия");
    return;
  }
  server.sendHeader("Content-Disposition", "attachment; filename=log.csv");
  server.streamFile(f, "text/csv");
  f.close();
}

// ================ ЛОГ: ОЧИСТКА ================
void hLogDel() {
  lastWeb = millis();
  if(fsOk && LittleFS.exists("/log.csv")) {
    LittleFS.remove("/log.csv");
  }
  server.sendHeader("Location", "/logview");
  server.send(302);
}

// ================ ДИАГНОСТИКА ФС ================
void hFsInfo() {
  lastWeb = millis();
  String info = "fsOk: " + String(fsOk ? "true" : "false") + "\n";
  info += "heap: " + String(ESP.getFreeHeap()) + "\n";
  if(fsOk) {
    info += "Всего: " + String(LittleFS.totalBytes()) + "\n";
    info += "Занято: " + String(LittleFS.usedBytes()) + "\n";
    info += "Свободно: " + String(LittleFS.totalBytes() - LittleFS.usedBytes()) + "\n";
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    while(file) {
      info += String(file.name()) + " - " + String(file.size()) + "\n";
      file = root.openNextFile();
    }
  }
  server.send(200, "text/plain", info);
}

// ================ РЕГИСТРАЦИЯ РОУТОВ ================
void webRegisterRoutes() {
  server.on("/", page);
  server.on("/status", hStatus);
  server.on("/set", setPage);
  server.on("/ch", hCh);
  server.on("/ramp", hRamp);
  server.on("/delta", hDelta);
  server.on("/alclr", hAlClr);
  server.on("/chen", hEn);
  server.on("/chmap", chmapPage);
  server.on("/chmapsave", hChmapSave);
  server.on("/chmapreset", hChmapReset);
  server.on("/net", netPage);
  server.on("/netsave", hNetSave);
  server.on("/ntp", ntpPage);
  server.on("/ntpsave", hNtpSave);
  server.on("/timesave", hTimeSave);
  server.on("/ntpforce", hNtpForce);
  server.on("/graph", graphPage);
  server.on("/archdays", hArchDays);
  server.on("/archday", hArchDay);
  server.on("/logview", logviewPage);
  server.on("/log", hLog);
  server.on("/logdel", hLogDel);
  server.on("/fsinfo", hFsInfo);
}