# Figma Asset Export Scripts

Automated SVG asset export and processing pipeline for AnalyzerPro.

## Quick Start

```bash
# 1. Install dependencies
npm install

# 2. Set up environment
cp .env.example .env
# Edit .env and add your FIGMA_TOKEN

# 3. Configure Figma file
# Edit figma-export.config.json and set your figmaFileId

# 4. Export and process assets
npm run assets:sync

# 5. Build project
cd ..
./scripts/build.sh
```

## Available Scripts

| Command | Description |
|---------|-------------|
| `npm run figma:export` | Export SVG assets from Figma using REST API |
| `npm run svg:process` | Optimize and process exported SVGs |
| `npm run svg:validate` | Validate SVGs without modifying files |
| `npm run assets:sync` | Run export + process in sequence |

## Files

### Configuration

- **`.env`** - Environment variables (Figma token)
  - Copy from `.env.example` and fill in
  - Add `FIGMA_TOKEN=your_token_here`
  - **Never commit this file!**

- **`figma-export.config.json`** - Asset routing and export settings
  - Set `figmaFileId` from Figma URL
  - Configure asset types (icons, illustrations, mockups)
  - Define export settings and validation rules

### Scripts

- **`figma-export.js`** - Figma API client
  - Connects to Figma REST API
  - Traverses document tree
  - Exports assets by naming convention
  - Generates export manifest

- **`process-svgs.js`** - SVG post-processor
  - Converts colors to `currentColor` for tinting
  - Optimizes with SVGO (file size reduction)
  - Validates JUCE compatibility
  - Reports errors and warnings

## Complete Documentation

See [docs/FIGMA_SVG_WORKFLOW.md](../docs/FIGMA_SVG_WORKFLOW.md) for the full workflow guide.

---

This is part of the AnalyzerPro build system. For build/release scripts, see the main [README.md](./README.md).
