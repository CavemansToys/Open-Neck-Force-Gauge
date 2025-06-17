#ifndef HTML_H
#define HTML_H

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>Force Gauge</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body {
      font-family: Arial, sans-serif;
      text-align: center;
      margin: 0;
      padding: 20px;
      background-color: #f0f0f0;
    }
    .container {
      max-width: 800px;
      margin: 0 auto;
      background-color: white;
      padding: 20px;
      border-radius: 10px;
      box-shadow: 0 2px 5px rgba(0,0,0,0.1);
    }
    .value {
      font-size: 48px;
      font-weight: bold;
      margin: 20px 0;
      color: #333;
    }
    .button {
      background-color: #4CAF50;
      border: none;
      color: white;
      padding: 15px 32px;
      text-align: center;
      text-decoration: none;
      display: inline-block;
      font-size: 16px;
      margin: 10px 5px;
      cursor: pointer;
      border-radius: 4px;
      transition: background-color 0.3s;
    }
    .button:hover {
      background-color: #45a049;
    }
    .button-blue {
      background-color: #2196F3;
    }
    .button-blue:hover {
      background-color: #1976D2;
    }
    .button-red {
      background-color: #f44336;
    }
    .button-red:hover {
      background-color: #d32f2f;
    }
    .section {
      margin: 20px 0;
      padding: 20px;
      border-top: 1px solid #eee;
    }
    h1 {
      color: #333;
      margin-bottom: 30px;
    }
    h2 {
      color: #666;
      margin: 20px 0;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>Force Gauge</h1>
    
    <div class="section">
      <div class="value" id="force">0.0</div>
      <button class="button" onclick="tare()">Tare</button>
    </div>

    <div class="section">
      <h2>Data Logging</h2>
      <button class="button" id="logButton" onclick="toggleLogging()">Start Logging</button>
      <div id="logStatus" style="margin-top: 10px; color: #666;">Logging: Stopped</div>
    </div>

    <div class="section">
      <h2>Data Management</h2>
      <button class="button button-blue" onclick="viewData()">View Current Data</button>
      <button class="button button-blue" onclick="downloadData()">Download Current Data</button>
      <button class="button button-red" onclick="viewBackup()">View Backup Data</button>
      <button class="button button-red" onclick="downloadBackup()">Download Backup Data</button>
    </div>

    <div class="section">
      <h2>System</h2>
      <button class="button" onclick="updateFirmware()">Update Firmware</button>
    </div>
  </div>

  <script>
    let isLogging = false;
    
    function toggleLogging() {
      const button = document.getElementById('logButton');
      const status = document.getElementById('logStatus');
      
      fetch('/toggle-logging')
        .then(response => response.text())
        .then(data => {
          isLogging = data === 'started';
          button.textContent = isLogging ? 'Stop Logging' : 'Start Logging';
          button.className = isLogging ? 'button button-red' : 'button';
          status.textContent = 'Logging: ' + (isLogging ? 'Active' : 'Stopped');
          status.style.color = isLogging ? '#4CAF50' : '#666';
        });
    }

    function updateForce() {
      fetch('/force')
        .then(response => response.text())
        .then(data => {
          document.getElementById('force').innerHTML = data;
        });
    }
    
    function tare() {
      fetch('/tare')
        .then(response => response.text())
        .then(data => {
          console.log('Tare complete');
        });
    }

    function viewData() {
      window.open('/view-data', '_blank');
    }

    function downloadData() {
      window.location.href = '/download';
    }

    function viewBackup() {
      window.open('/view-datab', '_blank');
    }

    function downloadBackup() {
      window.location.href = '/downloadb';
    }

    function updateFirmware() {
      window.location.href = '/update';
    }
    
    setInterval(updateForce, 100);
  </script>
</body>
</html>
)rawliteral";

#endif 