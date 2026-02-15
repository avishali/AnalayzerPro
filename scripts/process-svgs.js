#!/usr/bin/env node

/**
 * SVG Processing Pipeline
 *
 * Post-processes exported SVG files with:
 * - Color conversion to currentColor for tinting support
 * - SVGO optimization (file size reduction, cleanup)
 * - JUCE compatibility validation
 * - Error reporting and validation summary
 *
 * Usage:
 *   npm run svg:process
 *   npm run svg:validate (validation only, no modifications)
 *   node process-svgs.js [--validate-only] [--verbose]
 */

import fs from 'fs/promises';
import path from 'path';
import chalk from 'chalk';
import { load as loadCheerio } from 'cheerio';
import { optimize as optimizeSvgo } from 'svgo';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const CONFIG_FILE = path.join(__dirname, 'figma-export.config.json');
const MANIFEST_FILE = path.join(__dirname, 'export-manifest.json');

class SvgProcessor {
  constructor(config, options = {}) {
    this.config = config;
    this.validateOnly = options.validateOnly || false;
    this.verbose = options.verbose || config.output.verbose || false;
    this.processedFiles = [];
    this.errors = [];
    this.warnings = [];
  }

  /**
   * Load export manifest
   */
  async loadManifest() {
    try {
      const content = await fs.readFile(MANIFEST_FILE, 'utf-8');
      return JSON.parse(content);
    } catch (error) {
      console.log(chalk.yellow('⚠️  No export manifest found. Processing all SVG files...'));
      return null;
    }
  }

  /**
   * Find all SVG files in configured directories
   */
  async findSvgFiles() {
    const svgFiles = [];

    for (const [type, assetConfig] of Object.entries(this.config.assetTypes)) {
      if (assetConfig.exportSettings.format !== 'SVG') continue;

      const dir = path.join(__dirname, assetConfig.outputDir);

      try {
        const files = await fs.readdir(dir);
        for (const file of files) {
          if (file.endsWith('.svg')) {
            svgFiles.push({
              path: path.join(dir, file),
              name: file.replace('.svg', ''),
              type: type,
              config: assetConfig
            });
          }
        }
      } catch (error) {
        if (error.code !== 'ENOENT') {
          console.log(chalk.yellow(`⚠️  Could not read directory: ${dir}`));
        }
      }
    }

    return svgFiles;
  }

  /**
   * Process a single SVG file
   */
  async processSvg(file) {
    const content = await fs.readFile(file.path, 'utf-8');
    let processed = content;

    // Step 1: Color conversion for icons
    if (file.type === 'icons' && !file.config.preserveColors) {
      processed = this.convertColorsToCurrentColor(processed, file);
    }

    // Step 2: SVGO optimization
    processed = this.optimizeSvg(processed, file);

    // Step 3: Validation
    const validation = this.validateSvg(processed, file);

    if (!this.validateOnly && processed !== content) {
      await fs.writeFile(file.path, processed);
      this.processedFiles.push({
        ...file,
        originalSize: content.length,
        processedSize: processed.length,
        reduction: Math.round((1 - processed.length / content.length) * 100),
        validation: validation
      });
    }

    return { file, content: processed, validation };
  }

  /**
   * Convert colors to currentColor for tinting support
   */
  convertColorsToCurrentColor(svgContent, file) {
    const $ = loadCheerio(svgContent, { xmlMode: true });

    // Find all fill and stroke colors
    const colors = new Set();
    $('[fill]').each((_, el) => {
      const fill = $(el).attr('fill');
      if (fill && fill.startsWith('#')) {
        colors.add(fill.toLowerCase());
      }
    });
    $('[stroke]').each((_, el) => {
      const stroke = $(el).attr('stroke');
      if (stroke && stroke.startsWith('#')) {
        colors.add(stroke.toLowerCase());
      }
    });

    // Only convert if single color (excluding none/transparent)
    const solidColors = Array.from(colors).filter(c => c !== '#000000' && c !== '#ffffff');

    if (solidColors.length === 0 || solidColors.length > 1) {
      if (solidColors.length > 1 && this.verbose) {
        this.warnings.push({
          file: file.name,
          message: `Multiple colors found (${solidColors.length}), skipping currentColor conversion`,
          severity: 'info'
        });
      }
      return svgContent;
    }

    // Convert all fills and strokes to currentColor
    $('[fill]').each((_, el) => {
      const fill = $(el).attr('fill');
      if (fill && fill.startsWith('#') && fill !== 'none') {
        $(el).attr('fill', 'currentColor');
      }
    });

    $('[stroke]').each((_, el) => {
      const stroke = $(el).attr('stroke');
      if (stroke && stroke.startsWith('#') && stroke !== 'none') {
        $(el).attr('stroke', 'currentColor');
      }
    });

    if (this.verbose) {
      console.log(chalk.gray(`  Converted ${solidColors[0]} → currentColor (${file.name})`));
    }

    return $.xml();
  }

