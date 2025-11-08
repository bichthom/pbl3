let mqttClient;

window.addEventListener("load", (event) => {
  connectToBroker();

  const subscribeBtn = document.querySelector("#subscribe");
  subscribeBtn.addEventListener("click", function () {
    subscribeToTopic();
  });
const unsubscribeBtn = document.querySelector("#unsubscribe");
  unsubscribeBtn.addEventListener("click", function () {
    unsubscribeToTopic();
  });
  document.querySelector("#lightOn").onclick = () => publishControl("light", "ON");
  document.querySelector("#lightOff").onclick = () => publishControl("light", "OFF");
  document.querySelector("#fanOn").onclick = () => publishControl("fan", "ON");
  document.querySelector("#fanOff").onclick = () => publishControl("fan", "OFF");
});
function connectToBroker() {
  const clientId = "client" + Math.random().toString(36).substring(7);
// Change this to point to your MQTT broker
  const host = 'ws://broker.emqx.io:8083/mqtt'

  const options = {
    keepalive: 60,
    clientId: clientId,
    protocolId: "MQTT",
    protocolVersion: 5,
    clean: true,
    reconnectPeriod: 1000,
    connectTimeout: 30 * 1000,
  };
  mqttClient = mqtt.connect(host, options);
  mqttClient.on("error", (err) => {
    console.log("Error: ", err);
    mqttClient.end();
  });

  mqttClient.on("reconnect", () => {
    console.log("Reconnecting...");
  });

  mqttClient.on("connect", () => {
    console.log("Client connected:" + clientId);
  });

  // Received
 mqttClient.on("message", (topic, message) => {
  const msg = message.toString();
  console.log("Received:", msg, "Topic:", topic);

  // Hiện log vào textarea
  const messageTextArea = document.querySelector("#message");
  messageTextArea.value += msg + "\n";

  let data;
  try {
    data = JSON.parse(msg);
  } catch (e) {
    console.warn("⚠️ Message is not JSON → ignored");
    return;
  }

  // === ĐỘ ẨM ===
  if (data.temperature !== undefined) {
    document.querySelector("#temperature").textContent = data.temperature + " °C";
  }

  // === SỐ NGƯỜI ===
  if (data.people !== undefined || data.songuoi !== undefined) {
    const ppl = data.people ?? data.songuoi;
    document.querySelector("#songuoi").textContent = ppl;
  }

  // === ĐÈN ===
if (data.den !== undefined || data.light !== undefined) {
  const val = (data.den ?? data.light);
  const state = (val === "ON" || val === true) ? "ON" : "OFF";
  updateStatus("lightStatus", state);
}

// === QUẠT ===
if (data.fan !== undefined) {
  const val = data.fan;
  const state = (val === "ON" || val === true) ? "ON" : "OFF";
  updateStatus("fanStatus", state);
}
// === BIỂU ĐỒ ===
if (window.envChart) {
  const now = new Date().toLocaleTimeString();

  // Cập nhật độ ẩm
  if (data.temperature !== undefined) {
    window.envChart.data.labels.push(now);
    window.envChart.data.datasets[0].data.push(data.remperature);
  }

  // Cập nhật số người
  const ppl = data.people ?? data.songuoi;
  if (ppl !== undefined) {
    window.envChart.data.datasets[1].data.push(ppl);
  }

  // Giữ tối đa 20 điểm để biểu đồ không dài quá
  if (window.envChart.data.labels.length > 20) {
    window.envChart.data.labels.shift();
    window.envChart.data.datasets[0].data.shift();
    window.envChart.data.datasets[1].data.shift();
  }

  window.envChart.update();
}

});


}
function subscribeToTopic() {
  const status = document.querySelector("#status");
  const topic = document.querySelector("#topic").value.trim();
  console.log(`Subscribing to Topic: ${topic}`);

  mqttClient.subscribe(topic, { qos: 0 });
  status.style.color = "green";
  status.value = "SUBSCRIBED";
}

function unsubscribeToTopic() {
  const status = document.querySelector("#status");
  const topic = document.querySelector("#topic").value.trim();
  console.log(`Unsubscribing to Topic: ${topic}`);

  mqttClient.unsubscribe(topic, { qos: 0 });
  status.style.color = "red";
  status.value = "UNSUBSCRIBED";
}
function publishControl(device, state) {
  const topic = document.querySelector("#topic").value.trim();
  if (!topic) return alert("⚠️ Vui lòng nhập topic trước khi gửi lệnh!");
  const msg = JSON.stringify({ [device]: state });
  mqttClient.publish(topic, msg);
  console.log("📤 Sent:", msg);
  const log = document.querySelector("#message");
  log.value += `> Sent: ${msg}\n`;
    if (device === "light") updateStatus("lightStatus", state);
  if (device === "fan") updateStatus("fanStatus", state);
}
// Hàm cập nhật giao diện ON/OFF
function updateStatus(id, val) {
  const el = document.getElementById(id);
  el.textContent = val;
  el.className = "value " + (val === "ON" ? "badge-on" : "badge-off");
}
