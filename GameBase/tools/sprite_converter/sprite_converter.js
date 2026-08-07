/* GameBase browser sprite converter — mirrors tools/sprite_converter.py */

let generatedHeader = "";

function rgbToRgb565(r, g, b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

function rgb565ToRgb(value) {
  const r = (((value >> 11) & 0x1f) * 255) / 31;
  const g = (((value >> 5) & 0x3f) * 255) / 63;
  const b = ((value & 0x1f) * 255) / 31;
  return [Math.round(r), Math.round(g), Math.round(b)];
}

function maxPaletteSize(bpp) {
  if (bpp === 4) return 16;
  if (bpp === 8) return 256;
  return 0;
}

function isOpaquePixel(r, g, b, a, transparentRgb) {
  if (a < 20) return false;
  if (transparentRgb) {
    const [tr, tg, tb] = transparentRgb;
    if (Math.abs(r - tr) <= 12 && Math.abs(g - tg) <= 12 && Math.abs(b - tb) <= 12) {
      return false;
    }
  }
  return true;
}

function nearestPaletteIndex(rgb565, palette) {
  const [tr, tg, tb] = rgb565ToRgb(rgb565);
  let bestIdx = 0;
  let bestDist = Infinity;
  palette.forEach((color, idx) => {
    const [sr, sg, sb] = rgb565ToRgb(color);
    const dist = (tr - sr) ** 2 + (tg - sg) ** 2 + (tb - sb) ** 2;
    if (dist < bestDist) {
      bestDist = dist;
      bestIdx = idx;
    }
  });
  return bestIdx;
}

function buildPaletteFromColors(colors, bpp) {
  const unique = [...new Set(colors)];
  const limit = maxPaletteSize(bpp);
  if (unique.length > limit) {
    throw new Error(
      `Image uses ${unique.length} unique opaque colors but ${bpp}-bit allows at most ${limit}.`
    );
  }
  return unique;
}

function encodeMaskRows(width, height, opaqueGrid) {
  const maskRows = [];
  for (let y = 0; y < height; y++) {
    const rowBits = [];
    for (let x = 0; x < width; x++) {
      rowBits.push(opaqueGrid[y][x] ? "1" : "0");
    }
    for (let i = 0; i < width; i += 8) {
      let chunk = rowBits.slice(i, i + 8);
      while (chunk.length < 8) chunk.push("0");
      maskRows.push(parseInt(chunk.join(""), 2));
    }
  }
  return maskRows;
}

function encodePixels16(colors, opaqueGrid) {
  const height = colors.length;
  const width = colors[0]?.length || 0;
  const pixels = [];
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      pixels.push(opaqueGrid[y][x] ? colors[y][x] : 0xffff);
    }
  }
  return pixels;
}

function encodePixels8(colors, opaqueGrid, palette) {
  const height = colors.length;
  const width = colors[0]?.length || 0;
  const pixels = [];
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      pixels.push(opaqueGrid[y][x] ? nearestPaletteIndex(colors[y][x], palette) : 0);
    }
  }
  return pixels;
}

function encodePixels4(colors, opaqueGrid, palette) {
  const height = colors.length;
  const width = colors[0]?.length || 0;
  const packed = [];
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x += 2) {
      let high = 0;
      let low = 0;
      if (opaqueGrid[y][x]) {
        high = nearestPaletteIndex(colors[y][x], palette);
      }
      if (x + 1 < width && opaqueGrid[y][x + 1]) {
        low = nearestPaletteIndex(colors[y][x + 1], palette);
      }
      packed.push((high << 4) | (low & 0x0f));
    }
  }
  return packed;
}

async function loadImages(files) {
  const images = [];
  for (const file of files) {
    const url = URL.createObjectURL(file);
    const img = await new Promise((resolve, reject) => {
      const element = new Image();
      element.onload = () => resolve(element);
      element.onerror = reject;
      element.src = url;
    });
    images.push({ img, label: file.name.replace(/\.png$/i, "") });
    URL.revokeObjectURL(url);
  }
  return images;
}

