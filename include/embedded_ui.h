#pragma once

// Web pages embedded in the firmware flash (PROGMEM) instead of being
// shipped in a LittleFS filesystem image. Removing LittleFS eliminates
// the separate filesystem partition, the buildfs/uploadfs CI steps, and
// the runtime mount/unmount code, while keeping the exact same serving
// flow: the device hosts the configuration UI on itself.
//
// The pages are raw string literals so they keep their formatting; the
// AsyncWebServer serves them straight from flash via send_P() (no heap
// copy at request time).

namespace cryptoapp {

// Configuration page for the ticker list (served at CONFIG_PATH).
const char TICKER_CONFIG_HTML[] PROGMEM = R"TICKER(<!DOCTYPE html>
<html lang="en">

<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Crypto Ticker Configuration</title>
    <style>
        :root {
            color-scheme: dark;
        }

        * {
            box-sizing: border-box;
        }

        body {
            font-family: system-ui, -apple-system, sans-serif;
            margin: 0;
            padding: 20px;
            background: #121417;
            color: #e8eaed;
            max-width: 480px;
            margin: 0 auto;
        }

        h1 {
            font-size: 1.4rem;
            text-align: center;
        }

        .card {
            background: #1c1f24;
            border-radius: 12px;
            padding: 16px;
            margin-bottom: 16px;
            border: 1px solid #2a2e35;
        }

        h2 {
            font-size: 1rem;
            margin: 0 0 12px;
            color: #9aa0a6;
            font-weight: 600;
        }

        .ticker {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 10px 12px;
            border: 1px solid #2a2e35;
            border-radius: 8px;
            margin-bottom: 8px;
            background: #23272e;
        }

        .ticker .label {
            font-weight: 600;
            font-size: 1.05rem;
        }

        .ticker .meta {
            color: #9aa0a6;
            font-size: 0.85rem;
        }

        .ticker .actions {
            display: flex;
            gap: 6px;
        }

        .ticker .actions button {
            background: none;
            border: 1px solid #2a2e35;
            color: #9aa0a6;
            border-radius: 6px;
            padding: 4px 8px;
            cursor: pointer;
            font-size: 0.85rem;
        }

        .ticker .actions button:hover {
            border-color: #4d90f0;
            color: #e8eaed;
        }

        .ticker .actions button.remove:hover {
            border-color: #e05c5c;
            color: #e05c5c;
        }

        input[type="text"],
        select {
            width: 100%;
            padding: 12px;
            border-radius: 8px;
            border: 1px solid #2a2e35;
            background: #23272e;
            color: #e8eaed;
            font-size: 1rem;
            margin: 4px 0 12px;
        }

        .row {
            display: flex;
            gap: 8px;
        }

        .row input {
            flex: 1;
        }

        button.primary {
            width: 100%;
            padding: 14px;
            border: none;
            border-radius: 8px;
            background: #4d90f0;
            color: #fff;
            font-size: 1rem;
            font-weight: 600;
            cursor: pointer;
        }

        button.primary:hover {
            background: #3b7ae0;
        }

        button.primary:disabled {
            background: #3a4150;
            cursor: not-allowed;
        }

        button.secondary {
            width: 100%;
            padding: 10px;
            border: 1px solid #2a2e35;
            border-radius: 8px;
            background: none;
            color: #9aa0a6;
            font-size: 0.9rem;
            cursor: pointer;
            margin-top: 8px;
        }

        button.secondary:hover {
            border-color: #4d90f0;
            color: #e8eaed;
        }

        #status {
            text-align: center;
            margin-top: 12px;
            font-size: 0.9rem;
            min-height: 1.2em;
        }

        #status.error {
            color: #e05c5c;
        }

        #status.ok {
            color: #4caf50;
        }
    </style>
</head>

