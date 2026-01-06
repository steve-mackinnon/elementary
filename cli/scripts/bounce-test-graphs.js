#!/usr/bin/env node
const fs = require("fs");
const fsp = fs.promises;
const path = require("path");
const { spawnSync } = require("child_process");

function usage() {
  console.error(
    "Usage: bounce-test-graphs.js <output_dir> [elemoffline_path] [duration_seconds]"
  );
}

function resolveElemofflinePath(candidatePath) {
  if (!candidatePath) {
    return findInPath("elemoffline");
  }

  if (fs.existsSync(candidatePath)) {
    return candidatePath;
  }

  return findInPath(candidatePath);
}

function findInPath(binaryName) {
  const isWindows = process.platform === "win32";
  const suffixes = isWindows ? [".exe", ".cmd", ".bat", ""] : [""];
  const paths = (process.env.PATH || "").split(path.delimiter);

  for (const dir of paths) {
    for (const suffix of suffixes) {
      const full = path.join(dir, `${binaryName}${suffix}`);
      if (fs.existsSync(full)) {
        return full;
      }
    }
  }

  return null;
}

async function main() {
  const args = process.argv.slice(2);

  if (args[0] === "-h" || args[0] === "--help") {
    usage();
    process.exit(0);
  }

  if (args.length < 1 || args.length > 3) {
    usage();
    process.exit(1);
  }

  const repoRoot = path.resolve(__dirname, "..", "..");
  const distDir = path.resolve(repoRoot, "cli", "examples", "dist");
  const testGraphsDir = path.join(distDir, "test_graphs");
  const outputDir = path.resolve(args[0]);
  const elemofflinePath =
    resolveElemofflinePath(
      args[1] || path.join(repoRoot, "build", "cli", "elemoffline")
    ) || "";
  const durationSeconds = args[2];

  try {
    const stat = await fsp.stat(testGraphsDir);
    if (!stat.isDirectory()) {
      console.error(`test_graphs dir is not a directory: ${testGraphsDir}`);
      process.exit(1);
    }
  } catch (err) {
    console.error(`test_graphs dir not found: ${testGraphsDir}`);
    process.exit(1);
  }

  if (!elemofflinePath) {
    console.error("elemoffline not found in PATH.");
    process.exit(1);
  }

  try {
    await fsp.mkdir(outputDir, { recursive: true });
  } catch (err) {
    console.error(`Failed to create output_dir: ${outputDir}`);
    process.exit(1);
  }

  const entries = await fsp.readdir(testGraphsDir, { withFileTypes: true });
  const files = entries
    .filter((entry) => entry.isFile() && entry.name.endsWith(".js"))
    .map((entry) => path.join(testGraphsDir, entry.name))
    .sort();

  if (files.length === 0) {
    console.error(`No .js files found in: ${testGraphsDir}`);
    process.exit(1);
  }

  for (const file of files) {
    const baseName = path.basename(file, ".js");
    const outputFile = path.join(outputDir, `${baseName}.wav`);
    const cmdArgs = [file, outputFile];

    console.log(`Rendering ${file} -> ${outputFile}`);

    if (durationSeconds) {
      cmdArgs.push(durationSeconds);
    }

    const result = spawnSync(elemofflinePath, cmdArgs, { stdio: "inherit" });
    if (result.status !== 0) {
      process.exit(result.status ?? 1);
    }
  }
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
