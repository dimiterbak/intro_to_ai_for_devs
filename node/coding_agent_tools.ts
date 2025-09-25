/**
 * Tool functions and their descriptions for the coding agent.
 */

import * as fs from 'fs';
import * as path from 'path';
import { execSync } from 'child_process';

export interface CommandResult {
    stdout: string;
    stderr: string;
    returncode: number;
}

export interface SearchMatch {
    relativePath: string;
    lineNumber: number;
    lineContent: string;
}

export class CodingAgentTools {
    // Directories to skip when building file tree
    private static readonly SKIP_DIRS = new Set([
        ".venv",
        "__pycache__",
        ".git",
        ".pytest_cache",
        ".mypy_cache",
        ".coverage",
        "node_modules",
        ".DS_Store",
    ]);

    private projectDir: string;

    /**
     * Initialize AgentTools with the given project directory.
     * 
     * @param projectDir - The root directory of the project.
     */
    constructor(projectDir: string) {
        this.projectDir = path.resolve(projectDir);
    }

    /**
     * Read and return the contents of a file at the given relative filepath.
     * 
     * @param filepath - Path to the file, relative to the project directory.
     * @returns Contents of the file.
     */
    readFile(filepath: string): string {
        const absPath = path.join(this.projectDir, filepath);
        return fs.readFileSync(absPath, 'utf-8');
    }

    /**
     * Write the given content to a file at the given relative filepath, creating directories as needed.
     * 
     * @param filepath - Path to the file, relative to the project directory.
     * @param content - Content to write to the file.
     */
    writeFile(filepath: string, content: string): void {
        const absPath = path.join(this.projectDir, filepath);
        const dir = path.dirname(absPath);
        
        // Create directories if they don't exist
        fs.mkdirSync(dir, { recursive: true });
        fs.writeFileSync(absPath, content, 'utf-8');
    }

    /**
     * Return a list of all files and directories under the given root directory,
     * relative to the project directory.
     * 
     * @param rootDir - Root directory to list from, relative to the project directory. Defaults to ".".
     * @returns List of relative paths for all files and directories.
     */
    seeFileTree(rootDir: string = "."): string[] {
        const absRoot = path.join(this.projectDir, rootDir);
        const tree: string[] = [];

        const walkDir = (currentDir: string): void => {
            const items = fs.readdirSync(currentDir);
            
            for (const item of items) {
                const fullPath = path.join(currentDir, item);
                const stat = fs.statSync(fullPath);
                const relativePath = path.relative(this.projectDir, fullPath);

                // Skip blacklisted directories
                if (stat.isDirectory() && CodingAgentTools.SKIP_DIRS.has(item)) {
                    continue;
                }

                tree.push(relativePath);

                // Recursively walk subdirectories
                if (stat.isDirectory()) {
                    walkDir(fullPath);
                }
            }
        };

        walkDir(absRoot);
        return tree;
    }

    /**
     * Execute a bash command in the shell and return its output, error, and exit code.
     * Blocks running the Django development server (runserver).
     * 
     * @param command - The bash command to execute.
     * @param cwd - Working directory to run the command in, relative to the project directory.
     * @returns Object containing stdout, stderr, and return code.
     */
    executeBashCommand(command: string, cwd?: string): CommandResult {
        // Block running the Django development server
        if (command.includes("runserver")) {
            return {
                stdout: "",
                stderr: "Error: Running the Django development server (runserver) is not allowed through this tool.",
                returncode: 1
            };
        }

        const absCwd = cwd ? path.join(this.projectDir, cwd) : this.projectDir;

        try {
            const stdout = execSync(command, {
                cwd: absCwd,
                encoding: 'utf-8',
                timeout: 15000,
                stdio: ['pipe', 'pipe', 'pipe']
            }).toString();

            return {
                stdout,
                stderr: "",
                returncode: 0
            };
        } catch (error: any) {
            return {
                stdout: error.stdout ? error.stdout.toString() : "",
                stderr: error.stderr ? error.stderr.toString() : error.message,
                returncode: error.status || 1
            };
        }
    }

    /**
     * Search for a pattern in all files under the given root directory and return a list of matches
     * as (relative path, line number, line content).
     * 
     * @param pattern - Pattern to search for in files.
     * @param rootDir - Root directory to search from, relative to the project directory. Defaults to ".".
     * @returns List of matches with relative path, line number, and line content.
     */
    searchInFiles(pattern: string, rootDir: string = "."): SearchMatch[] {
        const absRoot = path.join(this.projectDir, rootDir);
        const matches: SearchMatch[] = [];

        const searchDir = (currentDir: string): void => {
            const items = fs.readdirSync(currentDir);

            for (const item of items) {
                const fullPath = path.join(currentDir, item);
                const stat = fs.statSync(fullPath);

                if (stat.isDirectory()) {
                    // Skip blacklisted directories
                    if (CodingAgentTools.SKIP_DIRS.has(item)) {
                        continue;
                    }
                    searchDir(fullPath);
                } else if (stat.isFile()) {
                    try {
                        const content = fs.readFileSync(fullPath, 'utf-8');
                        const lines = content.split('\n');
                        
                        lines.forEach((line, index) => {
                            if (line.includes(pattern)) {
                                const relativePath = path.relative(this.projectDir, fullPath);
                                matches.push({
                                    relativePath,
                                    lineNumber: index + 1,
                                    lineContent: line.trim()
                                });
                            }
                        });
                    } catch (error) {
                        // Skip files that can't be read
                        continue;
                    }
                }
            }
        };

        searchDir(absRoot);
        return matches;
    }
}