  /**
   * Optimize SVG with SVGO
   */
  optimizeSvg(svgContent, file) {
    try {
      const result = optimizeSvgo(svgContent, {
        plugins: [
          {
            name: 'preset-default',
            params: {
              overrides: {
                // Don't remove viewBox (needed for scaling)
                removeViewBox: false,
                // Don't remove dimensions (JUCE compatibility)
                removeDimensions: false,
                // Don't convert colors if we want to preserve them
                convertColors: !file.config.preserveColors
              }
            }
          },
          'removeDoctype',
          'removeXMLProcInst',
          'removeComments',
          'removeMetadata',
          'removeEditorsNSData',
          'cleanupAttrs',
          'mergeStyles',
          'inlineStyles',
          'minifyStyles',
          'cleanupIds',
          'removeUselessDefs',
          'cleanupNumericValues',
          'removeUnknownsAndDefaults'
        ]
      });

      return result.data;
    } catch (error) {
      this.errors.push({
        file: file.name,
        message: `SVGO optimization failed: ${error.message}`,
        severity: 'error'
      });
      return svgContent;
    }
  }

  /**
   * Validate SVG for JUCE compatibility
   */
  validateSvg(svgContent, file) {
    const validation = {
      valid: true,
      errors: [],
      warnings: []
    };

    try {
      const $ = loadCheerio(svgContent, { xmlMode: true });
      const svg = $('svg');

      // Check for viewBox
      if (!svg.attr('viewBox')) {
        validation.warnings.push('Missing viewBox attribute (may affect scaling)');
      }

      // Check file size
      const sizeKB = svgContent.length / 1024;
      if (sizeKB > this.config.validation.maxFileSizeKB) {
        validation.warnings.push(
          `File size ${Math.round(sizeKB)}KB exceeds ${this.config.validation.maxFileSizeKB}KB limit`
        );
      }

      // Count paths and check complexity
      const pathCount = $('path').length;
      if (pathCount > this.config.validation.maxPathCount) {
        validation.warnings.push(
          `Path count ${pathCount} exceeds ${this.config.validation.maxPathCount} limit (may impact performance)`
        );
      }

      // Check for unsupported elements
      const allElements = new Set();
      $('*').each((_, el) => {
        allElements.add(el.tagName);
      });

      const unsupported = [...allElements].filter(
        tag => !this.config.validation.allowedSvgElements.includes(tag)
      );

      if (unsupported.length > 0) {
        validation.warnings.push(
          `Contains potentially unsupported elements: ${unsupported.join(', ')}`
        );
      }

      // Check for animations
      if (svgContent.includes('<animate') || svgContent.includes('<animateTransform')) {
        validation.errors.push('Contains SVG animations (not supported by JUCE)');
        validation.valid = false;
      }

      // Check for external references
      if (svgContent.includes('xlink:href="http') || svgContent.includes('href="http')) {
        validation.errors.push('Contains external references (not supported)');
        validation.valid = false;
      }

      // Check for filters
      if (svgContent.includes('<filter')) {
        validation.warnings.push('Contains filters (may not render correctly in JUCE)');
      }

    } catch (error) {
      validation.valid = false;
      validation.errors.push(`XML parsing failed: ${error.message}`);
    }

    // Store validation issues
    if (validation.errors.length > 0) {
      for (const error of validation.errors) {
        this.errors.push({
          file: file.name,
          message: error,
          severity: 'error'
        });
      }
    }

    if (validation.warnings.length > 0) {
      for (const warning of validation.warnings) {
        this.warnings.push({
          file: file.name,
          message: warning,
          severity: 'warning'
        });
      }
    }

    return validation;
  }