<body>
    <h1>Crypto Ticker Configuration</h1>

    <div class="card">
        <h2>Configured Tickers</h2>
        <div id="tickers">
            <p>Loading...</p>
        </div>
    </div>

    <div class="card">
        <h2>Add Ticker</h2>
        <input type="text" id="label" placeholder="Display label (e.g. BTC)" autocomplete="off">
        <input type="text" id="apiId" placeholder="CoinGecko ID (e.g. bitcoin)" autocomplete="off">
        <select id="quote">
            <option value="usd">USD</option>
            <option value="usdc">USDC</option>
            <option value="eur">EUR</option>
            <option value="gbp">GBP</option>
        </select>
        <button class="primary" onclick="addTicker()">Add Ticker</button>
        <div id="status"></div>
    </div>

    <div class="card">
        <button class="secondary" onclick="resetTickers()">Reset to Defaults</button>
    </div>

    <script>
        let tickers = [];

        async function loadTickers() {
            const el = document.getElementById('tickers');
            el.innerHTML = '<p>Loading...</p>';
            try {
                const r = await fetch('/api/tickers');
                tickers = await r.json();
                renderTickers();
            } catch (e) {
                el.innerHTML = '<p>Failed to load tickers.</p>';
            }
        }

        function renderTickers() {
            const el = document.getElementById('tickers');
            if (!tickers.length) {
                el.innerHTML = '<p>No tickers configured.</p>';
                return;
            }
            el.innerHTML = tickers.map((t, i) => `
                <div class="ticker">
                    <div>
                        <div class="label">${t.label} / ${t.quote.toUpperCase()}</div>
                        <div class="meta">${t.apiId}</div>
                    </div>
                    <div class="actions">
                        <button onclick="moveTicker(${i}, ${i - 1})" ${i === 0 ? 'disabled' : ''}>&#9650;</button>
                        <button onclick="moveTicker(${i}, ${i + 1})" ${i === tickers.length - 1 ? 'disabled' : ''}>&#9660;</button>
                        <button class="remove" onclick="removeTicker(${i})">Remove</button>
                    </div>
                </div>
            `).join('');
        }

        function setStatus(msg, isError) {
            const el = document.getElementById('status');
            el.textContent = msg;
            el.className = isError ? 'error' : 'ok';
        }

        async function addTicker() {
            const label = document.getElementById('label').value.trim();
            const apiId = document.getElementById('apiId').value.trim();
            const quote = document.getElementById('quote').value;

            if (!label || !apiId) {
                setStatus('Enter a label and CoinGecko ID.', true);
                return;
            }

            try {
                const r = await fetch('/api/tickers', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                    body: 'label=' + encodeURIComponent(label) +
                        '&apiId=' + encodeURIComponent(apiId) +
                        '&quote=' + encodeURIComponent(quote)
                });
                if (r.ok) {
                    tickers = await r.json();
                    renderTickers();
                    document.getElementById('label').value = '';
                    document.getElementById('apiId').value = '';
                    setStatus('Ticker added.', false);
                } else {
                    const t = await r.text();
                    setStatus('Failed: ' + t, true);
                }
            } catch (e) {
                setStatus('Request failed.', true);
            }
        }

        async function removeTicker(id) {
            try {
                const r = await fetch('/api/tickers?id=' + id, { method: 'DELETE' });
                if (r.ok) {
                    tickers = await r.json();
                    renderTickers();
                    setStatus('Ticker removed.', false);
                } else {
                    setStatus('Failed to remove ticker.', true);
                }
            } catch (e) {
                setStatus('Request failed.', true);
            }
        }

        async function moveTicker(from, to) {
            if (to < 0 || to >= tickers.length) return;
            try {
                const r = await fetch('/api/tickers/move', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                    body: 'from=' + from + '&to=' + to
                });
                if (r.ok) {
                    tickers = await r.json();
                    renderTickers();
                }
            } catch (e) {
                setStatus('Request failed.', true);
            }
        }

        async function resetTickers() {
            try {
                const r = await fetch('/api/tickers/reset', { method: 'POST' });
                if (r.ok) {
                    tickers = await r.json();
                    renderTickers();
                    setStatus('Reset to defaults.', false);
                }
            } catch (e) {
                setStatus('Request failed.', true);
            }
        }

        loadTickers();
    </script>
</body>

</html>
)TICKER";

// WiFi captive-portal page (served by the AP-mode web server).
const char WIFI_CONFIG_HTML[] PROGMEM = R"WIFI(<!DOCTYPE html>
<html lang="en">

