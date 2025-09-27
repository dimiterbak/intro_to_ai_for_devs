#!/usr/bin/env node
// Minimal MCP client bridge to call the sequentialthinking server tool.
// Usage: node node/tools/mcp_seqthink_bridge.mjs '{"thought":"...","nextThoughtNeeded":true,"thoughtNumber":1,"totalThoughts":3}'
// Or pass base64-encoded JSON via env MCP_SEQ_ARGS_B64.

import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StdioClientTransport } from "@modelcontextprotocol/sdk/client/stdio.js";

function normalize(name) {
  return String(name).replaceAll("_", "").replaceAll("-", "").toLowerCase();
}

function parseArgs() {
  try {
    const b64 = process.env.MCP_SEQ_ARGS_B64;
    if (b64 && b64.length > 0) {
      const json = Buffer.from(b64, "base64").toString("utf8");
      return JSON.parse(json);
    }
    const arg = process.argv[2];
    if (arg && arg.trim().length > 0) {
      return JSON.parse(arg);
    }
    return {};
  } catch (err) {
    throw new Error(`Failed to parse arguments: ${err?.message || String(err)}`);
  }
}

async function main() {
  const args = parseArgs();

  const transport = new StdioClientTransport({
    command: "npx",
    args: ["-y", "@modelcontextprotocol/server-sequential-thinking"],
    env: {
      DISABLE_THOUGHT_LOGGING: process.env.DISABLE_THOUGHT_LOGGING || "false",
    },
  });

  const client = new Client({ name: "seqthink-bridge", version: "1.0.0" });
  try {
    await client.connect(transport);

    // Determine tool name by normalization
    let chosen = "sequentialthinking";
    const desired = normalize("sequentialthinking");
    const candidates = ["sequentialthinking", "sequential-thinking", "sequential_thinking"];
    try {
      const listed = await client.listTools();
      const available = listed.tools?.map(t => t.name) ?? [];
      const match = available.find(n => normalize(n) === desired);
      if (match) {
        chosen = match;
      } else {
        for (const n of available) if (!candidates.includes(n)) candidates.push(n);
      }
    } catch {
      // Continue with defaults if listTools fails
    }

    let lastErr = null;
    let result = null;
    for (const name of [chosen, ...candidates.filter(c => c !== chosen)]) {
      try {
        result = await client.callTool({ name, arguments: args });
        const textParts = [];
        const content = result?.content ?? [];
        for (const c of content) {
          if (c?.type === "text" && typeof c.text === "string") textParts.push(c.text);
        }
        const response = {
          ok: true,
          toolName: name,
          content,
          text: textParts.length ? textParts.join("\n") : undefined,
          isError: !!result?.isError,
        };
        process.stdout.write(JSON.stringify(response));
        return;
      } catch (e) {
        lastErr = e;
        // Try next candidate
      }
    }

    throw lastErr || new Error("Failed to call sequentialthinking tool");
  } catch (err) {
    const message = err?.message || String(err);
    // Print machine-readable error JSON to stdout; any human logs to stderr.
    process.stderr.write(`Error: ${message}\n`);
    process.stdout.write(JSON.stringify({ ok: false, error: message }));
    process.exitCode = 1;
  } finally {
    try { await client.close?.(); } catch {}
  }
}

main();