  /**
   * Print processing summary
   */
  printSummary() {
    console.log(chalk.bold('\n📊 Processing Summary\n'));

    if (!this.validateOnly) {
      console.log(`Files processed: ${chalk.green(this.processedFiles.length)}`);

      if (this.processedFiles.length > 0) {
        const avgReduction = Math.round(
          this.processedFiles.reduce((sum, f) => sum + f.reduction, 0) / this.processedFiles.length
        );
        console.log(`Average size reduction: ${chalk.cyan(avgReduction + '%')}`);
      }
    }

    if (this.errors.length > 0) {
      console.log(chalk.red(`\n❌ Errors: ${this.errors.length}`));
      const byFile = {};
      for (const error of this.errors) {
        if (!byFile[error.file]) byFile[error.file] = [];
        byFile[error.file].push(error.message);
      }
      for (const [file, messages] of Object.entries(byFile)) {
        console.log(chalk.red(`  ${file}:`));
        for (const msg of messages) {
          console.log(chalk.red(`    - ${msg}`));
        }
      }
    }

    if (this.warnings.length > 0) {
      console.log(chalk.yellow(`\n⚠️  Warnings: ${this.warnings.length}`));
      const byFile = {};
      for (const warning of this.warnings) {
        if (!byFile[warning.file]) byFile[warning.file] = [];
        byFile[warning.file].push(warning.message);
      }
      for (const [file, messages] of Object.entries(byFile)) {
        console.log(chalk.yellow(`  ${file}:`));
        for (const msg of messages) {
          console.log(chalk.yellow(`    - ${msg}`));
        }
      }
    }

    if (this.errors.length === 0 && this.warnings.length === 0) {
      console.log(chalk.green('✓ All SVG files passed validation!'));
    }

    console.log(''); // Empty line
  }

  /**
   * Main processing workflow
   */
  async run() {
    console.log(chalk.bold.blue('\n🔧 SVG Processing Pipeline\n'));

    if (this.validateOnly) {
      console.log(chalk.yellow('🔍 VALIDATE ONLY MODE - No files will be modified\n'));
    }

    try {
      // Find SVG files
      const svgFiles = await this.findSvgFiles();

      if (svgFiles.length === 0) {
        console.log(chalk.yellow('⚠️  No SVG files found to process'));
        console.log(chalk.gray('Run "npm run figma:export" first to export assets from Figma\n'));
        return;
      }

      console.log(chalk.blue(`Found ${svgFiles.length} SVG files\n`));

      // Process each file
      for (const file of svgFiles) {
        try {
          await this.processSvg(file);

          if (!this.validateOnly) {
            console.log(chalk.green(`✓ Processed: ${file.name}`));
          } else {
            console.log(chalk.gray(`✓ Validated: ${file.name}`));
          }
        } catch (error) {
          this.errors.push({
            file: file.name,
            message: `Processing failed: ${error.message}`,
            severity: 'error'
          });
          console.log(chalk.red(`✗ Failed: ${file.name}`));
        }
      }

      // Print summary
      this.printSummary();

      if (this.errors.length === 0 && !this.validateOnly) {
        console.log(chalk.green('✨ Processing completed successfully!\n'));
        console.log(chalk.gray('Next step: Build project to generate IconIds and embed assets\n'));
      }

      // Exit with error code if validation failed
      if (this.errors.length > 0) {
        process.exit(1);
      }

    } catch (error) {
      console.error(chalk.red(`\n❌ Processing failed: ${error.message}`));
      if (this.verbose) {
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

    const options = {
      validateOnly: process.argv.includes('--validate-only'),
      verbose: process.argv.includes('--verbose') || config.output.verbose
    };

    const processor = new SvgProcessor(config, options);
    await processor.run();
  } catch (error) {
    console.error(chalk.red(`\n❌ Fatal error: ${error.message}`));
    process.exit(1);
  }
})();
