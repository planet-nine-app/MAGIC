/**
 * LoRa WebSocket Server
 * Magical real-time dashboard for LoRa messages with sparkles!
 */

const express = require('express');
const http = require('http');
const WebSocket = require('ws');
const path = require('path');

const app = express();
const server = http.createServer(app);

// Single WebSocket server with manual routing
const wss = new WebSocket.Server({
  noServer: true,
  perMessageDeflate: false
});

// Track clients by type
const loraClients = new Set();
const dashboardClients = new Set();

// Store recent messages (last 100)
const recentMessages = [];
const MAX_MESSAGES = 100;

// Statistics
const stats = {
  totalMessages: 0,
  validSignatures: 0,
  invalidSignatures: 0,
  connectedGateways: 0,
  connectedDashboards: 0,
  startTime: Date.now()
};

// Parse JSON bodies
app.use(express.json());

// Serve static files
app.use(express.static('public'));

// Main dashboard route
app.get('/', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

// POST endpoint for BLE messages
app.post('/lora', (req, res) => {
  try {
    const message = req.body;

    if (message.type === 'lora_message') {
      console.log('\n✨ [MAGIC] LoRa Message Received via HTTP! ✨');
      console.log(`Message: "${message.message}"`);
      console.log(`Sender: ${message.senderPubKey.substring(0, 16)}...`);
      console.log(`Signature: ${message.signatureValid ? '✓ VALID' : '✗ INVALID'}`);

      // Update statistics
      stats.totalMessages++;
      if (message.signatureValid) {
        stats.validSignatures++;
      } else {
        stats.invalidSignatures++;
      }

      // Add timestamp and store
      message.receivedAt = Date.now();
      recentMessages.unshift(message);
      if (recentMessages.length > MAX_MESSAGES) {
        recentMessages.pop();
      }

      // Broadcast to all dashboards with sparkle effect
      broadcastToDashboards({
        type: 'new_message',
        message: message,
        stats: stats
      });

      res.json({ success: true, message: 'Message received!' });
    } else {
      res.status(400).json({ success: false, error: 'Invalid message type' });
    }
  } catch (error) {
    console.error('[HTTP] Error processing message:', error.message);
    res.status(500).json({ success: false, error: error.message });
  }
});

/**
 * Broadcast message to all dashboard clients
 */
function broadcastToDashboards(data) {
  const message = JSON.stringify(data);
  console.log(`[DEBUG] Broadcasting to ${dashboardClients.size} dashboard clients`);
  console.log(`[DEBUG] Message type: ${data.type}`);

  let sentCount = 0;
  dashboardClients.forEach(client => {
    if (client.readyState === WebSocket.OPEN) {
      client.send(message);
      sentCount++;
      console.log(`[DEBUG] Sent message to dashboard client`);
    } else {
      console.log(`[DEBUG] Client not ready, state: ${client.readyState}`);
    }
  });

  console.log(`[DEBUG] Successfully sent to ${sentCount} clients`);
}

/**
 * Handle WebSocket upgrade requests
 */
server.on('upgrade', (request, socket, head) => {
  const pathname = request.url;
  console.log(`[WebSocket] Upgrade request for: ${pathname}`);

  if (pathname === '/lora') {
    wss.handleUpgrade(request, socket, head, (ws) => {
      handleLoraConnection(ws);
    });
  } else if (pathname === '/dashboard') {
    wss.handleUpgrade(request, socket, head, (ws) => {
      handleDashboardConnection(ws);
    });
  } else {
    socket.destroy();
  }
});

/**
 * Handle LoRa gateway connections
 */
function handleLoraConnection(ws) {
  loraClients.add(ws);
  stats.connectedGateways++;
  console.log(`[LoRa] Gateway connected (${stats.connectedGateways} total)`);

  // Notify dashboards
  broadcastToDashboards({
    type: 'gateway_status',
    connected: true,
    totalGateways: stats.connectedGateways
  });

  ws.on('message', (data) => {
    try {
      const message = JSON.parse(data.toString());

      if (message.type === 'lora_message') {
        console.log('\n✨ [MAGIC] LoRa Message Received! ✨');
        console.log(`Message: "${message.message}"`);
        console.log(`Sender: ${message.senderPubKey.substring(0, 16)}...`);
        console.log(`Signature: ${message.signatureValid ? '✓ VALID' : '✗ INVALID'}`);
        console.log(`Signal: RSSI ${message.rssi} dBm, SNR ${message.snr} dB`);

        // Update statistics
        stats.totalMessages++;
        if (message.signatureValid) {
          stats.validSignatures++;
        } else {
          stats.invalidSignatures++;
        }

        // Add timestamp and store
        message.receivedAt = Date.now();
        recentMessages.unshift(message);
        if (recentMessages.length > MAX_MESSAGES) {
          recentMessages.pop();
        }

        // Broadcast to all dashboards with sparkle effect
        broadcastToDashboards({
          type: 'new_message',
          message: message,
          stats: stats
        });
      } else if (message.type === 'gateway_connected') {
        console.log(`[LoRa] Gateway identified: ${message.device || 'unknown'}`);
      }
    } catch (error) {
      console.error('[LoRa] Error parsing message:', error.message);
    }
  });

  ws.on('close', () => {
    loraClients.delete(ws);
    stats.connectedGateways--;
    console.log(`[LoRa] Gateway disconnected (${stats.connectedGateways} remaining)`);

    // Notify dashboards
    broadcastToDashboards({
      type: 'gateway_status',
      connected: stats.connectedGateways > 0,
      totalGateways: stats.connectedGateways
    });
  });

  ws.on('error', (error) => {
    console.error('[LoRa] Gateway error:', error.message);
  });
}

/**
 * Handle dashboard client connections
 */
function handleDashboardConnection(ws) {
  dashboardClients.add(ws);
  stats.connectedDashboards++;
  console.log(`[Dashboard] Client connected (${stats.connectedDashboards} total)`);

  // Send initial state
  ws.send(JSON.stringify({
    type: 'init',
    messages: recentMessages,
    stats: stats,
    gatewayConnected: stats.connectedGateways > 0
  }));

  ws.on('close', () => {
    dashboardClients.delete(ws);
    stats.connectedDashboards--;
    console.log(`[Dashboard] Client disconnected (${stats.connectedDashboards} remaining)`);
  });

  ws.on('error', (error) => {
    console.error('[Dashboard] Client error:', error.message);
  });
}

// Start server
const PORT = process.env.PORT || 8080;
server.listen(PORT, () => {
  console.log('\n' + '='.repeat(50));
  console.log('✨ Magical LoRa Dashboard Server ✨');
  console.log('='.repeat(50));
  console.log(`Dashboard: http://localhost:${PORT}`);
  console.log(`LoRa Gateway WebSocket: ws://localhost:${PORT}/lora`);
  console.log(`Dashboard WebSocket: ws://localhost:${PORT}/dashboard`);
  console.log('='.repeat(50) + '\n');
  console.log('Waiting for LoRa messages...\n');
});

// Graceful shutdown
process.on('SIGINT', () => {
  console.log('\n\nShutting down gracefully...');
  server.close(() => {
    console.log('Server closed');
    process.exit(0);
  });
});