<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>CryptoTicker WiFi Setup</title>
    <style>
        :root {
            color-scheme: dark;
        }

        * {
            box-sizing: border-box;
        }

        body {
            font-family: system-ui, -apple-system, sans-serif;
            margin: 0;
            padding: 20px;
            background: #121417;
            color: #e8eaed;
            max-width: 480px;
            margin: 0 auto;
        }

        h1 {
            font-size: 1.4rem;
            text-align: center;
        }

        .card {
            background: #1c1f24;
            border-radius: 12px;
            padding: 16px;
            margin-bottom: 16px;
            border: 1px solid #2a2e35;
        }

        h2 {
            font-size: 1rem;
            margin: 0 0 12px;
            color: #9aa0a6;
            font-weight: 600;
        }

        .net {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 10px 12px;
            border: 1px solid #2a2e35;
            border-radius: 8px;
            margin-bottom: 8px;
            cursor: pointer;
            background: #23272e;
        }

        .net:hover {
            border-color: #4d90f0;
        }

        .net.selected {
            border-color: #4d90f0;
            background: #1b2838;
        }

        .ssid {
            font-weight: 500;
            word-break: break-all;
        }

        .lock {
            color: #9aa0a6;
        }

        input[type="password"],
        input[type="text"] {
            width: 100%;
            padding: 12px;
            border-radius: 8px;
            border: 1px solid #2a2e35;
            background: #23272e;
            color: #e8eaed;
            font-size: 1rem;
            margin: 4px 0 12px;
        }

        button {
            width: 100%;
            padding: 14px;
            border: none;
            border-radius: 8px;
            background: #4d90f0;
            color: #fff;
            font-size: 1rem;
            font-weight: 600;
            cursor: pointer;
        }

        button:hover {
            background: #3b7ae0;
        }

        button:disabled {
            background: #3a4150;
            cursor: not-allowed;
        }

        #status {
            text-align: center;
            margin-top: 12px;
            font-size: 0.9rem;
            min-height: 1.2em;
        }

        #refresh {
            background: none;
            border: 1px solid #2a2e35;
            color: #9aa0a6;
            padding: 8px;
            margin-top: 8px;
            font-size: 0.85rem;
        }
    </style>
</head>

<body>
    <h1>CryptoTicker WiFi Setup</h1>

    <div class="card">
        <div style="display:flex; justify-content:space-between; align-items:center;">
            <h2>Available Networks</h2>
            <button id="refresh" onclick="loadNetworks()">Refresh</button>
        </div>
        <div id="networks">
            <p>Scanning...</p>
        </div>
    </div>

    <div class="card">
        <h2>Connect</h2>
        <input type="text" id="ssid" placeholder="Network name" autocomplete="off">
        <input type="password" id="pass" placeholder="Password" autocomplete="off">
        <button id="connectBtn" onclick="submitConnect()">Connect</button>
        <div id="status"></div>
    </div>

    <script>
        let nets = [];
        async function loadNetworks() {
            const el = document.getElementById('networks');
            el.innerHTML = '<p>Scanning...</p>';
            try {
                const r = await fetch('/scan');
                nets = await r.json();
                if (!nets.length) { el.innerHTML = '<p>No networks found.</p>'; return; }
                el.innerHTML = nets.map((n, i) =>
                    `<div class="net" onclick="selectNet(${i})">
         <span class="ssid"></span>
         <span class="lock">${n.encrypt ? '\u{1F512}' : ''}</span>
       </div>`
                ).join('');
                document.querySelectorAll('.ssid').forEach((s, i) => s.textContent = nets[i].ssid);
            } catch (e) { el.innerHTML = '<p>Scan failed.</p>'; }
        }
        function selectNet(i) {
            document.querySelectorAll('.net').forEach(n => n.classList.remove('selected'));
            document.querySelectorAll('.net')[i].classList.add('selected');
            document.getElementById('ssid').value = nets[i].ssid;
        }
        async function submitConnect() {
            const ssid = document.getElementById('ssid').value.trim();
            const pass = document.getElementById('pass').value;
            if (!ssid) { document.getElementById('status').textContent = 'Enter a network name.'; return; }
            const btn = document.getElementById('connectBtn');
            const status = document.getElementById('status');
            btn.disabled = true;
            status.textContent = 'Connecting...';
            try {
                const r = await fetch('/connect', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                    body: 'ssid=' + encodeURIComponent(ssid) + '&pass=' + encodeURIComponent(pass)
                });
                const t = await r.text();
                status.textContent = t === 'OK'
                    ? 'Connected! The device will join your network shortly.'
                    : 'Failed: ' + t;
            } catch (e) {
                status.textContent = 'Request failed.';
            }
            setTimeout(() => { btn.disabled = false; }, 3000);
        }
        loadNetworks();
    </script>
</body>

</html>
)WIFI";

}  // namespace cryptoapp