#!/usr/bin/env node

/**
 * Figma SVG Asset Export Script
 *
 * Automates export of SVG assets from Figma files using the Figma REST API.
 * Supports multiple asset types (icons, illustrations, mockups) with configurable
 * routing and export settings.
 *
 * Usage:
 *   npm run figma:export
 *   node figma-export.js [--file-id=FILE_ID] [--dry-run] [--list-frames]
 *
 *   --list-frames  When no assets are found, print first 50 node names from the file (for debugging naming).
 */

import fs from 'fs/promises';
import path from 'path';
import fetch from 'node-fetch';
import chalk from 'chalk';
import dotenv from 'dotenv';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

// Load environment variables
dotenv.config({ path: path.join(__dirname, '.env') });

// Configuration
const CONFIG_FILE = path.join(__dirname, 'figma-export.config.json');
const FIGMA_API_BASE = 'https://api.figma.com/v1';
const RATE_LIMIT_DELAY = 150; // ms between requests (stay under 500 req/min)
const MAX_RETRIES = 3;
const RETRY_DELAY = 1000; // ms

class FigmaExporter {
  constructor(config) {
    this.config = config;
    this.token = this.resolveToken(config.figmaToken);
    this.fileId = config.figmaFileId;
    this.exportedAssets = [];
    this.errors = [];
    this.dryRun = process.argv.includes('--dry-run');

    if (!this.token) {
      throw new Error('Figma token not found. Set FIGMA_TOKEN in .env file');
    }

    if (!this.fileId || this.fileId === 'YOUR_FIGMA_FILE_ID_HERE') {
      throw new Error('Figma file ID not configured. Update figma-export.config.json');
    }
  }

  /**
   * Resolve token from environment variable reference
   */
  resolveToken(tokenStr) {
    if (tokenStr.startsWith('${') && tokenStr.endsWith('}')) {
      const envVar = tokenStr.slice(2, -1);
      return process.env[envVar];
    }
    return tokenStr;
  }

  /**
   * Make API request with retry logic
   */
  async apiRequest(endpoint, options = {}) {
    const url = `${FIGMA_API_BASE}${endpoint}`;
    const headers = {
      'X-Figma-Token': this.token,
      ...options.headers
    };

    for (let attempt = 0; attempt < MAX_RETRIES; attempt++) {
      try {
        const response = await fetch(url, { ...options, headers });

        if (response.status === 429) {
          // Rate limited
          const retryAfter = parseInt(response.headers.get('retry-after') || '60');
          console.log(chalk.yellow(`⚠️  Rate limited. Waiting ${retryAfter}s...`));
          await this.sleep(retryAfter * 1000);
          continue;
        }

        if (!response.ok) {
          const error = await response.text();
          throw new Error(`Figma API error (${response.status}): ${error}`);
        }

        return await response.json();
      } catch (error) {
        if (attempt === MAX_RETRIES - 1) throw error;

        console.log(chalk.yellow(`⚠️  Request failed, retrying (${attempt + 1}/${MAX_RETRIES})...`));
        await this.sleep(RETRY_DELAY * Math.pow(2, attempt)); // Exponential backoff
      }
    }
  }

  /**
   * Fetch Figma file structure
   */
  async fetchFile() {
    console.log(chalk.blue('📡 Fetching Figma file structure...'));
    const data = await this.apiRequest(`/files/${this.fileId}`);
    return data.document;
  }

  /**
   * Collect node names and types for debugging (used with --list-frames)
   */
  collectNodeNames(node, depth = 0, out = [], max = 50) {
    if (out.length >= max) return out;
    if (node.name != null) {
      out.push({ depth, type: node.type || '?', name: node.name });
    }
    if (node.children) {
      for (const child of node.children) {
        this.collectNodeNames(child, depth + 1, out, max);
        if (out.length >= max) break;
      }
    }
    return out;
  }

  /**
   * Traverse document tree and find exportable nodes
   */
  traverseNodes(node, depth = 0) {
    const exportable = [];

    // Check if node matches any asset type prefix (case-insensitive). framePrefix can be a string or array.
    for (const [assetType, config] of Object.entries(this.config.assetTypes)) {
      const prefixes = [].concat(config.framePrefix ?? []).filter(Boolean);
      const nameLower = (node.name || '').toLowerCase();
      const matchedPrefix = prefixes
        .map((p) => String(p))
        .filter((p) => nameLower.startsWith(p.toLowerCase()))
        .sort((a, b) => b.length - a.length)[0];
      if (node.name && matchedPrefix !== undefined) {
        const rawName = node.name.slice(matchedPrefix.length).replace(/^\/+/, '');
        exportable.push({
          id: node.id,
          name: rawName,
          originalName: node.name,
          type: assetType,
          config: config,
          node: node
        });
      }
    }

    // Recursively traverse children
    if (node.children) {
      for (const child of node.children) {
        exportable.push(...this.traverseNodes(child, depth + 1));
      }
    }

    return exportable;
  }