function composeSheet(images, labels, layout) {
  if (images.length === 1) {
    return {
      canvas: imageToCanvas(images[0].img),
      regions: [{ label: labels[0], x: 0, y: 0, width: images[0].img.width, height: images[0].img.height }],
    };
  }

  if (layout === "horizontal") {
    const width = images.reduce((sum, entry) => sum + entry.img.width, 0);
    const height = Math.max(...images.map((entry) => entry.img.height));
    const canvas = document.createElement("canvas");
    canvas.width = width;
    canvas.height = height;
    const ctx = canvas.getContext("2d");
    const regions = [];
    let x = 0;
    images.forEach((entry, index) => {
      ctx.drawImage(entry.img, x, 0);
      regions.push({ label: labels[index], x, y: 0, width: entry.img.width, height: entry.img.height });
      x += entry.img.width;
    });
    return { canvas, regions };
  }

  const width = Math.max(...images.map((entry) => entry.img.width));
  const height = images.reduce((sum, entry) => sum + entry.img.height, 0);
  const canvas = document.createElement("canvas");
  canvas.width = width;
  canvas.height = height;
  const ctx = canvas.getContext("2d");
  const regions = [];
  let y = 0;
  images.forEach((entry, index) => {
    ctx.drawImage(entry.img, 0, y);
    regions.push({ label: labels[index], x: 0, y, width: entry.img.width, height: entry.img.height });
    y += entry.img.height;
  });
  return { canvas, regions };
}

function imageToCanvas(img) {
  const canvas = document.createElement("canvas");
  canvas.width = img.width;
  canvas.height = img.height;
  canvas.getContext("2d").drawImage(img, 0, 0);
  return canvas;
}

function rasterizeSheet(canvas, transparentRgb) {
  const ctx = canvas.getContext("2d");
  const { width, height } = canvas;
  const imageData = ctx.getImageData(0, 0, width, height);
  const colors = [];
  const opaqueGrid = [];
  for (let y = 0; y < height; y++) {
    const rowColors = [];
    const rowOpaque = [];
    for (let x = 0; x < width; x++) {
      const idx = (y * width + x) * 4;
      const r = imageData.data[idx];
      const g = imageData.data[idx + 1];
      const b = imageData.data[idx + 2];
      const a = imageData.data[idx + 3];
      rowOpaque.push(isOpaquePixel(r, g, b, a, transparentRgb));
      rowColors.push(rgbToRgb565(r, g, b));
    }
    colors.push(rowColors);
    opaqueGrid.push(rowOpaque);
  }
  return { colors, opaqueGrid };
}

function buildQuantizedPreview(canvas, colors, opaqueGrid, palette, bpp) {
  const preview = document.createElement("canvas");
  preview.width = canvas.width;
  preview.height = canvas.height;
  const ctx = preview.getContext("2d");
  const imageData = ctx.createImageData(canvas.width, canvas.height);
  for (let y = 0; y < canvas.height; y++) {
    for (let x = 0; x < canvas.width; x++) {
      const idx = (y * canvas.width + x) * 4;
      if (!opaqueGrid[y][x]) {
        imageData.data[idx + 3] = 0;
        continue;
      }
      let rgb565 = colors[y][x];
      if (bpp === 8 || bpp === 4) {
        rgb565 = palette[nearestPaletteIndex(rgb565, palette)];
      }
      const [r, g, b] = rgb565ToRgb(rgb565);
      imageData.data[idx] = r;
      imageData.data[idx + 1] = g;
      imageData.data[idx + 2] = b;
      imageData.data[idx + 3] = 255;
    }
  }
  ctx.putImageData(imageData, 0, 0);
  return preview;
}

function encodeSheet(canvas, regions, bpp, transparentRgb) {
  const { colors, opaqueGrid } = rasterizeSheet(canvas, transparentRgb);
  const opaqueValues = [];
  for (let y = 0; y < canvas.height; y++) {
    for (let x = 0; x < canvas.width; x++) {
      if (opaqueGrid[y][x]) opaqueValues.push(colors[y][x]);
    }
  }

  let palette = [];
  if (bpp === 4 || bpp === 8) {
    palette = buildPaletteFromColors(opaqueValues, bpp);
  }

  let pixels;
  if (bpp === 16) pixels = encodePixels16(colors, opaqueGrid);
  else if (bpp === 8) pixels = encodePixels8(colors, opaqueGrid, palette);
  else pixels = encodePixels4(colors, opaqueGrid, palette);

  const maskRows = encodeMaskRows(canvas.width, canvas.height, opaqueGrid);
  const preview = buildQuantizedPreview(canvas, colors, opaqueGrid, palette, bpp);
  return {
    bpp,
    sheetWidth: canvas.width,
    sheetHeight: canvas.height,
    palette,
    pixels,
    maskRows,
    regions,
    preview,
  };
}

