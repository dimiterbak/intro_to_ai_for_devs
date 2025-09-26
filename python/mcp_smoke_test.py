"""
Minimal smoke test for the Sequential Thinking MCP server.

Prereqs:
- Python: pip install "mcp[cli]" (or `uv pip install "mcp[cli]"`)
- Node.js with npx on PATH

Run:
  python mcp_smoke_test.py
"""

import os
import asyncio
from typing import Any

try:
    from mcp import ClientSession, StdioServerParameters, types as mcp_types
    from mcp.client.stdio import stdio_client
except Exception as e:
    raise SystemExit(
        "The MCP Python SDK is not installed. Install with: pip install \"mcp[cli]\"\n"
        f"Original error: {e}"
    )


async def main() -> None:
    params = StdioServerParameters(
        command="npx",
        args=["-y", "@modelcontextprotocol/server-sequential-thinking"],
        env={
            "DISABLE_THOUGHT_LOGGING": os.environ.get("DISABLE_THOUGHT_LOGGING", "false"),
        },
    )

    async with stdio_client(params) as (read, write):
        async with ClientSession(read, write) as session:
            await session.initialize()

            # Prepare a simple thinking step
            args: dict[str, Any] = {
                "thought": "Plan a 3-step approach to write a unit test for a Python function that reverses a string.",
                "nextThoughtNeeded": True,
                "thoughtNumber": 1,
                "totalThoughts": 3,
            }

            # Ensure tool exists (optional)
            tools = await session.list_tools()
            names = [t.name for t in tools.tools]
            print("Available tools:", names)

            def _norm(s: str) -> str:
                return s.replace("_", "").replace("-", "").lower()

            desired = _norm("sequentialthinking")
            tool_name = next((n for n in names if _norm(n) == desired), None)
            if not tool_name:
                raise SystemExit("sequentialthinking tool not found on server")

            result = await session.call_tool(tool_name, arguments=args)

            # Prefer structured output
            if getattr(result, "structuredContent", None):
                print("Structured result:", result.structuredContent)
            else:
                texts = []
                for c in result.content:
                    if isinstance(c, mcp_types.TextContent):
                        texts.append(c.text)
                print("\n".join(texts) if texts else "(no content)")


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except FileNotFoundError:
        raise SystemExit(
            "Failed to run npx. Ensure Node.js and npx are installed and available in PATH."
        )
    except Exception as e:
        raise SystemExit(f"Smoke test failed: {e}")