  /**
   * Export assets from Figma
   */
  async exportAssets(nodes) {
    console.log(chalk.blue(`\n📦 Exporting ${nodes.length} assets...`));

    for (const asset of nodes) {
      try {
        await this.exportSingleAsset(asset);
        await this.sleep(RATE_LIMIT_DELAY);
      } catch (error) {
        this.errors.push({ asset: asset.name, error: error.message });
        console.log(chalk.red(`✗ Failed to export ${asset.name}: ${error.message}`));
      }
    }
  }

  /**
   * Export a single asset
   */
  async exportSingleAsset(asset) {
    const { id, name, type, config } = asset;

    // Build export URL (Figma API requires lowercase: svg, png, jpg, pdf)
    const format = (config.exportSettings.format || 'svg').toString().toLowerCase();
    const params = new URLSearchParams({
      ids: id,
      ...config.exportSettings,
      format: format
    });

    // Get export URL from Figma
    const exportData = await this.apiRequest(`/images/${this.fileId}?${params}`);
    const imageUrl = exportData?.images?.[id];

    if (!imageUrl) {
      const err = exportData?.err || (exportData?.images ? 'Node not exportable (e.g. COMPONENT_SET may need a specific variant)' : 'Invalid response from Figma');
      throw new Error(typeof err === 'string' ? err : JSON.stringify(err));
    }

    // Download asset
    const imageResponse = await fetch(imageUrl);
    if (!imageResponse.ok) {
      throw new Error(`Failed to download asset: ${imageResponse.statusText}`);
    }

    const buffer = Buffer.from(await imageResponse.arrayBuffer());

    // Build output path: preserve subfolders (e.g. icon/MDSP/Toggle/Pill/M → icons/MDSP/Toggle/Pill/M.svg) or flatten
    const fileExt = format === 'svg' ? 'svg' : 'png';
    let fileName;
    if (config.preserveSubfolders && name.includes('/')) {
      const segments = name.split('/').filter(Boolean);
      const normalized = segments.map((s) => this.convertNaming(s, config.namingConvention));
      fileName = normalized.join(path.sep);
    } else {
      fileName = this.convertNaming(name, config.namingConvention);
    }
    const filePath = path.join(__dirname, config.outputDir, `${fileName}.${fileExt}`);

    if (this.dryRun) {
      console.log(chalk.gray(`[DRY RUN] Would export: ${filePath}`));
    } else {
      // Ensure output directory exists
      await fs.mkdir(path.dirname(filePath), { recursive: true });

      // Write file
      await fs.writeFile(filePath, buffer);
      console.log(chalk.green(`✓ Exported: ${fileName}.${fileExt} (${type})`));
    }

    // Analyze asset metadata
    const metadata = await this.analyzeAsset(buffer, format, asset);

    this.exportedAssets.push({
      nodeId: id,
      name: fileName,
      originalName: asset.originalName,
      type: type,
      path: filePath,
      format: format,
      size: buffer.length,
      metadata: metadata
    });
  }

  /**
   * Analyze asset metadata
   */
  async analyzeAsset(buffer, format, asset) {
    if (format !== 'svg') return {};

    const svgContent = buffer.toString('utf-8');

    // Extract colors
    const colorRegex = /#[0-9a-fA-F]{6}|#[0-9a-fA-F]{3}/g;
    const colors = [...new Set(svgContent.match(colorRegex) || [])];

    // Check for gradients/patterns
    const hasGradients = svgContent.includes('linearGradient') || svgContent.includes('radialGradient');
    const hasPatterns = svgContent.includes('<pattern');

    // Count paths
    const pathCount = (svgContent.match(/<path/g) || []).length;

    // Determine tintability
    const tintable = colors.length <= 1 && !hasGradients && !hasPatterns;

    return {
      colors: colors,
      colorCount: colors.length,
      tintable: tintable,
      hasGradients: hasGradients,
      hasPatterns: hasPatterns,
      pathCount: pathCount,
      fileSizeKB: Math.round(buffer.length / 1024 * 10) / 10
    };
  }