function formatU16Array(values, perLine = 12) {
  const lines = [];
  for (let i = 0; i < values.length; i += perLine) {
    const chunk = values.slice(i, i + perLine);
    lines.push("  " + chunk.map((v) => `0x${v.toString(16).toUpperCase().padStart(4, "0")}`).join(", ") + ",");
  }
  return lines.join("\n");
}

function formatU8Array(values, perLine = 16) {
  const lines = [];
  for (let i = 0; i < values.length; i += perLine) {
    const chunk = values.slice(i, i + perLine);
    lines.push("  " + chunk.map((v) => `0x${v.toString(16).toUpperCase().padStart(2, "0")}`).join(", ") + ",");
  }
  return lines.join("\n");
}

function writeLegacyHeader(name, encoded) {
  const upper = name.toUpperCase();
  const guard = `_${upper}_H_`;
  let out = `#ifndef ${guard}\n#define ${guard}\n#include <Arduino.h>\n\n`;
  out += `#define ${upper}_WIDTH ${encoded.sheetWidth}\n`;
  out += `#define ${upper}_HEIGHT ${encoded.sheetHeight}\n\n`;
  out += `const unsigned short ${name}[${encoded.pixels.length}] PROGMEM={\n`;
  out += `${formatU16Array(encoded.pixels)}\n};\n\n`;
  out += `const uint8_t ${name}Mask[${encoded.maskRows.length}] PROGMEM={\n`;
  out += `${formatU8Array(encoded.maskRows)}\n};\n`;
  if (encoded.regions.length > 1 || (encoded.regions[0] && encoded.regions[0].label !== "full")) {
    out += `\nstatic const SpriteSheetRegion ${name}Regions[] PROGMEM = {\n`;
    encoded.regions.forEach((region) => {
      out += `  { ${region.x}, ${region.y}, ${region.width}, ${region.height} }, // ${region.label}\n`;
    });
    out += "};\n";
  }
  out += "\n#endif\n";
  return out;
}

function writeAssetHeader(name, encoded) {
  const upper = name.toUpperCase();
  const guard = `_${upper}_H_`;
  const pixelType = encoded.bpp === 16 ? "uint16_t" : "uint8_t";
  const bppEnum = `SPRITE_BPP_${encoded.bpp}`;
  let out = `#ifndef ${guard}\n#define ${guard}\n#include <Arduino.h>\n#include "SpriteAsset.h"\n\n`;
  out += `#define ${upper}_BPP ${encoded.bpp}\n`;
  out += `#define ${upper}_SHEET_WIDTH ${encoded.sheetWidth}\n`;
  out += `#define ${upper}_SHEET_HEIGHT ${encoded.sheetHeight}\n`;
  out += `#define ${upper}_PALETTE_COUNT ${encoded.palette.length}\n`;
  out += `#define ${upper}_BITMAP_COUNT ${encoded.regions.length}\n\n`;

  if (encoded.palette.length) {
    out += `static const uint16_t ${name}Palette[${encoded.palette.length}] PROGMEM = {\n`;
    out += `${formatU16Array(encoded.palette)}\n};\n\n`;
  }

  out += `static const ${pixelType} ${name}Pixels[${encoded.pixels.length}] PROGMEM = {\n`;
  out += encoded.bpp === 16 ? formatU16Array(encoded.pixels) : formatU8Array(encoded.pixels);
  out += `\n};\n\n`;
  out += `static const uint8_t ${name}Mask[${encoded.maskRows.length}] PROGMEM = {\n`;
  out += `${formatU8Array(encoded.maskRows)}\n};\n\n`;
  out += `static const SpriteBitmapRegion ${name}Bitmaps[${encoded.regions.length}] PROGMEM = {\n`;
  encoded.regions.forEach((region) => {
    out += `  { ${region.x}, ${region.y}, ${region.width}, ${region.height} }, // ${region.label}\n`;
  });
  out += "};\n\n";
  out += `static const SpriteAsset ${name} PROGMEM = {\n`;
  out += `  ${bppEnum},\n`;
  out += `  ${upper}_SHEET_WIDTH,\n`;
  out += `  ${upper}_SHEET_HEIGHT,\n`;
  out += `  ${upper}_PALETTE_COUNT,\n`;
  out += `  ${encoded.palette.length ? `${name}Palette` : "NULL"},\n`;
  out += `  ${name}Pixels,\n`;
  out += `  ${name}Mask,\n`;
  out += "};\n\n";
  if (encoded.bpp === 16) {
    out += `#define ${upper}_WIDTH ${upper}_SHEET_WIDTH\n`;
    out += `#define ${upper}_HEIGHT ${upper}_SHEET_HEIGHT\n`;
  }
  out += "\n#endif\n";
  return out;
}

