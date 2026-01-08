const express = require("express");
const { SerialPort } = require("serialport");

const app = express();
app.use(express.json());

// 🔧 CHANGE THIS
const SERIAL_PORT = "/dev/ttyUSB0"; // Mac: /dev/cu.usbserial-xxxx
const BAUD_RATE = 115200;

// --- SERIAL SETUP ---
const port = new SerialPort({
  path: SERIAL_PORT,
  baudRate: BAUD_RATE,
  autoOpen: false
});

port.open(err => {
  if (err) {
    console.error("❌ Serial open failed:", err.message);
    process.exit(1);
  }
  console.log("✅ Serial connected");
});

// --- API ENDPOINT ---
let cooldown = false;

app.post("/api/hit", (req, res) => {
  if (cooldown) {
    return res.status(429).json({ status: "cooldown" });
  }

  console.log("🎯 HIT received from web");

  port.write("HIT\n", err => {
    if (err) {
      console.error("❌ Serial write failed:", err.message);
      return res.status(500).json({ status: "serial_error" });
    }

    cooldown = true;
    setTimeout(() => cooldown = false, 1000);

    res.json({ status: "sent" });
  });
});

// --- START SERVER ---
app.listen(3000, () => {
  console.log("🚀 Server running on http://localhost:3000");
});