  /**
   * Convert naming convention
   */
  convertNaming(name, convention) {
    switch (convention) {
      case 'snake_case':
        return name.toLowerCase().replace(/[^a-z0-9]+/g, '_').replace(/^_|_$/g, '');
      case 'kebab-case':
        return name.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '');
      case 'camelCase':
        return name.replace(/[^a-zA-Z0-9]+(.)/g, (_, c) => c.toUpperCase());
      default:
        return name;
    }
  }

  /**
   * Generate export manifest
   */
  async generateManifest() {
    const manifest = {
      exportDate: new Date().toISOString(),
      figmaFileId: this.fileId,
      totalAssets: this.exportedAssets.length,
      assetsByType: {},
      assets: this.exportedAssets,
      errors: this.errors
    };

    // Group by type
    for (const asset of this.exportedAssets) {
      if (!manifest.assetsByType[asset.type]) {
        manifest.assetsByType[asset.type] = 0;
      }
      manifest.assetsByType[asset.type]++;
    }

    const manifestPath = path.join(__dirname, this.config.output.manifestFile);

    if (this.dryRun) {
      console.log(chalk.gray(`\n[DRY RUN] Would write manifest to: ${manifestPath}`));
    } else {
      await fs.writeFile(manifestPath, JSON.stringify(manifest, null, 2));
      console.log(chalk.blue(`\n📄 Manifest written to: ${this.config.output.manifestFile}`));
    }

    return manifest;
  }

  /**
   * Print summary
   */
  printSummary(manifest) {
    console.log(chalk.bold('\n📊 Export Summary\n'));
    console.log(`Total assets exported: ${chalk.green(manifest.totalAssets)}`);

    for (const [type, count] of Object.entries(manifest.assetsByType)) {
      console.log(`  ${type}: ${chalk.cyan(count)}`);
    }

    if (this.errors.length > 0) {
      console.log(chalk.red(`\n❌ Errors: ${this.errors.length}`));
      for (const error of this.errors) {
        console.log(chalk.red(`  - ${error.asset}: ${error.error}`));
      }
    }

    // Warnings
    const warnings = this.exportedAssets.filter(a =>
      a.metadata.colorCount > 1 ||
      a.metadata.hasGradients ||
      a.metadata.fileSizeKB > this.config.validation.maxFileSizeKB ||
      a.metadata.pathCount > this.config.validation.maxPathCount
    );

    if (warnings.length > 0) {
      console.log(chalk.yellow(`\n⚠️  Warnings: ${warnings.length}`));
      for (const asset of warnings) {
        const issues = [];
        if (asset.metadata.colorCount > 1) {
          issues.push(`${asset.metadata.colorCount} colors (may not tint)`);
        }
        if (asset.metadata.hasGradients) {
          issues.push('contains gradients');
        }
        if (asset.metadata.fileSizeKB > this.config.validation.maxFileSizeKB) {
          issues.push(`${asset.metadata.fileSizeKB}KB (exceeds ${this.config.validation.maxFileSizeKB}KB)`);
        }
        if (asset.metadata.pathCount > this.config.validation.maxPathCount) {
          issues.push(`${asset.metadata.pathCount} paths (exceeds ${this.config.validation.maxPathCount})`);
        }
        console.log(chalk.yellow(`  - ${asset.name}: ${issues.join(', ')}`));
      }
      console.log(chalk.yellow('\n  Run "npm run svg:process" to optimize and fix issues'));
    }

    console.log(''); // Empty line
  }

  /**
   * Sleep utility
   */
  sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
  }

  /**
   * Main export workflow
   */
  async run() {
    console.log(chalk.bold.blue('\n🎨 Figma Asset Exporter\n'));

    if (this.dryRun) {
      console.log(chalk.yellow('🔍 DRY RUN MODE - No files will be written\n'));
    }

    try {
      // Fetch file structure
      const document = await this.fetchFile();

      // Find exportable nodes
      const exportableNodes = this.traverseNodes(document);
      console.log(chalk.blue(`\n✓ Found ${exportableNodes.length} exportable assets`));

      if (exportableNodes.length === 0) {
        console.log(chalk.yellow('\n⚠️  No assets found. Check your naming conventions in Figma:'));
        for (const [type, config] of Object.entries(this.config.assetTypes)) {
          const prefixDesc = Array.isArray(config.framePrefix) ? config.framePrefix.join('" or "') : config.framePrefix;
          console.log(chalk.gray(`  - ${type}: frames must start with "${prefixDesc}" (match is case-insensitive)`));
        }
        console.log(chalk.gray('\n  Tip: Frame names must match from the start (e.g. icon/MyIcon or Icon/MyIcon). Nested frames only export if their own name has the prefix.'));
        if (process.argv.includes('--list-frames')) {
          const names = this.collectNodeNames(document);
          console.log(chalk.blue('\n  First nodes in file (--list-frames):'));
          names.forEach(({ depth, type, name }) => console.log(chalk.gray(`    ${'  '.repeat(depth)}[${type}] ${name}`)));
        }
        return;
      }

      // Export assets
      await this.exportAssets(exportableNodes);

      // Generate manifest
      const manifest = await this.generateManifest();

      // Print summary
      this.printSummary(manifest);

      if (this.errors.length === 0 && !this.dryRun) {
        console.log(chalk.green('✨ Export completed successfully!\n'));
        console.log(chalk.gray('Next steps:'));
        console.log(chalk.gray('  1. Run: npm run svg:process'));
        console.log(chalk.gray('  2. Build project to generate IconIds\n'));
      }

    } catch (error) {
      console.error(chalk.red(`\n❌ Export failed: ${error.message}`));
      if (this.config.output.verbose) {
        console.error(error.stack);
      }
      process.exit(1);
    }
  }
}

// Main execution
(async () => {
  try {
    const config = JSON.parse(await fs.readFile(CONFIG_FILE, 'utf-8'));

    // Allow CLI override of file ID
    const fileIdArg = process.argv.find(arg => arg.startsWith('--file-id='));
    if (fileIdArg) {
      config.figmaFileId = fileIdArg.split('=')[1];
    }

    const exporter = new FigmaExporter(config);
    await exporter.run();
  } catch (error) {
    console.error(chalk.red(`\n❌ Fatal error: ${error.message}`));
    process.exit(1);
  }
})();