function previewStats(encoded) {
  const pixelBytes = encoded.pixels.length * (encoded.bpp === 16 ? 2 : 1);
  const paletteBytes = encoded.palette.length * 2;
  const maskBytes = encoded.maskRows.length;
  const baseline = encoded.sheetWidth * encoded.sheetHeight * 2;
  const saved = baseline - (pixelBytes + paletteBytes);
  return (
    `Sheet: ${encoded.sheetWidth}x${encoded.sheetHeight}, ${encoded.bpp}-bit, palette=${encoded.palette.length} colors\n` +
    `  pixels: ${pixelBytes} B\n` +
    `  palette: ${paletteBytes} B\n` +
    `  mask: ${maskBytes} B\n` +
    `  vs RGB565 baseline (${baseline} B pixels): ${saved >= 0 ? "saved" : "cost"} ${Math.abs(saved)} B`
  );
}

function drawToCanvas(targetCanvas, sourceCanvas) {
  targetCanvas.width = sourceCanvas.width;
  targetCanvas.height = sourceCanvas.height;
  targetCanvas.getContext("2d").drawImage(sourceCanvas, 0, 0);
}

function parseTransparent(value) {
  if (!value.trim()) return null;
  const parts = value.split(",").map((part) => parseInt(part.trim(), 10));
  if (parts.length !== 3 || parts.some(Number.isNaN)) {
    throw new Error("Transparent RGB must look like 255,0,255");
  }
  return parts;
}

function parseLabels(input, count, fallbackLabels) {
  if (!input.trim()) return fallbackLabels;
  const labels = input.split(",").map((part) => part.trim()).filter(Boolean);
  if (labels.length !== count) {
    throw new Error(`Expected ${count} region labels, got ${labels.length}`);
  }
  return labels;
}

async function convert() {
  const files = document.getElementById("fileInput").files;
  if (!files.length) {
    alert("Choose at least one PNG file.");
    return;
  }

  const name = document.getElementById("symbolName").value.trim();
  if (!name.match(/^[A-Za-z_][A-Za-z0-9_]*$/)) {
    alert("Symbol name must be a valid C identifier, e.g. sprite_foo");
    return;
  }

  const bpp = parseInt(document.getElementById("bppSelect").value, 10);
  const layout = document.getElementById("layoutSelect").value;
  const legacy = document.getElementById("legacyMode").checked;
  if (legacy && bpp !== 16) {
    alert("Legacy mode requires 16-bit output.");
    return;
  }

  const transparentRgb = parseTransparent(document.getElementById("transparentRgb").value);
  const images = await loadImages([...files]);
  const fallbackLabels = images.map((entry) => entry.label);
  const labels = parseLabels(document.getElementById("regionLabels").value, images.length, fallbackLabels);
  const { canvas, regions } = composeSheet(images, labels, layout);
  const encoded = encodeSheet(canvas, regions, bpp, transparentRgb);

  drawToCanvas(document.getElementById("sourceCanvas"), canvas);
  drawToCanvas(document.getElementById("previewCanvas"), encoded.preview);
  document.getElementById("stats").textContent = previewStats(encoded);
  generatedHeader = legacy ? writeLegacyHeader(name, encoded) : writeAssetHeader(name, encoded);
  document.getElementById("headerOutput").value = generatedHeader;
  document.getElementById("downloadBtn").disabled = false;
}

function downloadHeader() {
  if (!generatedHeader) return;
  const name = document.getElementById("symbolName").value.trim() || "sprite";
  const blob = new Blob([generatedHeader], { type: "text/plain" });
  const url = URL.createObjectURL(blob);
  const link = document.createElement("a");
  link.href = url;
  link.download = `${name}.h`;
  link.click();
  URL.revokeObjectURL(url);
}

document.getElementById("convertBtn").addEventListener("click", () => {
  convert().catch((error) => alert(error.message));
});
document.getElementById("downloadBtn").addEventListener("click", downloadHeader);
document.getElementById("legacyMode").addEventListener("change", (event) => {
  if (event.target.checked) {
    document.getElementById("bppSelect").value = "16";
  }
});
