"use strict";

const http = require("http");
const fs = require("fs");
const path = require("path");
const { WebSocketServer } = require("ws");

const DIST_DIR = path.join(__dirname, "dist");
const HTTP_PORT = parseInt(process.env.HTTP_PORT || "5173", 10);
const WS_PORT = parseInt(process.env.WS_PORT || "22006", 10);

console.log("[aimsync_webradar] dist dir: " + DIST_DIR);
console.log("[aimsync_webradar] running as pkg: " + (!!process.pkg));

const MIME = {
  ".html": "text/html; charset=utf-8",
  ".js": "application/javascript",
  ".mjs": "application/javascript",
  ".css": "text/css",
  ".json": "application/json",
  ".png": "image/png",
  ".jpg": "image/jpeg",
  ".jpeg": "image/jpeg",
  ".svg": "image/svg+xml",
  ".ico": "image/x-icon",
  ".woff": "font/woff",
  ".woff2": "font/woff2",
  ".ttf": "font/ttf",
  ".webmanifest": "application/manifest+json",
};

function serveStatic(req, res) {
  let urlPath = req.url.split("?")[0];
  if (urlPath === "/") urlPath = "/index.html";

  const filePath = path.join(DIST_DIR, urlPath);

  if (!filePath.startsWith(DIST_DIR)) {
    res.writeHead(403);
    res.end("Forbidden");
    return;
  }

  fs.readFile(filePath, function (err, data) {
    if (err) {
      fs.readFile(path.join(DIST_DIR, "index.html"), function (e2, html) {
        if (e2) {
          console.error("[aimsync_webradar] index.html not found in: " + DIST_DIR);
          res.writeHead(404);
          res.end("Not found");
          return;
        }
        res.writeHead(200, { "Content-Type": "text/html; charset=utf-8" });
        res.end(html);
      });
      return;
    }
    const ext = path.extname(filePath).toLowerCase();
    const mime = MIME[ext] || "application/octet-stream";
    res.writeHead(200, { "Content-Type": mime });
    res.end(data);
  });
}

const httpServer = http.createServer(function (req, res) {
  if (req.method === "POST" && req.url === "/reload") {
    const msg = JSON.stringify({ cmd: "reload" });
    let sent = 0;
    wss.clients.forEach(function (client) {
      if (client.readyState === 1) {
        client.send(msg);
        sent++;
      }
    });
    console.log("[aimsync_webradar] /reload -> pushed to " + sent + " client(s)");
    res.writeHead(200, { "Content-Type": "text/plain" });
    res.end("ok");
    return;
  }
  if (req.method !== "GET" && req.method !== "HEAD") {
    res.writeHead(405);
    res.end();
    return;
  }
  serveStatic(req, res);
});

httpServer.listen(HTTP_PORT, "0.0.0.0", function () {
  console.log("[aimsync_webradar] radar UI -> http://localhost:" + HTTP_PORT);
});

httpServer.on("error", function (err) {
  console.error("[aimsync_webradar] http error: " + err.message);
  process.exit(1);
});

const wsHttpServer = http.createServer((req, res) => {
  if (req.method === "POST" && (req.url === "/reload" || req.url === "/shutdown")) {
    const cmd = req.url === "/shutdown" ? "shutdown" : "reload";
    const msg = JSON.stringify({ cmd });
    let sent = 0;
    wss.clients.forEach((client) => {
      if (client.readyState === 1) {
        client.send(msg);
        sent++;
      }
    });
    console.log("[aimsync_webradar] " + req.url + " (ws) -> pushed to " + sent + " client(s)");
    res.writeHead(200, { "Content-Type": "text/plain" });
    res.end("ok");
    return;
  }
  res.writeHead(404);
  res.end();
});

const wss = new WebSocketServer({ server: wsHttpServer, path: "/aimsync_webradar" });

wss.on("connection", function (ws, req) {
  const addr = (req.socket.remoteAddress || "").replace("::ffff:", "");
  console.log("[ws] " + addr + " connected  (clients: " + wss.clients.size + ")");

  ws.on("message", function (msg) {
    wss.clients.forEach(function (client) {
      if (client.readyState === 1) client.send(msg);
    });
  });

  ws.on("close", function () {
    console.log("[ws] " + addr + " disconnected");
  });
  ws.on("error", function (err) {
    console.error("[ws] error: " + err.message);
  });
});

wsHttpServer.listen(WS_PORT, "0.0.0.0", function () {
  console.log("[aimsync_webradar] websocket -> ws://localhost:" + WS_PORT + "/aimsync_webradar");
});

wsHttpServer.on("error", function (err) {
  console.error("[aimsync_webradar] ws error: " + err.message);
  process.exit(1);
});
